//===-- GNUstepObjCTypeEncodingParser.cpp ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GNUstepObjCTypeEncodingParser.h"

#include "Plugins/ExpressionParser/Clang/ClangUtil.h"
#include "Plugins/Language/ObjC/ObjCConstants.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"

#include "lldb/Symbol/CompilerType.h"
#include "lldb/Symbol/DeclVendor.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"

#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/StringExtras.h"

#include <limits>
#include <optional>
#include <vector>

using namespace lldb_private;

// Encodings ObjCConstants.h does not name. _C_COMPLEX is a libobjc2
// extension (objc/runtime.h); long double has no macro there at all, and is
// a real type on x86-64 ELF, so a method mentioning one would otherwise be
// dropped wholesale.
static constexpr char _C_COMPLEX = 'j';
static constexpr char _C_LNG_DBL = 'D';
static constexpr char _C_INT128 = 't';
static constexpr char _C_UINT128 = 'T';

// Method-argument qualifiers, from libobjc2's objc_skip_type_qualifiers
// (encoding2.c). `r` is _C_CONST and is deliberately absent: it is a real
// qualifier that changes the type, and BuildType handles it.
static constexpr llvm::StringLiteral g_argument_qualifiers = "nNoORVA";

// The types clang can give a bitfield in the GNU spelling: an integer type,
// or the `i`/`I` it uses for an enum.
static constexpr llvm::StringLiteral g_bitfield_types = "cCsSiIlLqQtTB";

// An encoding nested more deeply than this is not something a compiler
// emitted; refusing it keeps a malformed string from recursing without bound.
static constexpr unsigned g_max_depth = 64;

static char popChar(llvm::StringRef &str) {
  const char c = str.front();
  str = str.drop_front();
  return c;
}

/// Reads a decimal count, consuming the digits. Returns nullopt when the value
/// does not fit in 32 bits: the encoding comes from inferior memory, and a
/// wrapped extent would size an array wrongly rather than fail.
static std::optional<uint32_t> ReadNumber(llvm::StringRef &type) {
  uint64_t total = 0;
  while (!type.empty() && llvm::isDigit(type.front())) {
    total = 10 * total + (popChar(type) - '0');
    if (total > std::numeric_limits<uint32_t>::max())
      return std::nullopt;
  }
  return static_cast<uint32_t>(total);
}

/// Reads up to (not including) the closing quote, which stays in \p type.
static std::optional<std::string> ReadQuotedString(llvm::StringRef &type) {
  std::string buffer;
  while (!type.empty() && type.front() != '"')
    buffer.push_back(popChar(type));
  if (type.empty())
    return std::nullopt;
  return buffer;
}

/// Reads a struct or union tag, which runs up to the '='.
static std::string ReadAggregateName(llvm::StringRef &type) {
  std::string buffer;
  while (!type.empty() && type.front() != _C_STRUCT_E &&
         type.front() != _C_UNION_E && type.front() != '=')
    buffer.push_back(popChar(type));
  return buffer;
}

/// Returns the length of the bracketed group \p type starts with, up to and
/// including its matching closer, or nullopt if it is not closed. Quoted
/// names are skipped, since they may contain anything.
static std::optional<size_t> MeasureGroup(llvm::StringRef type) {
  unsigned depth = 0;
  for (size_t i = 0; i < type.size(); ++i) {
    switch (type[i]) {
    case '"':
      i = type.find('"', i + 1);
      if (i == llvm::StringRef::npos)
        return std::nullopt;
      break;
    case _C_STRUCT_B:
    case _C_UNION_B:
    case _C_ARY_B:
    case '<':
      ++depth;
      break;
    case _C_STRUCT_E:
    case _C_UNION_E:
    case _C_ARY_E:
    case '>':
      if (depth == 0)
        return std::nullopt;
      if (--depth == 0)
        return i + 1;
      break;
    default:
      if (depth == 0)
        return std::nullopt;
      break;
    }
  }
  return std::nullopt;
}

GNUstepObjCTypeEncodingParser::GNUstepObjCTypeEncodingParser(
    const llvm::Triple &triple, ObjCLanguageRuntime *runtime)
    : ObjCLanguageRuntime::EncodingToType(), m_runtime(runtime) {
  m_scratch_ast_ctx_sp = std::make_shared<TypeSystemClang>(
      "GNUstepObjCTypeEncodingParser ASTContext", triple);
}

GNUstepObjCTypeEncodingParser::StructElement
GNUstepObjCTypeEncodingParser::ReadStructElement(TypeSystemClang &ast_ctx,
                                                 llvm::StringRef &type,
                                                 bool for_expression) {
  StructElement retval;
  // Quoted field names are an Apple extension that clang's GNUstep output
  // does not produce, but consuming one costs nothing and keeps this usable
  // against an encoding that came from somewhere else.
  if (type.consume_front("\"")) {
    if (auto maybe_name = ReadQuotedString(type)) {
      retval.name = *maybe_name;
      type = type.drop_front(); // the closing quote
    } else {
      return retval;
    }
  }
  uint32_t bitfield_size = 0;
  retval.type = BuildType(ast_ctx, type, for_expression, &bitfield_size);
  retval.bitfield = bitfield_size;
  return retval;
}

clang::QualType GNUstepObjCTypeEncodingParser::BuildAggregate(
    TypeSystemClang &ast_ctx, llvm::StringRef &type, bool for_expression,
    char opener, char closer, uint32_t kind, bool is_pointee) {
  const std::optional<size_t> length = MeasureGroup(type);
  if (!length || type.front() != opener)
    return clang::QualType();
  const llvm::StringRef encoding = type.take_front(*length);

  // Key the cache on the exact text this record was built from, so that two
  // structurally different types sharing a tag - `{Foo=}` and `{Foo=ii}` -
  // do not collide, and so a repeated struct is laid out once. How it was
  // reached matters too: `@"Name"` members only resolve for expressions, and
  // a member-less pointee is a forward declaration rather than an empty
  // struct.
  const size_t equals = encoding.find('=');
  const bool memberless =
      equals == llvm::StringRef::npos || equals + 2 == encoding.size();
  const std::string key = (llvm::Twine(for_expression ? 'E' : 'N') +
                           (is_pointee && memberless ? "D:" : "R:") + encoding)
                              .str();
  llvm::StringMap<clang::QualType> &cache = m_record_cache[&ast_ctx];
  auto cached = cache.find(key);
  if (cached != cache.end()) {
    type = type.drop_front(*length);
    return cached->second;
  }

  llvm::StringRef body = encoding.drop_front().drop_back();
  const std::string name = ReadAggregateName(body);

  // Templated names are parsed for their side effect on `type` and then
  // discarded; there is no sensible clang record to build for one.
  if (name.find('<') != std::string::npos)
    return clang::QualType();

  // clang writes `{Foo}`, with no '=', for a structure it does not expand
  // and `{Foo=}` for one it has no members for. Neither describes Foo.
  const bool has_members = body.consume_front("=");
  std::vector<StructElement> elements;
  while (has_members && !body.empty()) {
    StructElement element = ReadStructElement(ast_ctx, body, for_expression);
    if (element.type.isNull())
      return clang::QualType();
    elements.push_back(std::move(element));
  }
  if (!body.empty())
    return clang::QualType();

  CompilerType record_type(ast_ctx.CreateRecordType(
      nullptr, OptionalClangModuleID(), name, kind, lldb::eLanguageTypeC));
  if (!record_type)
    return clang::QualType();

  // Behind a pointer, a record with no members is only a declaration: the
  // encoding says nothing about the real layout, and defining it as empty
  // would conflict with any encoding that does spell it out. By value, the
  // members are always spelled out, so none means the struct is empty.
  if (!elements.empty() || !is_pointee) {
    TypeSystemClang::StartTagDeclarationDefinition(record_type);
    unsigned count = 0;
    for (StructElement &element : elements) {
      if (element.name.empty())
        element.name = ("__unnamed_" + llvm::Twine(count)).str();
      TypeSystemClang::AddFieldToRecordType(record_type, element.name.c_str(),
                                            ast_ctx.GetType(element.type),
                                            element.bitfield);
      ++count;
    }
    TypeSystemClang::CompleteTagDeclarationDefinition(record_type);
  }

  type = type.drop_front(*length);
  clang::QualType qual_type = ClangUtil::GetQualType(record_type);
  cache[key] = qual_type;
  return qual_type;
}

clang::QualType GNUstepObjCTypeEncodingParser::BuildArray(
    TypeSystemClang &ast_ctx, llvm::StringRef &type, bool for_expression) {
  if (!type.consume_front(_C_ARY_B))
    return clang::QualType();

  const std::optional<uint32_t> size = ReadNumber(type);
  if (!size)
    return clang::QualType();
  clang::QualType element_type(BuildType(ast_ctx, type, for_expression));
  if (element_type.isNull())
    return clang::QualType();
  if (!type.consume_front(_C_ARY_E))
    return clang::QualType();

  CompilerType array_type(ast_ctx.CreateArrayType(
      CompilerType(ast_ctx.weak_from_this(), element_type.getAsOpaquePtr()),
      *size, /*is_vector=*/false));
  return ClangUtil::GetQualType(array_type);
}

clang::QualType GNUstepObjCTypeEncodingParser::BuildObjCObjectPointerType(
    TypeSystemClang &clang_ast_ctx, llvm::StringRef &type,
    bool for_expression) {
  if (!type.consume_front(_C_ID))
    return clang::QualType();

  clang::ASTContext &ast_ctx = clang_ast_ctx.getASTContext();

  // `@?` is a block. clang's ivar encodings are extended ones, which follow
  // it with the block's signature in angle brackets; nothing here needs the
  // signature, so skip it and treat the block as an opaque object pointer.
  if (type.consume_front(_C_UNDEF)) {
    if (type.starts_with("<")) {
      const std::optional<size_t> length = MeasureGroup(type);
      if (!length)
        return clang::QualType();
      type = type.drop_front(*length);
    }
    return ast_ctx.getObjCIdType();
  }

  std::string name;
  if (type.consume_front("\"")) {
    // Unlike Apple's runtime, clang's GNUstep output never emits quoted field
    // names inside a struct, so a quoted string after '@' is unambiguously a
    // class name. Apple peeks at the next character to tell the two apart;
    // doing that here would mis-parse `{Foo=@"NSString"i}`, whose `i` is the
    // next field rather than a hint that "NSString" was a field name.
    if (auto maybe_name = ReadQuotedString(type)) {
      name = *maybe_name;
      type = type.drop_front(); // the closing quote
    } else {
      return clang::QualType();
    }
  }

  if (!for_expression || name.empty())
    return ast_ctx.getObjCIdType();

  // Protocol qualifiers carry no type information here: `<Proto>` alone is
  // just `id`, and `NSFoo<Proto>` is an NSFoo.
  const size_t less_than_pos = name.find('<');
  if (less_than_pos == 0)
    return ast_ctx.getObjCIdType();
  if (less_than_pos != std::string::npos)
    name.erase(less_than_pos);

  DeclVendor *decl_vendor = m_runtime ? m_runtime->GetDeclVendor() : nullptr;
  if (!decl_vendor)
    return ast_ctx.getObjCIdType();

  auto types = decl_vendor->FindTypes(ConstString(name), /*max_matches=*/1);
  if (types.empty()) {
    // Naming a class the runtime has not realized is expected, not a bug: an
    // encoding outlives the class it mentions.
    LLDB_LOG(GetLog(LLDBLog::Types),
             "GNUstep type encoding names an unknown class: {0}", name);
    return ast_ctx.getObjCIdType();
  }
  return ClangUtil::GetQualType(types.front().GetPointerType());
}

clang::QualType GNUstepObjCTypeEncodingParser::BuildType(
    TypeSystemClang &clang_ast_ctx, llvm::StringRef &type, bool for_expression,
    uint32_t *bitfield_bit_size) {
  if (type.empty())
    return clang::QualType();

  // Every nesting construct - pointers, arrays, aggregates, const - recurses
  // here, and the encoding is inferior data. Bound the depth so a malformed
  // one cannot exhaust the stack.
  if (m_depth >= g_max_depth)
    return clang::QualType();
  ++m_depth;
  llvm::scope_exit depth_guard([this] { --m_depth; });

  // Skip any argument qualifiers. clang emits these for declarations such as
  // `- (oneway void)release`, and leaving them in place would drop the whole
  // method.
  while (!type.empty() && g_argument_qualifiers.contains(type.front()))
    type = type.drop_front();
  if (type.empty())
    return clang::QualType();

  clang::ASTContext &ast_ctx = clang_ast_ctx.getASTContext();

  switch (type.front()) {
  default:
    break;
  case _C_STRUCT_B:
    return BuildAggregate(clang_ast_ctx, type, for_expression, _C_STRUCT_B,
                          _C_STRUCT_E,
                          llvm::to_underlying(clang::TagTypeKind::Struct));
  case _C_UNION_B:
    return BuildAggregate(clang_ast_ctx, type, for_expression, _C_UNION_B,
                          _C_UNION_E,
                          llvm::to_underlying(clang::TagTypeKind::Union));
  case _C_ARY_B:
    return BuildArray(clang_ast_ctx, type, for_expression);
  case _C_ID:
    return BuildObjCObjectPointerType(clang_ast_ctx, type, for_expression);
  }

  // Save a copy so an unrecognized encoding can be left untouched for the
  // caller to notice.
  llvm::StringRef backup = type;

  switch (popChar(type)) {
  default:
    type = backup;
    return clang::QualType();
  case _C_CHR:
    return ast_ctx.CharTy;
  case _C_INT:
    return ast_ctx.IntTy;
  case _C_SHT:
    return ast_ctx.ShortTy;
  case _C_LNG:
    // clang only emits 'l' where `long` is 32 bits and 'q' otherwise
    // (ASTContext::getObjCEncodingForPrimitiveType), so this is right on
    // LP64, LLP64 and ILP32 alike.
    return ast_ctx.getIntTypeForBitwidth(32, /*Signed=*/true);
  case _C_LNG_LNG:
    return ast_ctx.LongLongTy;
  case _C_UCHR:
    return ast_ctx.UnsignedCharTy;
  case _C_UINT:
    return ast_ctx.UnsignedIntTy;
  case _C_USHT:
    return ast_ctx.UnsignedShortTy;
  case _C_ULNG:
    return ast_ctx.getIntTypeForBitwidth(32, /*Signed=*/false);
  case _C_ULNG_LNG:
    return ast_ctx.UnsignedLongLongTy;
  case _C_INT128:
    return ast_ctx.Int128Ty;
  case _C_UINT128:
    return ast_ctx.UnsignedInt128Ty;
  case _C_FLT:
    return ast_ctx.FloatTy;
  case _C_DBL:
    return ast_ctx.DoubleTy;
  case _C_LNG_DBL:
    return ast_ctx.LongDoubleTy;
  case _C_BOOL:
    return ast_ctx.BoolTy;
  case _C_VOID:
    return ast_ctx.VoidTy;
  case _C_CHARPTR:
  case _C_ATOM:
    // _C_ATOM is a char* whose contents are interned.
    return ast_ctx.getPointerType(ast_ctx.CharTy);
  case _C_CLASS:
    return ast_ctx.getObjCClassType();
  case _C_SEL:
    return ast_ctx.getObjCSelType();
  case _C_COMPLEX: {
    clang::QualType element_type =
        BuildType(clang_ast_ctx, type, for_expression);
    if (element_type.isNull())
      return clang::QualType();
    return ast_ctx.getComplexType(element_type);
  }
  case _C_VECTOR:
    // The published grammar gives no way to recover a vector's width, so
    // there is nothing to build. Consume it and report failure rather than
    // silently producing the element type.
    return clang::QualType();
  case _C_BFLD: {
    // The GNU runtimes spell a bitfield `b<offset><type><width>`, giving its
    // bit offset and underlying type (ASTContext::EncodeBitField, libobjc2's
    // encoding2.c). Clang lays the record out again itself, so the offset is
    // not needed, but the type decides the field's signedness. NeXT's
    // `b<width>` is accepted where it cannot be mistaken for the GNU form;
    // libobjc2 only ever holds the GNU one.
    if (!bitfield_bit_size)
      return clang::QualType();
    const std::optional<uint32_t> first = ReadNumber(type);
    if (!first)
      return clang::QualType();
    clang::QualType field_type = ast_ctx.UnsignedIntTy;
    uint32_t width = *first;
    if (!type.empty() && g_bitfield_types.contains(type.front())) {
      field_type = BuildType(clang_ast_ctx, type, for_expression);
      const std::optional<uint32_t> gnu_width = ReadNumber(type);
      if (field_type.isNull() || !gnu_width)
        return clang::QualType();
      width = *gnu_width;
    }
    // Clang lays the record out again from its fields, and an unnamed `:0`,
    // which only pads to the next unit, cannot be handed to it: the members
    // after one would land at the wrong offsets, so refuse the record. Refuse
    // a width wider than the type too; only C++ allows one, as padding.
    if (width == 0 || width > ast_ctx.getTypeSize(field_type))
      return clang::QualType();
    *bitfield_bit_size = width;
    return field_type;
  }
  case _C_CONST: {
    clang::QualType target_type =
        BuildType(clang_ast_ctx, type, for_expression);
    if (target_type.isNull())
      return clang::QualType();
    if (target_type == ast_ctx.UnknownAnyTy)
      return ast_ctx.UnknownAnyTy;
    return ast_ctx.getConstType(target_type);
  }
  case _C_PTR: {
    if (!for_expression && type.consume_front(_C_UNDEF)) {
      // A pointer to something unrepresentable is more useful as void* than
      // as nothing at all, when the expression parser is not involved.
      return ast_ctx.VoidPtrTy;
    }
    clang::QualType target_type;
    if (type.starts_with(_C_STRUCT_B))
      target_type = BuildAggregate(
          clang_ast_ctx, type, for_expression, _C_STRUCT_B, _C_STRUCT_E,
          llvm::to_underlying(clang::TagTypeKind::Struct), /*is_pointee=*/true);
    else if (type.starts_with(_C_UNION_B))
      target_type = BuildAggregate(
          clang_ast_ctx, type, for_expression, _C_UNION_B, _C_UNION_E,
          llvm::to_underlying(clang::TagTypeKind::Union), /*is_pointee=*/true);
    else
      target_type = BuildType(clang_ast_ctx, type, for_expression);
    if (target_type.isNull())
      return clang::QualType();
    if (target_type == ast_ctx.UnknownAnyTy)
      return ast_ctx.UnknownAnyTy;
    return ast_ctx.getPointerType(target_type);
  }
  case _C_UNDEF:
    return for_expression ? ast_ctx.UnknownAnyTy : clang::QualType();
  }
}

CompilerType GNUstepObjCTypeEncodingParser::RealizeType(
    TypeSystemClang &ast_ctx, const char *name, bool for_expression) {
  if (!name || !name[0])
    return CompilerType();
  llvm::StringRef lexer(name);
  return ast_ctx.GetType(BuildType(ast_ctx, lexer, for_expression));
}

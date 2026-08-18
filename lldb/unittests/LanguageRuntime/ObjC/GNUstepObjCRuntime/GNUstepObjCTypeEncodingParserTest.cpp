//===-- GNUstepObjCTypeEncodingParserTest.cpp -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCTypeEncodingParser.h"

#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "TestingSupport/SubsystemRAII.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Symbol/CompilerType.h"

#include "gtest/gtest.h"

#include <memory>
#include <string>

using namespace lldb_private;

namespace {

/// The parser needs no target state, so each data model is just a triple.
struct DataModel {
  const char *triple;
  /// Width of `long` in bits, which is what differs between LP64 and LLP64.
  unsigned long_bits;
  /// Width of a pointer in bits, which is what drives aggregate layout.
  unsigned pointer_bits;
};

class GNUstepTypeEncodingParserTest
    : public ::testing::TestWithParam<DataModel> {
public:
  void SetUp() override {
    m_triple = llvm::Triple(GetParam().triple);
    m_ast_sp = std::make_shared<TypeSystemClang>("test ASTContext", m_triple);
    // No runtime: `@"Name"` has no DeclVendor to resolve against and must
    // degrade to `id` rather than crash.
    m_parser = std::make_unique<GNUstepObjCTypeEncodingParser>(m_triple);
  }

  void TearDown() override {
    m_parser.reset();
    m_ast_sp.reset();
  }

  /// Realizes \p encoding and returns its type name, or "" if it failed.
  std::string Realize(llvm::StringRef encoding, bool for_expression = false) {
    CompilerType type = m_parser->RealizeType(*m_ast_sp, encoding.str().c_str(),
                                              for_expression);
    if (!type)
      return "";
    return type.GetTypeName().GetString();
  }

  std::optional<uint64_t> SizeOf(llvm::StringRef encoding) {
    CompilerType type =
        m_parser->RealizeType(*m_ast_sp, encoding.str().c_str(), false);
    if (!type)
      return std::nullopt;
    llvm::Expected<uint64_t> size = type.GetByteSize(nullptr);
    if (!size) {
      llvm::consumeError(size.takeError());
      return std::nullopt;
    }
    return *size;
  }

  SubsystemRAII<FileSystem, HostInfo> subsystems;
  llvm::Triple m_triple;
  std::shared_ptr<TypeSystemClang> m_ast_sp;
  std::unique_ptr<GNUstepObjCTypeEncodingParser> m_parser;
};

TEST_P(GNUstepTypeEncodingParserTest, Scalars) {
  // `c` is a signed char: plain char where that is signed, but not on a
  // target whose plain char is unsigned, which clang encodes as `C`.
  const bool char_is_signed =
      m_ast_sp->getASTContext().CharTy->isSignedIntegerType();
  EXPECT_EQ(Realize("c"), char_is_signed ? "char" : "signed char");
  EXPECT_EQ(Realize("i"), "int");
  EXPECT_EQ(Realize("s"), "short");
  EXPECT_EQ(Realize("q"), "long long");
  EXPECT_EQ(Realize("C"), "unsigned char");
  EXPECT_EQ(Realize("I"), "unsigned int");
  EXPECT_EQ(Realize("S"), "unsigned short");
  EXPECT_EQ(Realize("Q"), "unsigned long long");
  EXPECT_EQ(Realize("f"), "float");
  EXPECT_EQ(Realize("d"), "double");
  EXPECT_EQ(Realize("B"), "bool");
  EXPECT_EQ(Realize("v"), "void");
  EXPECT_EQ(Realize("*"), "char *");
}

// clang emits 'l' only where `long` is 32 bits and 'q' otherwise
// (ASTContext::getObjCEncodingForPrimitiveType), so 'l' always means 32 bits
// no matter which data model the target uses.
TEST_P(GNUstepTypeEncodingParserTest, LongIsAlways32Bits) {
  EXPECT_EQ(SizeOf("l"), std::optional<uint64_t>(4u));
  EXPECT_EQ(SizeOf("L"), std::optional<uint64_t>(4u));
  EXPECT_EQ(SizeOf("q"), std::optional<uint64_t>(8u));
  EXPECT_EQ(SizeOf("Q"), std::optional<uint64_t>(8u));
}

TEST_P(GNUstepTypeEncodingParserTest, LongDouble) {
  // Unhandled by Apple's parser; an encoding mentioning one would otherwise
  // fail and take the whole method with it.
  EXPECT_EQ(Realize("D"), "long double");
}

TEST_P(GNUstepTypeEncodingParserTest, ObjCBuiltins) {
  EXPECT_EQ(Realize("@"), "id");
  EXPECT_EQ(Realize("#"), "Class");
  EXPECT_EQ(Realize(":"), "SEL");
  // A block. The extended form carries a signature nothing here needs.
  EXPECT_EQ(Realize("@?"), "id");
  EXPECT_EQ(Realize("@?<v@?>"), "id");
}

// clang's ivar encodings are extended ones, so a block member is followed by
// its signature. That must be skipped, not read as the next member.
TEST_P(GNUstepTypeEncodingParserTest, BlockSignatureInsideStruct) {
  EXPECT_EQ(SizeOf("{Foo=@?<v@?i>i}"),
            std::optional<uint64_t>(GetParam().pointer_bits == 32 ? 8u : 16u));
  EXPECT_EQ(Realize("{Foo=@?<v@?"), "");
}

TEST_P(GNUstepTypeEncodingParserTest, Int128) {
  EXPECT_EQ(Realize("t"), "__int128");
  EXPECT_EQ(Realize("T"), "unsigned __int128");
  EXPECT_EQ(SizeOf("{Wide=t}"), std::optional<uint64_t>(16u));
}

// Without a DeclVendor the class name cannot be resolved. That must degrade
// to `id`, not assert - libobjc2 encodings routinely name classes that are
// not realized.
TEST_P(GNUstepTypeEncodingParserTest, UnknownClassNameDegradesToId) {
  EXPECT_EQ(Realize("@\"NSString\""), "id");
  EXPECT_EQ(Realize("@\"NSString\"", /*for_expression=*/true), "id");
  EXPECT_EQ(Realize("@\"<NSCopying>\"", /*for_expression=*/true), "id");
  EXPECT_EQ(Realize("@\"NSFoo<NSCopying>\"", /*for_expression=*/true), "id");
}

TEST_P(GNUstepTypeEncodingParserTest, Pointers) {
  EXPECT_EQ(Realize("^i"), "int *");
  EXPECT_EQ(Realize("^^v"), "void **");
  EXPECT_EQ(Realize("^?"), "void *");
  EXPECT_EQ(Realize("ri"), "const int");
  EXPECT_EQ(Realize("^ri"), "const int *");
}

TEST_P(GNUstepTypeEncodingParserTest, Arrays) {
  EXPECT_EQ(Realize("[10i]"), "int[10]");
  EXPECT_EQ(SizeOf("[10i]"), std::optional<uint64_t>(40u));
  EXPECT_EQ(SizeOf("[3[4f]]"), std::optional<uint64_t>(48u));
}

TEST_P(GNUstepTypeEncodingParserTest, ArrayExtentDoesNotWrap) {
  // The encoding is inferior data, so an extent that does not fit must fail
  // rather than wrap: 10000000000 truncated to 32 bits is 1410065408, and an
  // array of that many ints would be accepted as if it were the real size.
  EXPECT_EQ(Realize("[10000000000i]"), "");
  EXPECT_EQ(Realize("[4294967296i]"), "");
  // One below the limit still parses, so the bound is not off by one.
  EXPECT_EQ(Realize("[4294967295i]"), "int[4294967295]");
}

// Each extent fitting is not enough: clang asserts if the array's total size
// in bits overflows, and on an array of an incomplete type.
TEST_P(GNUstepTypeEncodingParserTest, RejectsArraysClangCannotLayOut) {
  EXPECT_EQ(Realize("[4294967295[4294967295i]]"), "");
  EXPECT_EQ(Realize("[4294967295{S=[4294967295i]}]"), "");
  EXPECT_EQ(Realize("[4v]"), "");
  EXPECT_EQ(Realize("{S=[4v]}"), "");
  // 2^40 bytes is the limit.
  EXPECT_EQ(Realize("[4294967296C]"), "");
  EXPECT_EQ(Realize("[1024[1073741824C]]"), "unsigned char[1024][1073741824]");
  EXPECT_EQ(Realize("[1025[1073741824C]]"), "");
}

// Nothing here goes through Sema, so anything clang cannot lay out has to be
// refused before it is asked to: for expressions `?` becomes the UnknownAny
// placeholder, whose size clang cannot compute, and _Complex and _Atomic take
// only what C allows.
TEST_P(GNUstepTypeEncodingParserTest, RejectsWhatClangCannotLayOut) {
  EXPECT_EQ(Realize("[1?]", /*for_expression=*/true), "");
  EXPECT_EQ(Realize("[1^?]", /*for_expression=*/true), "");
  EXPECT_EQ(Realize("{S=?}", /*for_expression=*/true), "");
  EXPECT_EQ(Realize("{S=v}"), "");
  EXPECT_EQ(Realize("[1j?]", /*for_expression=*/true), "");
  EXPECT_EQ(Realize("jji"), "");
  EXPECT_EQ(Realize("j{S=i}"), "");
  EXPECT_EQ(Realize("j^i"), "");
  EXPECT_EQ(Realize("A[2i]"), "");
  EXPECT_EQ(Realize("AAi"), "");
  EXPECT_EQ(Realize("Av"), "");
  // What C does allow still realizes.
  EXPECT_EQ(Realize("ji"), "_Complex int");
  EXPECT_EQ(Realize("jd"), "_Complex double");
  EXPECT_EQ(Realize("A{S=ii}"), "_Atomic(S)");
  EXPECT_EQ(Realize("A^i"), "_Atomic(int *)");
}

TEST_P(GNUstepTypeEncodingParserTest, Structs) {
  EXPECT_EQ(Realize("{CGPoint=dd}"), "CGPoint");
  EXPECT_EQ(SizeOf("{CGPoint=dd}"), std::optional<uint64_t>(16u));
  // By value, no members means an empty struct.
  EXPECT_EQ(Realize("{CGImage=}"), "CGImage");
  EXPECT_EQ(Realize("^{CGImage=}"), "CGImage *");
  EXPECT_EQ(Realize("(U=ic)"), "U");
}

// Behind a pointer, clang writes a struct it does not expand as `{Foo}` and
// one it has no members for as `{Foo=}`. Neither says what Foo holds, so
// defining Foo as empty would contradict an encoding that does.
TEST_P(GNUstepTypeEncodingParserTest, MemberlessPointeeIsADeclaration) {
  for (const char *encoding : {"^{Foo}", "^{Foo=}", "^(U)"}) {
    CompilerType type = m_parser->RealizeType(*m_ast_sp, encoding, false);
    ASSERT_TRUE(type) << encoding;
    EXPECT_FALSE(type.GetPointeeType().GetCompleteType()) << encoding;
  }
  CompilerType defined = m_parser->RealizeType(*m_ast_sp, "^{Foo=ii}", false);
  ASSERT_TRUE(defined);
  EXPECT_TRUE(defined.GetPointeeType().GetCompleteType());
}

// This is the encoding shape libobjc2 actually emits for a struct ivar,
// verified against the shipped library: members are expanded positionally
// with no field names.
TEST_P(GNUstepTypeEncodingParserTest, StructWithoutFieldNames) {
  EXPECT_EQ(Realize("{_NSRange=QQ}"), "_NSRange");
  EXPECT_EQ(SizeOf("{_NSRange=QQ}"), std::optional<uint64_t>(16u));
}

// A quoted string after '@' is a class name, never a field name: unlike
// Apple's runtime, clang's GNUstep output does not emit quoted field names,
// so peeking to disambiguate would mis-parse the `i` here as evidence that
// "NSString" was a field name.
TEST_P(GNUstepTypeEncodingParserTest, ClassNameInStructIsNotAFieldName) {
  // An object pointer followed by an int: the size follows the pointer
  // width, so LLP64 and LP64 agree here and only ILP32 differs.
  EXPECT_EQ(SizeOf("{Foo=@\"NSString\"i}"),
            std::optional<uint64_t>(GetParam().pointer_bits == 32 ? 8u : 16u));
}

TEST_P(GNUstepTypeEncodingParserTest, Bitfields) {
  // A bitfield outside a struct has nowhere to report its width.
  EXPECT_EQ(Realize("b3"), "");
  EXPECT_EQ(Realize("b0I3"), "");
  // NeXT's spelling carries no type, so the base type is unsigned int and
  // eight bits of them still occupy one int rather than one byte.
  EXPECT_EQ(SizeOf("{Flags=b1b1b6}"), std::optional<uint64_t>(4u));
}

// What clang emits for the GNU runtimes, and so what libobjc2 holds:
// `b<offset><type><width>`. The first is gnustep-base's GSString flags
// struct (`wide:1; owned:1; unused:2; hash:28`, Source/GSPrivate.h).
TEST_P(GNUstepTypeEncodingParserTest, GNUBitfields) {
  EXPECT_EQ(SizeOf("{?=b0I1b1I1b2I2b4I28}"), std::optional<uint64_t>(4u));
  EXPECT_EQ(SizeOf("{?={?=b0I1b1I31}}"), std::optional<uint64_t>(4u));
  // The type is carried, so a char bitfield packs into a byte...
  EXPECT_EQ(SizeOf("{Small=b0C3b3C5}"), std::optional<uint64_t>(1u));

  // ...and signedness survives.
  CompilerType type =
      m_parser->RealizeType(*m_ast_sp, "{Mixed=b0i3b3I5}", false);
  ASSERT_TRUE(type);
  ASSERT_EQ(type.GetNumFields(), 2u);
  std::string name;
  uint64_t bit_offset = 0;
  uint32_t bit_size = 0;
  bool is_bitfield = false;
  CompilerType first =
      type.GetFieldAtIndex(0, name, &bit_offset, &bit_size, &is_bitfield);
  EXPECT_EQ(first.GetTypeName().GetStringRef(), "int");
  EXPECT_TRUE(is_bitfield);
  EXPECT_EQ(bit_size, 3u);
  is_bitfield = false;
  CompilerType second =
      type.GetFieldAtIndex(1, name, &bit_offset, &bit_size, &is_bitfield);
  EXPECT_EQ(second.GetTypeName().GetStringRef(), "unsigned int");
  EXPECT_TRUE(is_bitfield);
  EXPECT_EQ(bit_size, 5u);
  EXPECT_EQ(bit_offset, 3u);

  // `int :0` only pads, which clang cannot be told about when it lays the
  // record out again, so `b` would land at bit 3 rather than bit 32.
  EXPECT_EQ(Realize("{Pad=b0i3b32i0b32i5}"), "");

  // A width is at most that of its type.
  EXPECT_EQ(SizeOf("{Wide=b0q40}"), std::optional<uint64_t>(8u));
  EXPECT_EQ(Realize("{Bad=b0C9}"), "");
  EXPECT_EQ(Realize("{Bad=b0i33}"), "");
  EXPECT_EQ(Realize("{Bad=b65}"), "");

  // A type that cannot hold bits is not something a compiler emits.
  EXPECT_EQ(Realize("{Bad=b0d3}"), "");

  // The offset is where the bitfield really was. A packed record puts this
  // one at bit 8, which a record clang lays out from the fields alone cannot
  // reproduce, so it is refused rather than described wrongly.
  EXPECT_EQ(Realize("{Packed=cb8I32}"), "");
  EXPECT_EQ(SizeOf("{Ordinary=cb32I32}"), std::optional<uint64_t>(8u));
}

// clang emits these for declarations such as `- (oneway void)release`.
// Leaving them unhandled would drop the whole method.
TEST_P(GNUstepTypeEncodingParserTest, SkipsArgumentQualifiers) {
  EXPECT_EQ(Realize("Vv"), "void");
  EXPECT_EQ(Realize("ni"), "int");
  EXPECT_EQ(Realize("Ni"), "int");
  EXPECT_EQ(Realize("oi"), "int");
  EXPECT_EQ(Realize("Oi"), "int");
  EXPECT_EQ(Realize("Ri"), "int");
  // 'r' is const and 'A' is _Atomic, not argument qualifiers, and must keep
  // their meaning.
  EXPECT_NE(Realize("ri"), "int");
  EXPECT_EQ(Realize("Ai"), "_Atomic(int)");
}

// An _Atomic member can be more strictly aligned than the type it wraps - on
// i386 an _Atomic(long long) is 8-aligned where a long long is 4-aligned - so
// dropping the `A` would lay the struct out wrongly.
TEST_P(GNUstepTypeEncodingParserTest, AtomicKeepsItsAlignment) {
  EXPECT_EQ(SizeOf("{S=cAqi}"), std::optional<uint64_t>(24u));
}

// The same struct appearing twice must yield the same type rather than two
// separately laid out decls sharing a name.
TEST_P(GNUstepTypeEncodingParserTest, RecordsAreCached) {
  CompilerType first = m_parser->RealizeType(*m_ast_sp, "{CGPoint=dd}", false);
  CompilerType second = m_parser->RealizeType(*m_ast_sp, "{CGPoint=dd}", false);
  ASSERT_TRUE(first);
  EXPECT_EQ(first.GetOpaqueQualType(), second.GetOpaqueQualType());
}

// Two different types can legitimately share a tag - an opaque declaration
// and its definition - so the cache must not collapse them.
TEST_P(GNUstepTypeEncodingParserTest, SameTagDifferentBodyIsNotCached) {
  CompilerType opaque = m_parser->RealizeType(*m_ast_sp, "{Foo=}", false);
  CompilerType defined = m_parser->RealizeType(*m_ast_sp, "{Foo=ii}", false);
  ASSERT_TRUE(opaque);
  ASSERT_TRUE(defined);
  EXPECT_NE(opaque.GetOpaqueQualType(), defined.GetOpaqueQualType());
}

// Nothing here may crash, hang, or recurse without bound. The encoding comes
// from inferior memory, so it cannot be assumed well-formed.
TEST_P(GNUstepTypeEncodingParserTest, MalformedInputIsRejected) {
  for (llvm::StringRef bad : {"",
                              "{",
                              "{Foo=",
                              "{Foo=ii",
                              "[10",
                              "[10i",
                              "^",
                              "b",
                              "@\"",
                              "(U=ic",
                              "{Foo=@\"NSString",
                              "]",
                              ")",
                              "}",
                              "=",
                              "{S=b",
                              "{S=b0",
                              "{S=b0I",
                              "@?<",
                              "^{",
                              "{Foo=}}",
                              "{Foo={Bar=i}"}) {
    // No assertion on the result beyond "it returned"; the point is that it
    // terminates and does not trip an assertion inside clang.
    Realize(bad);
  }
}

TEST_P(GNUstepTypeEncodingParserTest, DeeplyNestedInputTerminates) {
  EXPECT_EQ(Realize(std::string(4096, '^') + "v"), "");
  EXPECT_EQ(Realize(std::string(1000, '{')), "");
}

// Truncating a valid encoding at every length is the cheapest way to reach
// the partial states a hand-written parser gets wrong.
TEST_P(GNUstepTypeEncodingParserTest, EveryTruncationTerminates) {
  const std::string full =
      "{Outer=@\"NSString\"^{Inner=ii}[4f]b3qb0I2@?<v@?>{_NSRange=QQ}}";
  for (size_t n = 1; n <= full.size(); ++n)
    Realize(llvm::StringRef(full).take_front(n));
}

INSTANTIATE_TEST_SUITE_P(
    DataModels, GNUstepTypeEncodingParserTest,
    ::testing::Values(DataModel{"x86_64-pc-linux", 64, 64},
                      // Windows is LLP64: pointers are 64 bits, long is 32.
                      DataModel{"x86_64-pc-windows-msvc", 32, 64},
                      DataModel{"i386-pc-linux", 32, 32},
                      // Plain char is unsigned here.
                      DataModel{"aarch64-unknown-linux-gnu", 64, 64}),
    [](const ::testing::TestParamInfo<DataModel> &info) {
      std::string name = info.param.triple;
      for (char &c : name)
        if (!std::isalnum(static_cast<unsigned char>(c)))
          c = '_';
      return name;
    });

} // namespace

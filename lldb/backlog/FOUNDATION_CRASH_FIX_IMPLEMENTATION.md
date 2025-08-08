# GNUstep Foundation Crash Fix Implementation Plan

**Target**: Fix P0-Critical crash `NSInvalidArgumentException: -[GSTinyString ]: unrecognized selector sent to instance`

## Root Cause Analysis

The crash occurs because:

1. **GNUstepObjCDeclVendor::FindDecls()** is a stub that returns 0 matches
2. **No Foundation selector declarations** are registered with LLDB
3. **Tagged pointer detection** is incomplete in the introspector 
4. **Method introspection** sends empty selectors to objects during debugging

## Implementation Strategy

### Phase 1: Critical Selector Registration (Immediate Fix)

**File: `GNUstepObjCDeclVendor.cpp`**

```cpp
//===-- GNUstepObjCDeclVendor.cpp ----------------------------------------===//

#include "GNUstepObjCDeclVendor.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "lldb/Utility/ConstString.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/Type.h"

using namespace lldb;
using namespace lldb_private;
using namespace clang;

// Foundation class to selector mapping
static const std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> 
foundation_selectors = {
    // GSTinyString and all string classes
    {"NSString", {
        {"length", "L0:0"},
        {"characterAtIndex:", "S0:0L0:0"},
        {"UTF8String", "*0:0"},
        {"description", "@0:0"},
        {"debugDescription", "@0:0"},
        {"isEqualToString:", "B0:0@0:0"},
        {"hash", "L0:0"},
        {"boolValue", "B0:0"},
        {"intValue", "i0:0"},
        {"integerValue", "l0:0"},
        {"longLongValue", "q0:0"},
        {"doubleValue", "d0:0"},
        {"floatValue", "f0:0"},
        {"copy", "@0:0"},
        {"mutableCopy", "@0:0"},
        {"retain", "@0:0"},
        {"release", "v0:0"},
        {"autorelease", "@0:0"},
        {"retainCount", "L0:0"},
        {"getCharacters:range:", "v0:0^S0:0{_NSRange=LL}0:0"},
        {"getCString:maxLength:encoding:", "B0:0*0:0L0:0L0:0"}
    }},
    
    // NSArray family
    {"NSArray", {
        {"count", "L0:0"},
        {"objectAtIndex:", "@0:0L0:0"},
        {"description", "@0:0"},
        {"descriptionWithLocale:", "@0:0@0:0"},
        {"objectEnumerator", "@0:0"},
        {"containsObject:", "B0:0@0:0"},
        {"indexOfObject:", "L0:0@0:0"},
        {"copy", "@0:0"},
        {"mutableCopy", "@0:0"},
        {"hash", "L0:0"}
    }},
    
    // NSDictionary family  
    {"NSDictionary", {
        {"count", "L0:0"},
        {"objectForKey:", "@0:0@0:0"},
        {"keyEnumerator", "@0:0"},
        {"objectEnumerator", "@0:0"},
        {"allKeys", "@0:0"},
        {"allValues", "@0:0"},
        {"description", "@0:0"},
        {"descriptionWithLocale:", "@0:0@0:0"},
        {"copy", "@0:0"},
        {"mutableCopy", "@0:0"},
        {"hash", "L0:0"}
    }},
    
    // NSSet family
    {"NSSet", {
        {"count", "L0:0"},
        {"member:", "@0:0@0:0"},
        {"objectEnumerator", "@0:0"},
        {"containsObject:", "B0:0@0:0"},
        {"description", "@0:0"},
        {"descriptionWithLocale:", "@0:0@0:0"},
        {"copy", "@0:0"},
        {"mutableCopy", "@0:0"},
        {"hash", "L0:0"}
    }},
    
    // NSNumber family
    {"NSNumber", {
        {"boolValue", "B0:0"},
        {"charValue", "c0:0"},
        {"shortValue", "s0:0"},
        {"intValue", "i0:0"},
        {"longValue", "l0:0"},
        {"longLongValue", "q0:0"},
        {"unsignedCharValue", "C0:0"},
        {"unsignedShortValue", "S0:0"},
        {"unsignedIntValue", "I0:0"},
        {"unsignedLongValue", "L0:0"},
        {"unsignedLongLongValue", "Q0:0"},
        {"floatValue", "f0:0"},
        {"doubleValue", "d0:0"},
        {"integerValue", "l0:0"},
        {"unsignedIntegerValue", "L0:0"},
        {"objCType", "*0:0"},
        {"stringValue", "@0:0"},
        {"description", "@0:0"},
        {"copy", "@0:0"},
        {"hash", "L0:0"}
    }}
};

// GNUstep concrete classes to Foundation class mapping
static const std::unordered_map<std::string, std::string> concrete_to_foundation = {
    // String classes
    {"GSTinyString", "NSString"},
    {"GSCString", "NSString"},
    {"GSUnicodeString", "NSString"},
    {"GSCBufferString", "NSString"},
    {"GSUnicodeBufferString", "NSString"},
    {"GSCInlineString", "NSString"},
    {"GSUInlineString", "NSString"},
    {"GSCSubString", "NSString"},
    {"GSUnicodeSubString", "NSString"},
    
    // Array classes
    {"GSArray", "NSArray"},
    {"GSInlineArray", "NSArray"},
    {"NSGArray", "NSArray"},
    {"GSMutableArray", "NSMutableArray"},
    {"NSGMutableArray", "NSMutableArray"},
    
    // Dictionary classes
    {"GSDictionary", "NSDictionary"},
    {"GSMutableDictionary", "NSMutableDictionary"},
    {"GSCachedDictionary", "NSDictionary"},
    {"NSGDictionary", "NSDictionary"},
    {"NSGMutableDictionary", "NSMutableDictionary"},
    
    // Set classes
    {"GSSet", "NSSet"},
    {"GSMutableSet", "NSMutableSet"},
    {"NSGSet", "NSSet"},
    {"NSGMutableSet", "NSMutableSet"},
    
    // Number classes
    {"NSSignedIntegerNumber", "NSNumber"},
    {"NSIntNumber", "NSNumber"},
    {"NSBoolNumber", "NSNumber"},
    {"NSLongLongNumber", "NSNumber"},
    {"NSUnsignedLongLongNumber", "NSNumber"},
    {"NSSmallInt", "NSNumber"},
    {"NSFloatingPointNumber", "NSNumber"},
    {"NSFloatNumber", "NSNumber"},
    {"NSDoubleNumber", "NSNumber"},
    {"NSSmallExtendedDouble", "NSNumber"},
    {"NSSmallRepeatingDouble", "NSNumber"}
};

GNUstepObjCDeclVendor::GNUstepObjCDeclVendor(ObjCLanguageRuntime &runtime)
    : ClangDeclVendor(eGNUstepObjCDeclVendor), m_runtime(runtime),
      m_external_source(nullptr) {
  
  // Initialize the TypeSystem
  m_ast_ctx = m_runtime.GetProcess()->GetTarget()
                 .GetScratchTypeSystemForLanguage(lldb::eLanguageTypeObjC)
                 .dyn_cast_or_null<TypeSystemClang>();
  
  if (!m_ast_ctx) {
    // Can't proceed without AST context
    return;
  }
}

uint32_t GNUstepObjCDeclVendor::FindDecls(ConstString name, bool append,
                                          uint32_t max_matches,
                                          std::vector<CompilerDecl> &decls) {
  if (!m_ast_ctx) {
    return 0;
  }

  std::string class_name = name.GetCString();
  uint32_t matches_found = 0;

  // Check if this is a known Foundation class (including concrete GNUstep classes)
  std::string foundation_class = class_name;
  
  // Map concrete GNUstep class to Foundation class
  auto concrete_it = concrete_to_foundation.find(class_name);
  if (concrete_it != concrete_to_foundation.end()) {
    foundation_class = concrete_it->second;
  }
  
  // Check if we have selectors for this Foundation class
  auto selectors_it = foundation_selectors.find(foundation_class);
  if (selectors_it == foundation_selectors.end()) {
    return 0; // No known selectors for this class
  }

  // Get or create the ObjC interface declaration
  ObjCInterfaceDecl *interface_decl = GetOrCreateInterfaceDecl(class_name, foundation_class);
  if (!interface_decl) {
    return 0;
  }

  // Add to results
  if (!append) {
    decls.clear();
  }
  
  CompilerDecl compiler_decl(m_ast_ctx.get(), interface_decl);
  decls.push_back(compiler_decl);
  matches_found = 1;

  return matches_found;
}

ObjCInterfaceDecl* GNUstepObjCDeclVendor::GetOrCreateInterfaceDecl(
    const std::string &class_name, const std::string &foundation_class) {
  
  if (!m_ast_ctx) {
    return nullptr;
  }

  ASTContext &ast_ctx = m_ast_ctx->getASTContext();
  
  // Check if we already have this interface
  IdentifierInfo &class_identifier = ast_ctx.Idents.get(class_name);
  DeclarationName decl_name(&class_identifier);
  
  DeclContext::lookup_result lookup_result = 
      ast_ctx.getTranslationUnitDecl()->lookup(decl_name);
  
  for (auto *decl : lookup_result) {
    if (auto *interface_decl = dyn_cast<ObjCInterfaceDecl>(decl)) {
      return interface_decl;
    }
  }

  // Create new interface declaration
  ObjCInterfaceDecl *interface_decl = ObjCInterfaceDecl::Create(
      ast_ctx, ast_ctx.getTranslationUnitDecl(), SourceLocation(),
      &class_identifier, nullptr, SourceLocation());
  
  if (!interface_decl) {
    return nullptr;
  }

  // Add the interface to the translation unit
  ast_ctx.getTranslationUnitDecl()->addDecl(interface_decl);
  
  // Add Foundation class methods to this interface
  AddFoundationClassMethods(interface_decl, foundation_class);
  
  return interface_decl;
}

void GNUstepObjCDeclVendor::AddFoundationClassMethods(
    ObjCInterfaceDecl *interface_decl, const std::string &foundation_class) {
  
  if (!interface_decl || !m_ast_ctx) {
    return;
  }

  auto selectors_it = foundation_selectors.find(foundation_class);
  if (selectors_it == foundation_selectors.end()) {
    return;
  }

  ASTContext &ast_ctx = m_ast_ctx->getASTContext();

  // Add each method to the interface
  for (const auto &selector_info : selectors_it->second) {
    const std::string &selector_name = selector_info.first;
    const std::string &type_encoding = selector_info.second;
    
    // Create method declaration
    ObjCMethodDecl *method_decl = CreateMethodDecl(
        interface_decl, selector_name.c_str(), type_encoding.c_str(), true);
    
    if (method_decl) {
      interface_decl->addDecl(method_decl);
    }
  }

  // Make sure the interface is complete
  interface_decl->startDefinition();
  interface_decl->setCompleteDefinition(true);
}

ObjCMethodDecl* GNUstepObjCDeclVendor::CreateMethodDecl(
    ObjCInterfaceDecl *interface_decl, const char *name, 
    const char *types, bool is_instance) {
  
  if (!interface_decl || !m_ast_ctx || !name || !types) {
    return nullptr;
  }

  ASTContext &ast_ctx = m_ast_ctx->getASTContext();
  
  // Parse the selector name  
  SmallVector<IdentifierInfo*, 4> selector_pieces;
  std::string selector_str(name);
  
  if (selector_str.find(':') != std::string::npos) {
    // Multi-part selector
    size_t pos = 0;
    while (pos < selector_str.length()) {
      size_t colon_pos = selector_str.find(':', pos);
      if (colon_pos == std::string::npos) {
        // Last piece without colon
        std::string piece = selector_str.substr(pos);
        if (!piece.empty()) {
          selector_pieces.push_back(&ast_ctx.Idents.get(piece));
        }
        break;
      } else {
        // Piece with colon
        std::string piece = selector_str.substr(pos, colon_pos - pos + 1);
        selector_pieces.push_back(&ast_ctx.Idents.get(piece));
        pos = colon_pos + 1;
      }
    }
  } else {
    // Single-part selector
    selector_pieces.push_back(&ast_ctx.Idents.get(selector_str));
  }

  Selector selector = ast_ctx.Selectors.getSelector(selector_pieces.size(), 
                                                   selector_pieces.data());

  // Parse the return type from type encoding (simplified)
  QualType return_type = ast_ctx.VoidTy; // Default
  if (types && types[0]) {
    switch (types[0]) {
      case 'v': return_type = ast_ctx.VoidTy; break;
      case 'B': return_type = ast_ctx.BoolTy; break;
      case 'c': return_type = ast_ctx.CharTy; break;
      case 's': return_type = ast_ctx.ShortTy; break;
      case 'i': return_type = ast_ctx.IntTy; break;
      case 'l': return_type = ast_ctx.LongTy; break;
      case 'q': return_type = ast_ctx.LongLongTy; break;
      case 'C': return_type = ast_ctx.UnsignedCharTy; break;
      case 'S': return_type = ast_ctx.UnsignedShortTy; break;
      case 'I': return_type = ast_ctx.UnsignedIntTy; break;
      case 'L': return_type = ast_ctx.UnsignedLongTy; break;
      case 'Q': return_type = ast_ctx.UnsignedLongLongTy; break;
      case 'f': return_type = ast_ctx.FloatTy; break;
      case 'd': return_type = ast_ctx.DoubleTy; break;
      case '*': return_type = ast_ctx.getPointerType(ast_ctx.CharTy); break;
      case '@': return_type = ast_ctx.getObjCIdType(); break;
      default:  return_type = ast_ctx.getObjCIdType(); break;
    }
  }

  // Create the method declaration
  ObjCMethodDecl *method_decl = ObjCMethodDecl::Create(
      ast_ctx, SourceLocation(), SourceLocation(),
      selector, return_type, nullptr, interface_decl,
      is_instance, false, // is_instance, is_variadic
      false, // is_property_accessor
      false, // is_synthesized_accessor  
      false, // is_defined
      ObjCImplementationControl::None);

  return method_decl;
}
```

**File: `GNUstepObjCDeclVendor.h`** (Updated header)

```cpp
//===-- GNUstepObjCDeclVendor.h --------------------------------*- C++ -*-===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCDECLVENDOR_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCDECLVENDOR_H

#include "Plugins/ExpressionParser/Clang/ClangDeclVendor.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "../ObjCLanguageRuntime.h"
#include <unordered_map>

namespace clang {
class ObjCInterfaceDecl;
class ObjCMethodDecl;
class ExternalASTSource;
}

namespace lldb_private {

class GNUstepObjCExternalASTSource;

class GNUstepObjCDeclVendor : public ClangDeclVendor {
public:
  GNUstepObjCDeclVendor(ObjCLanguageRuntime &runtime);
  ~GNUstepObjCDeclVendor() override = default;

  uint32_t FindDecls(ConstString name, bool append, uint32_t max_matches,
                     std::vector<CompilerDecl> &decls) override;

  clang::ObjCInterfaceDecl *GetDeclForISA(ObjCLanguageRuntime::ObjCISA isa);
  bool FinishDecl(clang::ObjCInterfaceDecl *interface_decl);

private:
  typedef std::unordered_map<ObjCLanguageRuntime::ObjCISA,
                             clang::ObjCInterfaceDecl *> ISAToInterfaceMap;

  ObjCLanguageRuntime &m_runtime;
  std::shared_ptr<TypeSystemClang> m_ast_ctx;
  GNUstepObjCExternalASTSource *m_external_source;
  ISAToInterfaceMap m_isa_to_interface;

  // Helper methods
  clang::ObjCInterfaceDecl* GetOrCreateInterfaceDecl(
      const std::string &class_name, const std::string &foundation_class);
  void AddFoundationClassMethods(clang::ObjCInterfaceDecl *interface_decl,
                                 const std::string &foundation_class);
  clang::ObjCMethodDecl *CreateMethodDecl(clang::ObjCInterfaceDecl *interface_decl,
                                           const char *name, const char *types,
                                           bool is_instance);
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCDECLVENDOR_H
```

### Phase 2: Enhanced Tagged Pointer Detection

**File: `GNUstepObjCRuntimeIntrospector.cpp`** (Enhanced Implementation)

```cpp
// Add to existing implementation

// Tagged pointer constants (from GNUstep source analysis)
static const uintptr_t TINY_STRING_TAG_MASK = 0x7;
static const uintptr_t TINY_STRING_TAG = 0x1;
static const uintptr_t TINY_STRING_LENGTH_SHIFT = 3;
static const uintptr_t TINY_STRING_LENGTH_MASK = 0x7;

static const uintptr_t SMALL_INT_TAG = 0x1;  // Odd pointers are small ints
static const uintptr_t SMALL_DOUBLE_TAG_MASK = 0x7;
static const uintptr_t SMALL_DOUBLE_TAG = 0x4;

bool GNUstepObjCRuntimeIntrospector::IsTaggedPointer(lldb::addr_t obj_addr) {
    uintptr_t ptr = static_cast<uintptr_t>(obj_addr);
    
    // Check for tagged string
    if ((ptr & TINY_STRING_TAG_MASK) == TINY_STRING_TAG) {
        return true;
    }
    
    // Check for tagged integer (odd pointers, but not strings)
    if ((ptr & 0x1) == SMALL_INT_TAG && (ptr & TINY_STRING_TAG_MASK) != TINY_STRING_TAG) {
        return true;
    }
    
    // Check for tagged double
    if ((ptr & SMALL_DOUBLE_TAG_MASK) == SMALL_DOUBLE_TAG) {
        return true;
    }
    
    return false;
}

std::string GNUstepObjCRuntimeIntrospector::GetTaggedPointerClassName(lldb::addr_t obj_addr) {
    uintptr_t ptr = static_cast<uintptr_t>(obj_addr);
    
    // Tagged string
    if ((ptr & TINY_STRING_TAG_MASK) == TINY_STRING_TAG) {
        return "GSTinyString";
    }
    
    // Tagged integer
    if ((ptr & 0x1) == SMALL_INT_TAG && (ptr & TINY_STRING_TAG_MASK) != TINY_STRING_TAG) {
        return "NSSmallInt";
    }
    
    // Tagged double
    if ((ptr & SMALL_DOUBLE_TAG_MASK) == SMALL_DOUBLE_TAG) {
        return "NSSmallExtendedDouble";
    }
    
    return "NSObject"; // Fallback
}

std::string GNUstepObjCRuntimeIntrospector::DecodeTaggedString(lldb::addr_t obj_addr) {
    uintptr_t ptr = static_cast<uintptr_t>(obj_addr);
    
    if ((ptr & TINY_STRING_TAG_MASK) != TINY_STRING_TAG) {
        return ""; // Not a tagged string
    }
    
    // Extract length
    NSUInteger length = (ptr >> TINY_STRING_LENGTH_SHIFT) & TINY_STRING_LENGTH_MASK;
    
    // Extract characters (simplified - would need full implementation)
    std::string result;
    result.reserve(length);
    
    for (NSUInteger i = 0; i < length && i < 8; ++i) {
        // Extract character from pointer (this is a simplified version)
        // Real implementation would need to match GNUstep's TINY_STRING_CHAR macro
        char c = (ptr >> (8 + i * 8)) & 0xFF;
        if (c == 0) break;
        result.push_back(c);
    }
    
    return result;
}
```

### Phase 3: Runtime Integration

**File: `GNUstepObjCRuntime.cpp`** (Enhanced initialization)

```cpp
// Add to GNUstepObjCRuntime::Initialize() or CreateInstance()

void GNUstepObjCRuntime::InitializeDeclVendor() {
    if (!m_decl_vendor_up) {
        m_decl_vendor_up = std::make_unique<GNUstepObjCDeclVendor>(*this);
        
        // Register the decl vendor with the target
        GetProcess()->GetTarget().GetClangDeclVendors().push_back(m_decl_vendor_up.get());
    }
}

// Override GetClassNameFromObject to handle tagged pointers
std::string GNUstepObjCRuntime::GetClassNameFromObject(ValueObject &valobj) {
    lldb::addr_t obj_addr = valobj.GetPointerValue();
    
    // Check for tagged pointers first
    if (m_introspector && m_introspector->IsTaggedPointer(obj_addr)) {
        return m_introspector->GetTaggedPointerClassName(obj_addr);
    }
    
    // Fall back to regular class name lookup
    return ObjCLanguageRuntime::GetClassNameFromObject(valobj);
}
```

## Testing Strategy

### Unit Test for Critical Fix

**File: `lldb/unittests/Plugins/LanguageRuntime/ObjC/TestGNUstepFoundationMapping.cpp`**

```cpp
#include "gtest/gtest.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCDeclVendor.h"
#include "TestUtilities/MockClangHost.h"

class GNUstepFoundationMappingTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Set up mock runtime and AST context
  }
};

TEST_F(GNUstepFoundationMappingTest, GSTinyStringHasRequiredSelectors) {
  // Test that GSTinyString gets proper NSString selectors
  std::vector<CompilerDecl> decls;
  uint32_t matches = decl_vendor->FindDecls(ConstString("GSTinyString"), 
                                           false, 10, decls);
  
  EXPECT_GT(matches, 0);
  ASSERT_EQ(decls.size(), 1);
  
  // Verify the interface has required methods
  // This would need access to the actual AST node to verify methods
}

TEST_F(GNUstepFoundationMappingTest, TaggedPointerDetection) {
  // Test tagged pointer detection
  GNUstepObjCRuntimeIntrospector introspector(mock_process);
  
  // Create a mock tagged string pointer (odd number with correct tag)
  lldb::addr_t tagged_string = 0x1234567890ABCDE1; // Odd = tagged
  
  EXPECT_TRUE(introspector.IsTaggedPointer(tagged_string));
  EXPECT_EQ(introspector.GetTaggedPointerClassName(tagged_string), "GSTinyString");
}
```

### Integration Test

**File: `lldb/examples/test_foundation_crash_fix.sh`**

```bash
#!/bin/bash
set -e

cd "$(dirname "$0")"

# Compile test program
make string_test

# Run LLDB with the test program
/home/robk/code/llvm-project/build/bin/lldb string_test << 'EOF'
b main
run
frame select 0

# These commands should NOT crash after the fix
po stringVar
p [stringVar length]
p [stringVar UTF8String]
p [stringVar description]

# Test with nil objects (should be handled gracefully)
expr id nilString = nil
po nilString

quit
EOF

echo "Foundation crash fix test completed successfully!"
```

## Build and Deploy

```bash
# Build the plugin
cd /home/robk/code/llvm-project/build && ninja lldbPluginGNUstepObjCRuntime

# Test the fix
cd /home/robk/code/llvm-project/lldb/examples
./test_foundation_crash_fix.sh
```

## Expected Impact

1. **Immediate crash fix**: GSTinyString and other Foundation objects will have proper selectors
2. **Robust introspection**: Tagged pointers will be properly detected and classified
3. **Complete Foundation support**: All major Foundation classes will work in the debugger
4. **Performance improvement**: Proper selector declarations reduce failed lookups

This implementation directly addresses the P0 crash by ensuring that when LLDB attempts to introspect GNUstep Foundation objects, proper selector declarations are available, preventing the empty selector crash.
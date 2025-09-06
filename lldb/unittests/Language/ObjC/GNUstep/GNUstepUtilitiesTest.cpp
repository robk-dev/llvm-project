//===-- GNUstepUtilitiesTest.cpp -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeUtilities.h"
#include "Plugins/Platform/Linux/PlatformLinux.h"
#include "lldb/Core/Module.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Target/Platform.h"
#include "gtest/gtest.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::gnustep_objc_runtime_utilities;

class GNUstepUtilitiesTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
    platform_linux::PlatformLinux::Initialize();
    Platform::SetHostPlatform(
        platform_linux::PlatformLinux::CreateInstance(true, nullptr));
  }

  void TearDown() override {
    platform_linux::PlatformLinux::Terminate();
    HostInfo::Terminate();
    FileSystem::Terminate();
  }
};

// Test tagged pointer detection logic (bit manipulation only)
TEST_F(GNUstepUtilitiesTest, TaggedPointerDetection) {
  // On 64-bit systems, tagged pointers have bit 63 set
  uint64_t tagged_ptr = 0x8000000000000001ULL;
  uint64_t regular_ptr = 0x7FFFFFFF00000000ULL;
  uint64_t null_ptr = 0x0;
  
  // The low bit indicates tagged pointer in GNUstep
  EXPECT_TRUE(IsTaggedPointer(tagged_ptr));
  EXPECT_FALSE(IsTaggedPointer(regular_ptr));
  EXPECT_FALSE(IsTaggedPointer(null_ptr));
}

// Test tagged pointer slot extraction (bit manipulation only)
TEST_F(GNUstepUtilitiesTest, TaggedPointerSlotExtraction) {
  // Tagged pointer format: bit 63 set, bits 1-3 contain slot
  uint64_t tagged_with_slot_0 = 0x8000000000000001ULL;  // slot 0
  uint64_t tagged_with_slot_1 = 0x8000000000000003ULL;  // slot 1 
  uint64_t tagged_with_slot_7 = 0x800000000000000FULL;  // slot 7
  
  EXPECT_EQ(GetTaggedPointerSlot(tagged_with_slot_0), 0);
  EXPECT_EQ(GetTaggedPointerSlot(tagged_with_slot_1), 1);
  EXPECT_EQ(GetTaggedPointerSlot(tagged_with_slot_7), 7);
}

// Test address size calculations
TEST_F(GNUstepUtilitiesTest, AddressSizeCalculations) {
  // Test various address sizes
  EXPECT_EQ(GetPointerSize(4), 4);  // 32-bit
  EXPECT_EQ(GetPointerSize(8), 8);  // 64-bit
  
  // Test struct alignment calculations
  EXPECT_EQ(AlignTo(5, 4), 8);   // 5 aligned to 4 = 8
  EXPECT_EQ(AlignTo(7, 8), 8);   // 7 aligned to 8 = 8
  EXPECT_EQ(AlignTo(16, 8), 16); // 16 aligned to 8 = 16
}

// Test helper string utilities
TEST_F(GNUstepUtilitiesTest, StringHelpers) {
  // Test class name extraction from mangled names
  std::string mangled = "_OBJC_CLASS_$_NSString";
  std::string extracted = ExtractClassName(mangled);
  EXPECT_EQ(extracted, "NSString");
  
  // Test selector name cleanup
  std::string selector = "initWithString:";
  std::string cleaned = CleanSelectorName(selector);
  EXPECT_EQ(cleaned, "initWithString:");
  
  // Test nil/empty string handling
  EXPECT_EQ(ExtractClassName(""), "");
  EXPECT_EQ(CleanSelectorName(""), "");
}

// Test error message formatting
TEST_F(GNUstepUtilitiesTest, ErrorMessageFormatting) {
  std::string error = FormatRuntimeError("objc_getClass", "Class not found");
  EXPECT_TRUE(error.find("objc_getClass") != std::string::npos);
  EXPECT_TRUE(error.find("Class not found") != std::string::npos);
  
  // Test with empty inputs
  std::string empty_error = FormatRuntimeError("", "");
  EXPECT_FALSE(empty_error.empty());
}

// Test memory alignment helpers
TEST_F(GNUstepUtilitiesTest, MemoryAlignmentHelpers) {
  // Test various alignments
  EXPECT_TRUE(IsAligned(0, 4));
  EXPECT_TRUE(IsAligned(4, 4));
  EXPECT_TRUE(IsAligned(8, 4));
  EXPECT_FALSE(IsAligned(5, 4));
  EXPECT_FALSE(IsAligned(7, 4));
  
  EXPECT_TRUE(IsAligned(0, 8));
  EXPECT_TRUE(IsAligned(8, 8));
  EXPECT_TRUE(IsAligned(16, 8));
  EXPECT_FALSE(IsAligned(4, 8));
  EXPECT_FALSE(IsAligned(12, 8));
}

// Test byte order detection helpers
TEST_F(GNUstepUtilitiesTest, ByteOrderHelpers) {
  // Test endianness conversions
  uint32_t value = 0x12345678;
  uint32_t swapped = SwapBytes32(value);
  EXPECT_EQ(swapped, 0x78563412);
  
  uint64_t value64 = 0x123456789ABCDEF0ULL;
  uint64_t swapped64 = SwapBytes64(value64);
  EXPECT_EQ(swapped64, 0xF0DEBC9A78563412ULL);
}

// Test plugin registration mechanics
TEST_F(GNUstepUtilitiesTest, PluginRegistration) {
  // Test plugin name generation
  std::string plugin_name = GetPluginName();
  EXPECT_EQ(plugin_name, "gnustep-objc-runtime");
  
  // Test plugin description
  std::string description = GetPluginDescription();
  EXPECT_TRUE(description.find("GNUstep") != std::string::npos);
  EXPECT_TRUE(description.find("Objective-C") != std::string::npos);
}
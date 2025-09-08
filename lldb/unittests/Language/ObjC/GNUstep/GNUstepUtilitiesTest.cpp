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

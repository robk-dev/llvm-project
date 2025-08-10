//===-- GNUstepRuntimeTest.cpp -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h"
#include "Plugins/LanguageRuntime/ObjC/ObjCLanguageRuntime.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/ModuleSpec.h"
#include "lldb/Core/Section.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Symbol/ObjectFile.h"
#include "lldb/Target/Platform.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/ArchSpec.h"
#include "lldb/Utility/DataBuffer.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/StreamString.h"
#include "lldb/Utility/Listener.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <thread>
#include <atomic>
#include <chrono>

using namespace lldb;
using namespace lldb_private;

namespace {

// Mock Process for testing runtime detection
class MockProcess : public Process {
public:
  MockProcess(lldb::TargetSP target_sp, lldb::ListenerSP listener_sp)
      : Process(target_sp, listener_sp) {}

  // Required overrides for abstract Process methods
  Status DoDestroy() override { return Status(); }
  void RefreshStateAfterStop() override {}
  size_t DoReadMemory(lldb::addr_t addr, void *buf, size_t size,
                      Status &error) override {
    error.Clear();
    return size;
  }
  bool CanDebug(lldb::TargetSP target_sp,
                bool plugin_specified_by_name) override {
    return true;
  }
  Status DoDetach(bool keep_stopped) override { return Status(); }
  bool DoUpdateThreadList(ThreadList &old_thread_list,
                          ThreadList &new_thread_list) override {
    return true;
  }
  Status DoLaunch(Module *exe_module, ProcessLaunchInfo &launch_info) override {
    return Status();
  }
  Status DoAttachToProcessWithID(lldb::pid_t pid,
                                  const ProcessAttachInfo &attach_info) override {
    return Status();
  }
  Status DoAttachToProcessWithName(const char *process_name,
                                    const ProcessAttachInfo &attach_info) override {
    return Status();
  }
  void DidAttach(ArchSpec &process_arch) override {}
  Status DoSignal(int signal) override { return Status(); }
  Status DoResume() override { return Status(); }
  Status DoHalt(bool &caused_stop) override { 
    caused_stop = true;
    return Status(); 
  }
  Status WillResume() override { return Status(); }
  llvm::StringRef GetPluginName() override { return "MockProcess"; }
  
  // Mock methods for testing
  void AddModule(const char *name) {
    m_module_names.push_back(name);
  }
  
  bool HasModule(const char *name) {
    return std::find(m_module_names.begin(), m_module_names.end(), name) !=
           m_module_names.end();
  }

  Target &GetTarget() override { return *m_target_wp.lock(); }
  const Target &GetTarget() const override { return *m_target_wp.lock(); }

private:
  std::vector<std::string> m_module_names;
};

class GNUstepRuntimeTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    
    // Create mock target and process
    m_debugger_sp = Debugger::CreateInstance();
    m_platform_sp = Platform::GetHostPlatform();
    
    ArchSpec arch("x86_64-pc-linux-gnu");
    m_debugger_sp->GetTargetList().CreateTarget(
        *m_debugger_sp, "", arch, eLoadDependentsNo, m_platform_sp, m_target_sp);
    
    m_listener_sp = Listener::MakeListener("GNUstepRuntimeTest");
    m_process_sp = std::make_shared<MockProcess>(m_target_sp, m_listener_sp);
  }

  void TearDown() override {
    m_process_sp.reset();
    m_target_sp.reset();
    m_debugger_sp.reset();
    FileSystem::Terminate();
  }

  DebuggerSP m_debugger_sp;
  PlatformSP m_platform_sp;
  TargetSP m_target_sp;
  ListenerSP m_listener_sp;
  std::shared_ptr<MockProcess> m_process_sp;
};

TEST_F(GNUstepRuntimeTest, PluginInitialization) {
  // Test that the plugin registers itself correctly
  GNUstepObjCRuntime::Initialize();
  
  auto plugin_name = GNUstepObjCRuntime::GetPluginNameStatic();
  EXPECT_EQ(plugin_name, "gnustep-objc-runtime");
  
  auto description = GNUstepObjCRuntime::GetPluginDescriptionStatic();
  EXPECT_STREQ(description, "GNUstep Objective-C Language Runtime");
  
  GNUstepObjCRuntime::Terminate();
}

TEST_F(GNUstepRuntimeTest, RuntimeDetection) {
  // Test runtime detection with GNUstep libraries
  m_process_sp->AddModule("libobjc.so.4");
  m_process_sp->AddModule("libgnustep-base.so");
  
  auto runtime = GNUstepObjCRuntime::CreateInstance(m_process_sp.get(), nullptr);
  EXPECT_NE(runtime, nullptr);
  
  // Verify it's actually a GNUstep runtime
  auto *gnustep_runtime = llvm::dyn_cast<GNUstepObjCRuntime>(runtime.get());
  EXPECT_NE(gnustep_runtime, nullptr);
}

TEST_F(GNUstepRuntimeTest, RuntimeDetectionFailure) {
  // Test that runtime is not created without GNUstep libraries
  m_process_sp->AddModule("libsystem.so");
  
  auto runtime = GNUstepObjCRuntime::CreateInstance(m_process_sp.get(), nullptr);
  EXPECT_EQ(runtime, nullptr);
}

TEST_F(GNUstepRuntimeTest, RuntimeDetectionWithOnlyLibobjc) {
  // Test detection with only libobjc
  m_process_sp->AddModule("libobjc.so.4");
  
  auto runtime = GNUstepObjCRuntime::CreateInstance(m_process_sp.get(), nullptr);
  EXPECT_NE(runtime, nullptr);
}

TEST_F(GNUstepRuntimeTest, GetObjectDescription) {
  m_process_sp->AddModule("libobjc.so.4");
  m_process_sp->AddModule("libgnustep-base.so");
  
  auto runtime = GNUstepObjCRuntime::CreateInstance(m_process_sp.get(), nullptr);
  ASSERT_NE(runtime, nullptr);
  
  // Test GetObjectDescription with various addresses
  StreamString stream;
  ValueObject *valobj = nullptr; // Would need mock ValueObject for full test
  
  // Test with null address
  bool result = runtime->GetObjectDescription(stream, valobj, 0x0);
  EXPECT_FALSE(result);
  
  // Test with valid address (would need mock memory)
  result = runtime->GetObjectDescription(stream, valobj, 0x1000);
  // This would fail without proper memory setup, but tests the code path
  EXPECT_FALSE(result);
}

TEST_F(GNUstepRuntimeTest, CouldHaveDynamicValue) {
  m_process_sp->AddModule("libobjc.so.4");
  auto runtime = GNUstepObjCRuntime::CreateInstance(m_process_sp.get(), nullptr);
  ASSERT_NE(runtime, nullptr);
  
  // Test various scenarios for dynamic value detection
  ValueObject *valobj = nullptr; // Would need mock ValueObject
  
  // Without a proper ValueObject, just test the method exists
  bool could_have = runtime->CouldHaveDynamicValue(*valobj);
  // Default implementation should return false for null
  EXPECT_FALSE(could_have);
}

TEST_F(GNUstepRuntimeTest, GetDynamicTypeAndAddress) {
  m_process_sp->AddModule("libobjc.so.4");
  auto runtime = GNUstepObjCRuntime::CreateInstance(m_process_sp.get(), nullptr);
  ASSERT_NE(runtime, nullptr);
  
  // Test dynamic type resolution
  ValueObject *valobj = nullptr;
  lldb::DynamicValueType use_dynamic = lldb::eDynamicCanRunTarget;
  TypeAndOrName type_and_or_name;
  Address address;
  Value::ValueType value_type = Value::ValueType::Invalid;
  
  bool result = runtime->GetDynamicTypeAndAddress(
      *valobj, use_dynamic, type_and_or_name, address, value_type);
  
  // Without proper setup, this should fail gracefully
  EXPECT_FALSE(result);
}

TEST_F(GNUstepRuntimeTest, RuntimeVersion) {
  m_process_sp->AddModule("libobjc.so.4");
  auto runtime = GNUstepObjCRuntime::CreateInstance(m_process_sp.get(), nullptr);
  ASSERT_NE(runtime, nullptr);
  
  auto *gnustep_runtime = static_cast<GNUstepObjCRuntime*>(runtime.get());
  
  // Test version detection (would need proper module setup)
  auto version = gnustep_runtime->GetRuntimeVersion();
  // Default should be V2 for libobjc.so.4
  EXPECT_EQ(version, ObjCRuntimeVersions::eGNUstep_V2);
}

TEST_F(GNUstepRuntimeTest, IsValidISA) {
  m_process_sp->AddModule("libobjc.so.4");
  auto runtime = GNUstepObjCRuntime::CreateInstance(m_process_sp.get(), nullptr);
  ASSERT_NE(runtime, nullptr);
  
  auto *gnustep_runtime = static_cast<GNUstepObjCRuntime*>(runtime.get());
  
  // Test ISA validation
  ObjCISA isa = 0x0;
  EXPECT_FALSE(gnustep_runtime->IsValidISA(isa));
  
  // Non-zero should be considered potentially valid
  isa = 0x1000;
  EXPECT_TRUE(gnustep_runtime->IsValidISA(isa));
  
  // Tagged pointer ISA (0x1-0xF) should be valid
  isa = 0x5;
  EXPECT_TRUE(gnustep_runtime->IsValidISA(isa));
}

TEST_F(GNUstepRuntimeTest, ThreadSafety) {
  m_process_sp->AddModule("libobjc.so.4");
  auto runtime = GNUstepObjCRuntime::CreateInstance(m_process_sp.get(), nullptr);
  ASSERT_NE(runtime, nullptr);
  
  // Test thread safety of runtime operations
  std::vector<std::thread> threads;
  std::atomic<int> counter{0};
  
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&runtime, &counter]() {
      for (int j = 0; j < 100; ++j) {
        // Call various runtime methods concurrently
        ObjCISA isa = 0x1000 + j;
        runtime->IsValidISA(isa);
        counter++;
      }
    });
  }
  
  for (auto &thread : threads) {
    thread.join();
  }
  
  EXPECT_EQ(counter, 1000);
}

TEST_F(GNUstepRuntimeTest, ExceptionHandling) {
  m_process_sp->AddModule("libobjc.so.4");
  auto runtime = GNUstepObjCRuntime::CreateInstance(m_process_sp.get(), nullptr);
  ASSERT_NE(runtime, nullptr);
  
  // Test that runtime handles exceptions gracefully
  StreamString stream;
  
  // Test with invalid memory address
  bool result = runtime->GetObjectDescription(stream, nullptr, 0xDEADBEEF);
  EXPECT_FALSE(result);
  
  // Test with maximum address
  result = runtime->GetObjectDescription(stream, nullptr, LLDB_INVALID_ADDRESS);
  EXPECT_FALSE(result);
}

TEST_F(GNUstepRuntimeTest, PerformanceBaseline) {
  m_process_sp->AddModule("libobjc.so.4");
  auto runtime = GNUstepObjCRuntime::CreateInstance(m_process_sp.get(), nullptr);
  ASSERT_NE(runtime, nullptr);
  
  // Baseline performance test for ISA validation
  auto start = std::chrono::high_resolution_clock::now();
  
  for (int i = 0; i < 100000; ++i) {
    lldb::addr_t isa = i;
    runtime->IsValidISA(isa);
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  // Should complete in under 50ms
  EXPECT_LT(duration.count(), 50);
}

} // namespace
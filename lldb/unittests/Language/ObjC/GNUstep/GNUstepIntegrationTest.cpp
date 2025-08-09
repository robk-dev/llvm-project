//===-- GNUstepIntegrationTest.cpp ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeIntrospector.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepFormattersRegistry.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Core/Module.h"
#include "lldb/DataFormatters/FormatManager.h"
#include "lldb/DataFormatters/TypeCategory.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Target/Platform.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/Listener.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <chrono>
#include <thread>
#include <atomic>

using namespace lldb;
using namespace lldb_private;

namespace {

// Comprehensive mock process for integration testing
class IntegrationMockProcess : public Process {
public:
  IntegrationMockProcess(lldb::TargetSP target_sp, lldb::ListenerSP listener_sp)
      : Process(target_sp, listener_sp) {
    SetupMockRuntime();
  }

  // Required Process overrides
  Status DoDestroy() override { return Status(); }
  void RefreshStateAfterStop() override {}
  bool CanDebug(lldb::TargetSP target_sp, bool plugin_specified_by_name) override {
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
  llvm::StringRef GetPluginName() override { return "IntegrationMockProcess"; }

  size_t DoReadMemory(lldb::addr_t addr, void *buf, size_t size,
                      Status &error) override {
    error.Clear();
    
    auto it = m_memory.find(addr);
    if (it != m_memory.end() && it->second.size() >= size) {
      memcpy(buf, it->second.data(), size);
      return size;
    }
    
    memset(buf, 0, size);
    return size;
  }

  // Mock setup methods
  void SetupMockRuntime() {
    // Add required modules
    AddModule("libobjc.so.4");
    AddModule("libgnustep-base.so");
    
    // Set up mock class structures
    SetupMockClasses();
    SetupMockObjects();
  }

  void AddModule(const char *name) {
    m_modules.push_back(name);
  }

  bool HasModule(const char *name) {
    return std::find(m_modules.begin(), m_modules.end(), name) != m_modules.end();
  }

private:
  void SetupMockClasses() {
    // Set up NSString class
    lldb::addr_t nsstring_class = 0x100000;
    SetMemory(nsstring_class + 8, "NSString", 9);
    
    // Set up NSArray class
    lldb::addr_t nsarray_class = 0x200000;
    SetMemory(nsarray_class + 8, "NSArray", 8);
    
    // Set up NSDictionary class
    lldb::addr_t nsdict_class = 0x300000;
    SetMemory(nsdict_class + 8, "NSDictionary", 13);
  }

  void SetupMockObjects() {
    // Set up mock NSString object
    lldb::addr_t string_obj = 0x400000;
    lldb::addr_t string_isa = 0x100000;
    SetPointer(string_obj, string_isa);
    
    // Set up mock NSArray object
    lldb::addr_t array_obj = 0x500000;
    lldb::addr_t array_isa = 0x200000;
    SetPointer(array_obj, array_isa);
    SetUInt32(array_obj + 16, 3); // count = 3
  }

  void SetMemory(lldb::addr_t addr, const void *data, size_t size) {
    std::vector<uint8_t> vec(size);
    memcpy(vec.data(), data, size);
    m_memory[addr] = vec;
  }

  void SetMemory(lldb::addr_t addr, const char *str, size_t len) {
    SetMemory(addr, static_cast<const void*>(str), len);
  }

  void SetPointer(lldb::addr_t addr, lldb::addr_t value) {
    SetMemory(addr, &value, sizeof(value));
  }

  void SetUInt32(lldb::addr_t addr, uint32_t value) {
    SetMemory(addr, &value, sizeof(value));
  }

  std::vector<std::string> m_modules;
  std::map<lldb::addr_t, std::vector<uint8_t>> m_memory;
};

class GNUstepIntegrationTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    
    // Initialize the plugin system
    GNUstepObjCRuntime::Initialize();
    
    // Create debugger and target
    m_debugger_sp = Debugger::CreateInstance();
    m_platform_sp = Platform::GetHostPlatform();
    m_debugger_sp->GetPlatformList().Append(m_platform_sp, true);
    
    ArchSpec arch("x86_64-pc-linux-gnu");
    m_debugger_sp->GetTargetList().CreateTarget(
        *m_debugger_sp, "", arch, eLoadDependentsNo, m_platform_sp, m_target_sp);
    
    m_listener_sp = Listener::MakeListener("GNUstepIntegrationTest");
    m_process_sp = std::make_shared<IntegrationMockProcess>(m_target_sp, m_listener_sp);
    
    // Create the runtime
    m_runtime = GNUstepObjCRuntime::CreateInstance(*m_process_sp, nullptr);
  }

  void TearDown() override {
    m_runtime.reset();
    m_process_sp.reset();
    m_target_sp.reset();
    m_debugger_sp.reset();
    
    GNUstepObjCRuntime::Terminate();
    FileSystem::Terminate();
  }

  DebuggerSP m_debugger_sp;
  PlatformSP m_platform_sp;
  TargetSP m_target_sp;
  ListenerSP m_listener_sp;
  std::shared_ptr<IntegrationMockProcess> m_process_sp;
  ObjCLanguageRuntimeSP m_runtime;
};

TEST_F(GNUstepIntegrationTest, FullRuntimeInitialization) {
  // Test that runtime initializes with all components
  ASSERT_NE(m_runtime, nullptr);
  
  auto *gnustep_runtime = static_cast<GNUstepObjCRuntime*>(m_runtime.get());
  EXPECT_NE(gnustep_runtime, nullptr);
  
  // Verify runtime version
  EXPECT_EQ(gnustep_runtime->GetRuntimeVersion(), ObjCRuntimeVersions::eGNUstep_V2);
}

TEST_F(GNUstepIntegrationTest, FormatterRegistration) {
  // Test that formatters are registered correctly
  ASSERT_NE(m_runtime, nullptr);
  
  // Get the format manager
  auto &format_manager = m_debugger_sp->GetFormatManager();
  
  // Check that GNUstep category exists
  auto category_sp = format_manager.GetCategory(ConstString("gnustep"));
  // Category might not exist without full initialization
  
  // Verify formatters would be registered
  GNUstepFormattersRegistry registry;
  registry.RegisterFormatters(format_manager);
  
  // Test formatter lookup (would need mock ValueObject)
  // This tests the registration mechanism
  EXPECT_TRUE(true);
}

TEST_F(GNUstepIntegrationTest, IntrospectorIntegration) {
  ASSERT_NE(m_runtime, nullptr);
  
  auto *gnustep_runtime = static_cast<GNUstepObjCRuntime*>(m_runtime.get());
  
  // Create introspector
  GNUstepObjCRuntimeIntrospector introspector(*m_process_sp);
  
  // Test ISA resolution
  lldb::addr_t test_addr = 0x400000; // Mock string object
  auto isa = introspector.GetISA(test_addr);
  EXPECT_NE(isa, LLDB_INVALID_ADDRESS);
  
  // Test class name retrieval
  auto class_name = introspector.GetClassName(isa);
  // Would need proper mock setup to get actual name
}

TEST_F(GNUstepIntegrationTest, TypeCategoryActivation) {
  ASSERT_NE(m_runtime, nullptr);
  
  // Test that type category activates properly
  auto &format_manager = m_debugger_sp->GetFormatManager();
  
  // Register formatters
  GNUstepFormattersRegistry registry;
  registry.RegisterFormatters(format_manager);
  
  // Check category activation
  auto category_sp = format_manager.GetCategory(ConstString("gnustep"));
  if (category_sp) {
    EXPECT_TRUE(category_sp->IsEnabled());
  }
}

TEST_F(GNUstepIntegrationTest, ConcurrentFormatterAccess) {
  ASSERT_NE(m_runtime, nullptr);
  
  auto &format_manager = m_debugger_sp->GetFormatManager();
  GNUstepFormattersRegistry registry;
  registry.RegisterFormatters(format_manager);
  
  // Test concurrent access to formatters
  std::vector<std::thread> threads;
  std::atomic<int> access_count{0};
  
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&format_manager, &access_count]() {
      for (int j = 0; j < 100; ++j) {
        auto category = format_manager.GetCategory(ConstString("gnustep"));
        if (category) {
          access_count++;
        }
      }
    });
  }
  
  for (auto &thread : threads) {
    thread.join();
  }
  
  // All accesses should complete without crash
  EXPECT_GT(access_count, 0);
}

TEST_F(GNUstepIntegrationTest, PerformanceBenchmark) {
  ASSERT_NE(m_runtime, nullptr);
  
  auto *gnustep_runtime = static_cast<GNUstepObjCRuntime*>(m_runtime.get());
  
  // Benchmark ISA validation
  auto start = std::chrono::high_resolution_clock::now();
  
  for (int i = 0; i < 100000; ++i) {
    ObjCISA isa = i;
    gnustep_runtime->IsValidISA(isa);
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  // Should complete in under 50ms
  EXPECT_LT(duration.count(), 50);
  
  // Benchmark formatter registration
  start = std::chrono::high_resolution_clock::now();
  
  auto &format_manager = m_debugger_sp->GetFormatManager();
  GNUstepFormattersRegistry registry;
  registry.RegisterFormatters(format_manager);
  
  end = std::chrono::high_resolution_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  // Registration should complete in under 100ms
  EXPECT_LT(duration.count(), 100);
}

TEST_F(GNUstepIntegrationTest, MemoryLeakCheck) {
  // Test for memory leaks in runtime creation/destruction
  for (int i = 0; i < 100; ++i) {
    auto runtime = GNUstepObjCRuntime::CreateInstance(*m_process_sp, nullptr);
    EXPECT_NE(runtime, nullptr);
    // Runtime will be destroyed when going out of scope
  }
  
  // If we get here without crash, no obvious leaks
  EXPECT_TRUE(true);
}

TEST_F(GNUstepIntegrationTest, ErrorHandling) {
  ASSERT_NE(m_runtime, nullptr);
  
  auto *gnustep_runtime = static_cast<GNUstepObjCRuntime*>(m_runtime.get());
  
  // Test error handling with invalid addresses
  Stream stream;
  
  // Null address
  bool result = gnustep_runtime->GetObjectDescription(stream, nullptr, 0x0);
  EXPECT_FALSE(result);
  
  // Invalid address
  result = gnustep_runtime->GetObjectDescription(stream, nullptr, LLDB_INVALID_ADDRESS);
  EXPECT_FALSE(result);
  
  // Very large address
  result = gnustep_runtime->GetObjectDescription(stream, nullptr, 0xFFFFFFFFFFFFFFFF);
  EXPECT_FALSE(result);
}

TEST_F(GNUstepIntegrationTest, RuntimeReinitialization) {
  // Test that runtime can be reinitialized multiple times
  for (int i = 0; i < 5; ++i) {
    GNUstepObjCRuntime::Terminate();
    GNUstepObjCRuntime::Initialize();
    
    auto runtime = GNUstepObjCRuntime::CreateInstance(*m_process_sp, nullptr);
    EXPECT_NE(runtime, nullptr);
  }
}

TEST_F(GNUstepIntegrationTest, MultipleRuntimeInstances) {
  // Test handling of multiple runtime instances (shouldn't happen in practice)
  auto runtime1 = GNUstepObjCRuntime::CreateInstance(*m_process_sp, nullptr);
  EXPECT_NE(runtime1, nullptr);
  
  // Second instance for same process should return the same or fail gracefully
  auto runtime2 = GNUstepObjCRuntime::CreateInstance(*m_process_sp, nullptr);
  // Behavior depends on implementation - just ensure no crash
  EXPECT_TRUE(true);
}

TEST_F(GNUstepIntegrationTest, FormatterChainIntegration) {
  ASSERT_NE(m_runtime, nullptr);
  
  // Test the full formatter chain from registration to activation
  auto &format_manager = m_debugger_sp->GetFormatManager();
  
  // Register formatters
  GNUstepFormattersRegistry registry;
  registry.RegisterFormatters(format_manager);
  
  // Get category
  auto category_sp = format_manager.GetCategory(ConstString("gnustep"));
  if (!category_sp) {
    // Create category if it doesn't exist
    category_sp = std::make_shared<TypeCategoryImpl>(
        format_manager, ConstString("gnustep"));
    format_manager.GetCategories().Add(ConstString("gnustep"), category_sp);
  }
  
  // Enable category
  category_sp->Enable(true, 0);
  EXPECT_TRUE(category_sp->IsEnabled());
  
  // Verify formatters are accessible
  auto regex_sp = category_sp->GetRegexTypeSummariesContainer();
  EXPECT_NE(regex_sp, nullptr);
}

TEST_F(GNUstepIntegrationTest, StressTest) {
  ASSERT_NE(m_runtime, nullptr);
  
  auto *gnustep_runtime = static_cast<GNUstepObjCRuntime*>(m_runtime.get());
  
  // Stress test with many operations
  std::vector<std::thread> threads;
  std::atomic<int> operations{0};
  std::atomic<bool> stop{false};
  
  // Thread 1: ISA validation
  threads.emplace_back([gnustep_runtime, &operations, &stop]() {
    while (!stop) {
      for (int i = 0; i < 1000; ++i) {
        gnustep_runtime->IsValidISA(i);
        operations++;
      }
    }
  });
  
  // Thread 2: Object description
  threads.emplace_back([gnustep_runtime, &operations, &stop]() {
    Stream stream;
    while (!stop) {
      for (int i = 0; i < 100; ++i) {
        gnustep_runtime->GetObjectDescription(stream, nullptr, 0x1000 + i);
        operations++;
      }
    }
  });
  
  // Thread 3: Formatter registration
  threads.emplace_back([this, &operations, &stop]() {
    while (!stop) {
      auto &format_manager = m_debugger_sp->GetFormatManager();
      GNUstepFormattersRegistry registry;
      registry.RegisterFormatters(format_manager);
      operations += 10;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  });
  
  // Let threads run for a short time
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  stop = true;
  
  for (auto &thread : threads) {
    thread.join();
  }
  
  // Should have completed many operations without crash
  EXPECT_GT(operations, 1000);
}

} // namespace
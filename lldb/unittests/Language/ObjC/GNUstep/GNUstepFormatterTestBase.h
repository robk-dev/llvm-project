// GNUstepFormatterTestBase.h - Base test infrastructure for GNUstep formatters
// Provides mock runtime helpers, memory leak detection, and performance benchmarking

#ifndef LLDB_UNITTESTS_LANGUAGE_OBJC_GNUSTEP_FORMATTERTEST_BASE_H
#define LLDB_UNITTESTS_LANGUAGE_OBJC_GNUSTEP_FORMATTERTEST_BASE_H

#include "gtest/gtest.h"
#include "lldb/Core/ValueObject.h"
#include "lldb/DataFormatters/FormatManager.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/DataFormatters/TypeSynthetic.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/Status.h"
#include "llvm/ADT/StringRef.h"
#include <chrono>
#include <memory>
#include <vector>

namespace lldb_private {
namespace gnustep {
namespace test {

// Mock memory manager for testing memory access patterns
class MockMemory {
public:
    MockMemory() = default;
    ~MockMemory() = default;
    
    // Setup mock objects in memory
    void SetupNSString(lldb::addr_t addr, const std::string& content);
    void SetupNSNumber(lldb::addr_t addr, int64_t value);
    void SetupNSArray(lldb::addr_t addr, size_t count);
    void SetupNSDictionary(lldb::addr_t addr, size_t count);
    void SetupNSIndexSet(lldb::addr_t addr, const std::vector<uint32_t>& indexes);
    void SetupNSDecimalNumber(lldb::addr_t addr, const std::string& value);
    void SetupNSCharacterSet(lldb::addr_t addr, uint32_t bitmap);
    void SetupClass(lldb::addr_t isa_addr, const std::string& class_name);
    void SetupObject(lldb::addr_t obj_addr, lldb::addr_t isa_addr);
    
    // Memory access
    Status ReadMemory(lldb::addr_t addr, void* buffer, size_t size);
    Status WriteMemory(lldb::addr_t addr, const void* buffer, size_t size);
    
    // Memory validation
    bool IsValidAddress(lldb::addr_t addr) const;
    void SetAddressRange(lldb::addr_t start, lldb::addr_t end);
    
    // Memory leak detection
    void StartLeakDetection();
    size_t GetLeakedBytes() const;
    void ResetLeakDetection();
    
private:
    struct MemoryRegion {
        lldb::addr_t start;
        lldb::addr_t end;
        std::vector<uint8_t> data;
    };
    
    std::map<lldb::addr_t, MemoryRegion> regions;
    lldb::addr_t valid_start = 0x1000;
    lldb::addr_t valid_end = 0xFFFFFFFF;
    
    // Leak detection
    size_t allocated_bytes = 0;
    size_t freed_bytes = 0;
};

// Mock process for testing
class MockProcess : public Process {
public:
    MockProcess(lldb::TargetSP target_sp, MockMemory* memory);
    ~MockProcess() override = default;
    
    // Required Process overrides
    Status DoReadMemory(lldb::addr_t addr, void* buf, size_t size,
                       Status& error) override;
    size_t DoWriteMemory(lldb::addr_t addr, const void* buf, size_t size,
                        Status& error) override;
    Status DoDestroy() override { return Status(); }
    void DoDidExec() override {}
    bool DoUpdateThreadList(ThreadList& old_thread_list,
                           ThreadList& new_thread_list) override { return true; }
    Status DoLaunch(Module* exe_module, ProcessLaunchInfo& launch_info) override {
        return Status();
    }
    Status DoAttachToProcessWithID(lldb::pid_t pid,
                                  const ProcessAttachInfo& attach_info) override {
        return Status();
    }
    Status DoAttachToProcessWithName(const char* process_name,
                                    const ProcessAttachInfo& attach_info) override {
        return Status();
    }
    Status DoResume() override { return Status(); }
    Status DoHalt(bool& caused_stop) override { 
        caused_stop = true;
        return Status();
    }
    Status DoDetach(bool keep_stopped) override { return Status(); }
    Status DoSignal(int signal) override { return Status(); }
    void RefreshStateAfterStop() override {}
    bool IsAlive() override { return true; }
    size_t GetSTDOUT(char* buf, size_t buf_size, Status& error) override {
        return 0;
    }
    size_t GetSTDERR(char* buf, size_t buf_size, Status& error) override {
        return 0;
    }
    size_t PutSTDIN(const char* buf, size_t buf_size, Status& error) override {
        return 0;
    }
    const char* GetPluginName() override { return "MockProcess"; }
    
    // Test helpers
    void SetMemory(MockMemory* mem) { m_memory = mem; }
    
private:
    MockMemory* m_memory;
};

// Base test fixture for formatter tests
class GNUstepFormatterTestBase : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
    
    // Helper methods for test setup
    void SetupMockRuntime();
    void SetupMockProcess();
    lldb::ValueObjectSP CreateValueObject(lldb::addr_t addr,
                                          const std::string& type_name);
    
    // Performance testing helpers
    class PerformanceTimer {
    public:
        PerformanceTimer() : start(std::chrono::high_resolution_clock::now()) {}
        
        double GetElapsedMilliseconds() const {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            return duration.count() / 1000.0;
        }
        
        void AssertUnder(double max_ms, const std::string& operation) {
            double elapsed = GetElapsedMilliseconds();
            ASSERT_LT(elapsed, max_ms) << operation << " took " << elapsed 
                                       << "ms (limit: " << max_ms << "ms)";
        }
        
    private:
        std::chrono::high_resolution_clock::time_point start;
    };
    
    // Memory leak detection
    void StartLeakCheck() { m_memory->StartLeakDetection(); }
    void AssertNoLeaks() {
        size_t leaked = m_memory->GetLeakedBytes();
        ASSERT_EQ(leaked, 0) << "Memory leak detected: " << leaked << " bytes";
    }
    
    // Common test data setup
    void SetupCommonTestData();
    
    // Formatter validation helpers
    void ValidateStringSummary(lldb::ValueObjectSP valobj,
                               const std::string& expected);
    void ValidateChildCount(lldb::ValueObjectSP valobj,
                            size_t expected_count);
    void ValidateChildAtIndex(lldb::ValueObjectSP valobj,
                              size_t index,
                              const std::string& expected_name,
                              const std::string& expected_value);
    
    // Edge case testing
    void TestNilObject(const std::string& type_name);
    void TestEmptyCollection(const std::string& type_name);
    void TestLargeCollection(const std::string& type_name, size_t size);
    void TestMalformedData(const std::string& type_name);
    
protected:
    std::unique_ptr<MockMemory> m_memory;
    std::shared_ptr<MockProcess> m_process;
    lldb::TargetSP m_target;
    lldb::DebuggerSP m_debugger;
    
    // Common test addresses
    static constexpr lldb::addr_t kTestObjectAddr = 0x1000;
    static constexpr lldb::addr_t kTestClassAddr = 0x2000;
    static constexpr lldb::addr_t kTestStringAddr = 0x3000;
    static constexpr lldb::addr_t kTestArrayAddr = 0x4000;
    static constexpr lldb::addr_t kTestDictAddr = 0x5000;
    
    // Performance thresholds (milliseconds)
    static constexpr double kMaxStringFormatterTime = 10.0;
    static constexpr double kMaxNumberFormatterTime = 5.0;
    static constexpr double kMaxArrayFormatterTime = 20.0;
    static constexpr double kMaxDictFormatterTime = 30.0;
    static constexpr double kMaxSetFormatterTime = 20.0;
    static constexpr double kMaxIndexSetFormatterTime = 15.0;
    static constexpr double kMaxDecimalFormatterTime = 10.0;
    static constexpr double kMaxCharSetFormatterTime = 10.0;
};

// Macro helpers for common test patterns
#define TEST_FORMATTER_PERFORMANCE(formatter_call, max_time) \
    do { \
        PerformanceTimer timer; \
        formatter_call; \
        timer.AssertUnder(max_time, #formatter_call); \
    } while(0)

#define TEST_NO_MEMORY_LEAK(test_body) \
    do { \
        StartLeakCheck(); \
        test_body; \
        AssertNoLeaks(); \
    } while(0)

#define EXPECT_SUMMARY_EQ(valobj, expected) \
    do { \
        const char* summary = valobj->GetSummaryAsCString(); \
        EXPECT_STREQ(summary, expected) << "Summary mismatch for " \
                                        << valobj->GetName().GetCString(); \
    } while(0)

#define EXPECT_CHILD_COUNT(valobj, count) \
    do { \
        size_t num_children = valobj->GetNumChildren(); \
        EXPECT_EQ(num_children, count) << "Child count mismatch for " \
                                       << valobj->GetName().GetCString(); \
    } while(0)

} // namespace test
} // namespace gnustep
} // namespace lldb_private

#endif // LLDB_UNITTESTS_LANGUAGE_OBJC_GNUSTEP_FORMATTERTEST_BASE_H
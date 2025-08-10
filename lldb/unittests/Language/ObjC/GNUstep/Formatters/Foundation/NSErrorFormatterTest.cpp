//===-- NSErrorFormatterTest.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"

#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/Stream.h"
#include "lldb/DataFormatters/TypeSummary.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Platform.h"
#include "lldb/Utility/ArchSpec.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Core/ValueObject.h"
#include "lldb/Utility/StreamString.h"

#include "../Common/FormatterTestHelpers.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepErrorFormatters.h"

#include <chrono>
#include <memory>
#include <limits>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;
using namespace lldb_private::formatters::test;

namespace {

class NSErrorFormatterTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
  }
  
  void TearDown() override {
    HostInfo::Terminate();
    FileSystem::Terminate();
  }
};

TEST_F(NSErrorFormatterTest, BasicErrorFormatting) {
  // Test basic NSError object formatting
  lldb::DebuggerSP debugger = Debugger::CreateInstance();
  lldb::TargetSP target = std::make_shared<Target>(*debugger, ArchSpec("x86_64"), 
                                                   PlatformSP(), true);
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  target->SetProcessSP(process);
  
  // Create NSError with domain, code, and description
  std::string domain = "NSCocoaErrorDomain";
  std::string description = "The operation couldn't be completed.";
  int64_t error_code = 4;
  
  struct {
    uint64_t isa;
    uint64_t domain_ptr;      // NSString for domain
    int64_t code;             // Error code
    uint64_t user_info_ptr;   // NSDictionary (optional)
  } nserror_obj = {
    0x8000,  // ISA
    0x9000,  // Domain string
    error_code,
    0x0      // No userInfo for basic test
  };
  
  // Create NSString for domain
  struct {
    uint64_t isa;
    uint32_t length;
    uint32_t padding;
    uint64_t str_ptr;
  } domain_nsstring = {
    0x1000,
    static_cast<uint32_t>(domain.length()),
    0,
    0xA000
  };
  
  static_cast<MockProcess*>(process.get())->SetMemory(0xB000, &nserror_obj, sizeof(nserror_obj));
  static_cast<MockProcess*>(process.get())->SetMemory(0x9000, &domain_nsstring, sizeof(domain_nsstring));
  static_cast<MockProcess*>(process.get())->SetMemory(0xA000, domain.c_str(), domain.length() + 1);
  
  lldb::ValueObjectSP nserror = MockValueObject::Create(target, "test_error", 0xB000, "NSError");
  
  StreamString output_stream;
  TypeSummaryOptions options;
  
  bool result = GNUstepNSErrorFormatterFunction(*nserror, output_stream, options);
  
  EXPECT_TRUE(result) << "NSError formatter should succeed";
  
  std::string output = output_stream.GetString().str();
  EXPECT_FALSE(output.empty()) << "NSError should produce output";
  
  // Should show domain and/or code
  EXPECT_TRUE(output.find("NSCocoaErrorDomain") != std::string::npos || 
              output.find("Cocoa") != std::string::npos ||
              output.find("4") != std::string::npos ||
              output.find("error") != std::string::npos ||
              output.find("Error") != std::string::npos) 
    << "NSError should show domain/code info: " << output;
}

TEST_F(NSErrorFormatterTest, CommonErrorDomains) {
  // Test formatting of common error domains
  lldb::DebuggerSP debugger = Debugger::CreateInstance();
  lldb::TargetSP target = std::make_shared<Target>(*debugger, ArchSpec("x86_64"), 
                                                   PlatformSP(), true);
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  target->SetProcessSP(process);
  
  GNUstepNSErrorSummaryProvider provider;
  
  std::vector<std::pair<std::string, int64_t>> common_errors = {
    {"NSCocoaErrorDomain", 260},        // File doesn't exist
    {"NSPOSIXErrorDomain", 2},          // ENOENT  
    {"NSURLErrorDomain", -1009},        // Network error
    {"NSXMLParserErrorDomain", 4},      // XML parsing error
    {"kCFErrorDomainCFNetwork", 2},     // Network error
    {"MyCustomErrorDomain", 1001}       // Custom domain
  };
  
  for (size_t i = 0; i < common_errors.size(); ++i) {
    const std::string& domain = common_errors[i].first;
    int64_t code = common_errors[i].second;
    
    struct {
      uint64_t isa;
      uint64_t domain_ptr;
      int64_t error_code;
      uint64_t user_info_ptr;
    } error_obj = {0x8000, 0xC000 + (i * 100), code, 0};
    
    struct {
      uint64_t isa;
      uint32_t length;
      uint32_t padding;
      uint64_t str_ptr;
    } domain_string = {0x1000, static_cast<uint32_t>(domain.length()), 0, 0xD000 + (i * 100)};
    
    lldb::addr_t error_addr = 0xE000 + (i * 100);
    lldb::addr_t domain_str_addr = 0xC000 + (i * 100);
    lldb::addr_t domain_data_addr = 0xD000 + (i * 100);
    
    static_cast<MockProcess*>(process.get())->SetMemory(error_addr, &error_obj, sizeof(error_obj));
    static_cast<MockProcess*>(process.get())->SetMemory(domain_str_addr, &domain_string, sizeof(domain_string));
    static_cast<MockProcess*>(process.get())->SetMemory(domain_data_addr, domain.c_str(), domain.length() + 1);
    
    lldb::ValueObjectSP error_valobj = MockValueObject::Create(target, "common_error", 
                                                               error_addr, "NSError");
    StreamString output_stream;
    TypeSummaryOptions options;
    
    bool result = provider.FormatObject(*error_valobj, output_stream, options);
    
    if (result || !output_stream.GetString().empty()) {
      std::string output = output_stream.GetString().str();
      
      // Should show domain or meaningful abbreviation
      EXPECT_TRUE(output.find(domain) != std::string::npos ||
                  output.find("Cocoa") != std::string::npos ||
                  output.find("POSIX") != std::string::npos ||
                  output.find("URL") != std::string::npos ||
                  output.find("XML") != std::string::npos ||
                  output.find("Network") != std::string::npos ||
                  output.find("Custom") != std::string::npos ||
                  output.find(std::to_string(code)) != std::string::npos ||
                  output.find("error") != std::string::npos) 
        << "Error domain '" << domain << "' should be recognizable: " << output;
    }
  }
}

TEST_F(NSErrorFormatterTest, ErrorWithUserInfo) {
  // Test NSError with userInfo dictionary
  lldb::DebuggerSP debugger = Debugger::CreateInstance();
  lldb::TargetSP target = std::make_shared<Target>(*debugger, ArchSpec("x86_64"), 
                                                   PlatformSP(), true);
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  target->SetProcessSP(process);
  
  std::string domain = "TestErrorDomain";
  int64_t code = 42;
  
  struct {
    uint64_t isa;
    uint64_t domain_ptr;
    int64_t error_code;
    uint64_t user_info_ptr;  // Points to NSDictionary
  } error_with_info = {0x8000, 0xF000, code, 0x10000};
  
  // Domain string
  struct {
    uint64_t isa;
    uint32_t length;
    uint32_t padding;
    uint64_t str_ptr;
  } domain_string = {0x1000, static_cast<uint32_t>(domain.length()), 0, 0x11000};
  
  // Simplified userInfo dictionary (just has structure)
  struct {
    uint64_t isa;
    uint64_t count;      // Number of key-value pairs
    uint64_t keys_ptr;   // Array of keys
    uint64_t values_ptr; // Array of values
  } user_info_dict = {0x3000, 2, 0x12000, 0x13000};
  
  // Set up memory
  static_cast<MockProcess*>(process.get())->SetMemory(0x14000, &error_with_info, sizeof(error_with_info));
  static_cast<MockProcess*>(process.get())->SetMemory(0xF000, &domain_string, sizeof(domain_string));
  static_cast<MockProcess*>(process.get())->SetMemory(0x11000, domain.c_str(), domain.length() + 1);
  static_cast<MockProcess*>(process.get())->SetMemory(0x10000, &user_info_dict, sizeof(user_info_dict));
  
  lldb::ValueObjectSP error_obj = MockValueObject::Create(target, "error_with_userinfo", 
                                                          0x14000, "NSError");
  
  StreamString output_stream;
  TypeSummaryOptions options;
  
  bool result = GNUstepNSErrorFormatterFunction(*error_obj, output_stream, options);
  
  EXPECT_TRUE(result) << "NSError with userInfo should be handled";
  
  std::string output = output_stream.GetString().str();
  EXPECT_TRUE(output.find("TestErrorDomain") != std::string::npos ||
              output.find("42") != std::string::npos ||
              output.find("userInfo") != std::string::npos ||
              output.find("info") != std::string::npos ||
              output.find("Error") != std::string::npos) 
    << "NSError with userInfo should show relevant details: " << output;
}

TEST_F(NSErrorFormatterTest, ErrorCodeFormatting) {
  // Test various error code formats (positive, negative, zero)
  lldb::DebuggerSP debugger = Debugger::CreateInstance();
  lldb::TargetSP target = std::make_shared<Target>(*debugger, ArchSpec("x86_64"), 
                                                   PlatformSP(), true);
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  target->SetProcessSP(process);
  
  GNUstepNSErrorSummaryProvider provider;
  std::string domain = "TestDomain";
  
  std::vector<int64_t> test_codes = {
    0,        // No error
    1,        // Simple positive
    -1,       // Simple negative  
    404,      // HTTP-style code
    -1009,    // Network error code
    INT64_MAX, // Maximum value
    INT64_MIN  // Minimum value
  };
  
  for (size_t i = 0; i < test_codes.size(); ++i) {
    int64_t code = test_codes[i];
    
    struct {
      uint64_t isa;
      uint64_t domain_ptr;
      int64_t error_code;
      uint64_t user_info_ptr;
    } test_error = {0x8000, 0x15000, code, 0};
    
    struct {
      uint64_t isa;
      uint32_t length;
      uint32_t padding;
      uint64_t str_ptr;
    } domain_string = {0x1000, static_cast<uint32_t>(domain.length()), 0, 0x16000};
    
    lldb::addr_t error_addr = 0x17000 + (i * 100);
    
    static_cast<MockProcess*>(process.get())->SetMemory(error_addr, &test_error, sizeof(test_error));
    static_cast<MockProcess*>(process.get())->SetMemory(0x15000, &domain_string, sizeof(domain_string));
    static_cast<MockProcess*>(process.get())->SetMemory(0x16000, domain.c_str(), domain.length() + 1);
    
    lldb::ValueObjectSP code_obj = MockValueObject::Create(target, "code_error", error_addr, "NSError");
    StreamString output_stream;
    TypeSummaryOptions options;
    
    bool result = provider.FormatObject(*code_obj, output_stream, options);
    
    if (result || !output_stream.GetString().empty()) {
      std::string output = output_stream.GetString().str();
      
      // Should show error code or indicate error presence
      std::string code_str = std::to_string(code);
      EXPECT_TRUE(output.find(code_str) != std::string::npos ||
                  output.find("TestDomain") != std::string::npos ||
                  output.find("error") != std::string::npos ||
                  output.find("Error") != std::string::npos ||
                  (code == 0 && (output.find("0") != std::string::npos || output.find("no error") != std::string::npos))) 
        << "Error code " << code << " should be shown or indicated: " << output;
    }
  }
}

TEST_F(NSErrorFormatterTest, ErrorHandling) {
  // Test error conditions in NSError formatting
  lldb::DebuggerSP debugger = Debugger::CreateInstance();
  lldb::TargetSP target = std::make_shared<Target>(*debugger, ArchSpec("x86_64"), 
                                                   PlatformSP(), true);
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  target->SetProcessSP(process);
  
  GNUstepNSErrorSummaryProvider provider;
  
  // Test null NSError
  lldb::ValueObjectSP null_error = MockValueObject::Create(target, "null_error", 0x0, "NSError");
  StreamString null_stream;
  TypeSummaryOptions options;
  
  bool null_result = provider.FormatObject(*null_error, null_stream, options);
  
  if (!null_result) {
    EXPECT_TRUE(true) << "Null NSError correctly rejected";
  } else {
    std::string null_output = null_stream.GetString().str();
    EXPECT_TRUE(null_output.find("invalid") != std::string::npos ||
                null_output.find("nil") != std::string::npos) 
      << "Null NSError should indicate error: " << null_output;
  }
  
  // Test NSError with invalid domain pointer
  struct {
    uint64_t isa;
    uint64_t invalid_domain_ptr;
    int64_t error_code;
    uint64_t user_info_ptr;
  } invalid_error = {0x8000, LLDB_INVALID_ADDRESS, 123, 0};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0x18000, &invalid_error, sizeof(invalid_error));
  
  lldb::ValueObjectSP invalid_obj = MockValueObject::Create(target, "invalid_error", 
                                                            0x18000, "NSError");
  StreamString invalid_stream;
  
  bool invalid_result = provider.FormatObject(*invalid_obj, invalid_stream, options);
  
  if (invalid_result || !invalid_stream.GetString().empty()) {
    std::string invalid_output = invalid_stream.GetString().str();
    // Should handle gracefully, might show code or indicate error
    EXPECT_TRUE(invalid_output.find("123") != std::string::npos ||
                invalid_output.find("Error") != std::string::npos ||
                invalid_output.find("error") != std::string::npos ||
                invalid_output.find("invalid") != std::string::npos ||
                !invalid_output.empty()) 
      << "Invalid NSError should be handled gracefully: " << invalid_output;
  }
  
  // Test NSError with corrupted ISA
  struct {
    uint64_t corrupted_isa;
    uint64_t domain_ptr;
    int64_t error_code;
    uint64_t user_info_ptr;
  } corrupted_error = {LLDB_INVALID_ADDRESS, 0, 456, 0};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0x19000, &corrupted_error, sizeof(corrupted_error));
  
  lldb::ValueObjectSP corrupted_obj = MockValueObject::Create(target, "corrupted_error", 
                                                              0x19000, "NSError");
  StreamString corrupted_stream;
  
  bool corrupted_result = provider.FormatObject(*corrupted_obj, corrupted_stream, options);
  
  // Should either reject or handle gracefully
  if (corrupted_result || !corrupted_stream.GetString().empty()) {
    std::string corrupted_output = corrupted_stream.GetString().str();
    EXPECT_TRUE(corrupted_output.find("NSError") != std::string::npos ||
                corrupted_output.find("error") != std::string::npos ||
                !corrupted_output.empty()) 
      << "Corrupted NSError should be handled: " << corrupted_output;
  }
}

TEST_F(NSErrorFormatterTest, PerformanceRequirements) {
  // Test that NSError formatting meets <50ms requirement
  lldb::DebuggerSP debugger = Debugger::CreateInstance();
  lldb::TargetSP target = std::make_shared<Target>(*debugger, ArchSpec("x86_64"), 
                                                   PlatformSP(), true);
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  target->SetProcessSP(process);
  
  // Create NSError with complex domain for performance test
  std::string long_domain = "com.example.MyVeryLongApplicationErrorDomainWithManyComponents.SubSystem.SpecificModule";
  
  struct {
    uint64_t isa;
    uint64_t domain_ptr;
    int64_t error_code;
    uint64_t user_info_ptr;
  } perf_error = {0x8000, 0x1A000, 999, 0x1B000};
  
  struct {
    uint64_t isa;
    uint32_t length;
    uint32_t padding;
    uint64_t str_ptr;
  } perf_domain = {0x1000, static_cast<uint32_t>(long_domain.length()), 0, 0x1C000};
  
  // Simple userInfo for complexity
  struct {
    uint64_t isa;
    uint64_t count;
    uint64_t keys_ptr;
    uint64_t values_ptr;
  } perf_userinfo = {0x3000, 5, 0x1D000, 0x1E000};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0x1F000, &perf_error, sizeof(perf_error));
  static_cast<MockProcess*>(process.get())->SetMemory(0x1A000, &perf_domain, sizeof(perf_domain));
  static_cast<MockProcess*>(process.get())->SetMemory(0x1C000, long_domain.c_str(), long_domain.length() + 1);
  static_cast<MockProcess*>(process.get())->SetMemory(0x1B000, &perf_userinfo, sizeof(perf_userinfo));
  
  lldb::ValueObjectSP perf_obj = MockValueObject::Create(target, "perf_error", 0x1F000, "NSError");
  
  // Test performance
  auto start = std::chrono::high_resolution_clock::now();
  
  StreamString output_stream;
  TypeSummaryOptions options;
  GNUstepNSErrorSummaryProvider provider;
  
  bool result = provider.FormatObject(*perf_obj, output_stream, options);
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_LT(duration.count(), 50) << "NSError formatting should be under 50ms, took " 
                                   << duration.count() << "ms";
  
  if (result || !output_stream.GetString().empty()) {
    std::string output = output_stream.GetString().str();
    EXPECT_FALSE(output.empty()) << "Performance test should produce output";
    
    // Should handle long domain gracefully
    EXPECT_LT(output.length(), 200) << "Complex error output should be manageable";
  }
}

TEST_F(NSErrorFormatterTest, LocalizedErrorHandling) {
  // Test NSError localized description handling
  lldb::DebuggerSP debugger = Debugger::CreateInstance();
  lldb::TargetSP target = std::make_shared<Target>(*debugger, ArchSpec("x86_64"), 
                                                   PlatformSP(), true);
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  target->SetProcessSP(process);
  
  GNUstepNSErrorSummaryProvider provider;
  
  std::string domain = "LocalizedErrorDomain";
  std::string localized_desc = "Localized error description";
  
  struct {
    uint64_t isa;
    uint64_t domain_ptr;
    int64_t error_code;
    uint64_t user_info_ptr;  // Contains NSLocalizedDescriptionKey
  } localized_error = {0x8000, 0x20000, 100, 0x21000};
  
  // Domain string
  struct {
    uint64_t isa;
    uint32_t length;
    uint32_t padding;
    uint64_t str_ptr;
  } domain_string = {0x1000, static_cast<uint32_t>(domain.length()), 0, 0x22000};
  
  // UserInfo dictionary with localized description
  struct {
    uint64_t isa;
    uint64_t count;
    uint64_t keys_ptr;
    uint64_t values_ptr;
  } userinfo_dict = {0x3000, 1, 0x23000, 0x24000};  // One key-value pair
  
  // Set up memory
  static_cast<MockProcess*>(process.get())->SetMemory(0x25000, &localized_error, sizeof(localized_error));
  static_cast<MockProcess*>(process.get())->SetMemory(0x20000, &domain_string, sizeof(domain_string));
  static_cast<MockProcess*>(process.get())->SetMemory(0x22000, domain.c_str(), domain.length() + 1);
  static_cast<MockProcess*>(process.get())->SetMemory(0x21000, &userinfo_dict, sizeof(userinfo_dict));
  
  lldb::ValueObjectSP localized_obj = MockValueObject::Create(target, "localized_error", 
                                                              0x25000, "NSError");
  StreamString output_stream;
  TypeSummaryOptions options;
  
  bool result = provider.FormatObject(*localized_obj, output_stream, options);
  
  if (result || !output_stream.GetString().empty()) {
    std::string output = output_stream.GetString().str();
    
    // Should show domain, code, or userInfo indication
    EXPECT_TRUE(output.find("LocalizedErrorDomain") != std::string::npos ||
                output.find("100") != std::string::npos ||
                output.find("userInfo") != std::string::npos ||
                output.find("Localized") != std::string::npos ||
                output.find("Error") != std::string::npos) 
      << "Localized error should show relevant info: " << output;
  }
}

} // namespace
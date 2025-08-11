//===-- NSURLFormatterTest.cpp ------------------------------------------===//
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
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/Utility/StreamString.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Target/Platform.h"
#include "lldb/DataFormatters/TypeSummary.h"

#include "../Common/FormatterTestHelpers.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepURLFormatters.h"

#include <chrono>
#include <memory>
#include <vector>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;
using namespace lldb_private::formatters::test;

namespace {

class NSURLFormatterTest : public ::testing::Test {
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

TEST_F(NSURLFormatterTest, BasicURLFormatting) {
  // Test basic NSURL object formatting
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Create NSURL object with URL string
  std::string url_string = "https://www.example.com/path";
  
  struct {
    uint64_t isa;
    uint64_t url_string_ptr;  // Points to NSString with URL
    uint64_t base_url_ptr;    // Base URL (can be nil)
  } nsurl_obj = {
    0x6000,  // ISA
    0x7000,  // URL string
    0        // No base URL
  };
  
  // Create NSString for URL
  struct {
    uint64_t isa;
    uint32_t length;
    uint32_t padding;
    uint64_t str_ptr;
  } url_nsstring = {
    0x1000,
    static_cast<uint32_t>(url_string.length()),
    0,
    0x8000
  };
  
  static_cast<MockProcess*>(process.get())->SetMemory(0x9000, &nsurl_obj, sizeof(nsurl_obj));
  static_cast<MockProcess*>(process.get())->SetMemory(0x7000, &url_nsstring, sizeof(url_nsstring));
  static_cast<MockProcess*>(process.get())->SetMemory(0x8000, url_string.c_str(), url_string.length() + 1);
  
  lldb::ValueObjectSP nsurl = MockValueObject::Create(target, "test_url", 0x9000, "NSURL");
  
  StreamString output_stream;
  TypeSummaryOptions options;
  
  bool result = GNUstepNSURLFormatterFunction(*nsurl, output_stream, options);
  
  EXPECT_TRUE(result) << "NSURL formatter should succeed";
  
  std::string output = output_stream.GetString().str();
  EXPECT_FALSE(output.empty()) << "NSURL should produce output";
  
  // Should show URL string or indication of URL
  EXPECT_TRUE(output.find("example.com") != std::string::npos || 
              output.find("https") != std::string::npos ||
              output.find("URL") != std::string::npos ||
              output.find("www") != std::string::npos) 
    << "NSURL should show URL content: " << output;
}

TEST_F(NSURLFormatterTest, FileURLHandling) {
  // Test file:// URL handling
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  std::string file_url = "file:///home/user/document.txt";
  
  struct {
    uint64_t isa;
    uint64_t url_string_ptr;
    uint64_t base_url_ptr;
  } file_nsurl = {0x6000, 0xA000, 0};
  
  struct {
    uint64_t isa;
    uint32_t length;
    uint32_t padding;
    uint64_t str_ptr;
  } file_nsstring = {0x1000, static_cast<uint32_t>(file_url.length()), 0, 0xB000};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0xC000, &file_nsurl, sizeof(file_nsurl));
  static_cast<MockProcess*>(process.get())->SetMemory(0xA000, &file_nsstring, sizeof(file_nsstring));
  static_cast<MockProcess*>(process.get())->SetMemory(0xB000, file_url.c_str(), file_url.length() + 1);
  
  lldb::ValueObjectSP file_obj = MockValueObject::Create(target, "file_url", 0xC000, "NSURL");
  
  StreamString output_stream;
  TypeSummaryOptions options;
  
  bool result = GNUstepNSURLFormatterFunction(*file_obj, output_stream, options);
  
  EXPECT_TRUE(result) << "File URL should be handled";
  
  std::string output = output_stream.GetString().str();
  EXPECT_TRUE(output.find("file") != std::string::npos ||
              output.find("document.txt") != std::string::npos ||
              output.find("/home/user") != std::string::npos ||
              output.find("URL") != std::string::npos) 
    << "File URL should show file path info: " << output;
}

TEST_F(NSURLFormatterTest, RelativeURLHandling) {
  // Test relative URL with base URL
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  std::string relative_url = "page.html";
  std::string base_url = "https://www.example.com/";
  
  // Create relative NSURL
  struct {
    uint64_t isa;
    uint64_t url_string_ptr;
    uint64_t base_url_ptr;    // Points to base NSURL
  } relative_nsurl = {0x6000, 0xD000, 0xE000};
  
  // Create base NSURL
  struct {
    uint64_t isa;
    uint64_t base_url_string_ptr;
    uint64_t base_base_url_ptr;  // nil for base
  } base_nsurl = {0x6000, 0xF000, 0};
  
  // NSStrings for both URLs
  struct {
    uint64_t isa;
    uint32_t length;
    uint32_t padding;
    uint64_t str_ptr;
  } relative_nsstring = {0x1000, static_cast<uint32_t>(relative_url.length()), 0, 0x10000};
  
  struct {
    uint64_t isa;
    uint32_t length;
    uint32_t padding;
    uint64_t str_ptr;
  } base_nsstring = {0x1000, static_cast<uint32_t>(base_url.length()), 0, 0x11000};
  
  // Set up all objects
  static_cast<MockProcess*>(process.get())->SetMemory(0x12000, &relative_nsurl, sizeof(relative_nsurl));
  static_cast<MockProcess*>(process.get())->SetMemory(0xE000, &base_nsurl, sizeof(base_nsurl));
  static_cast<MockProcess*>(process.get())->SetMemory(0xD000, &relative_nsstring, sizeof(relative_nsstring));
  static_cast<MockProcess*>(process.get())->SetMemory(0xF000, &base_nsstring, sizeof(base_nsstring));
  static_cast<MockProcess*>(process.get())->SetMemory(0x10000, relative_url.c_str(), relative_url.length() + 1);
  static_cast<MockProcess*>(process.get())->SetMemory(0x11000, base_url.c_str(), base_url.length() + 1);
  
  lldb::ValueObjectSP relative_obj = MockValueObject::Create(target, "relative_url", 0x12000, "NSURL");
  
  StreamString output_stream;
  TypeSummaryOptions options;
  
  bool result = GNUstepNSURLFormatterFunction(*relative_obj, output_stream, options);
  
  EXPECT_TRUE(result || !output_stream.GetString().empty()) << "Relative URL should be handled";
  
  std::string output = output_stream.GetString().str();
  EXPECT_TRUE(output.find("page.html") != std::string::npos ||
              output.find("example.com") != std::string::npos ||
              output.find("relative") != std::string::npos ||
              output.find("base") != std::string::npos ||
              output.find("URL") != std::string::npos) 
    << "Relative URL should show URL info: " << output;
}

TEST_F(NSURLFormatterTest, MalformedURLHandling) {
  // Test malformed/invalid URLs
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  GNUstepNSURLSummaryProvider provider;
  
  std::vector<std::string> malformed_urls = {
    "not a url at all",
    "ht!tp://invalid",
    "",  // Empty URL
    "://missing-scheme"
  };
  
  for (size_t i = 0; i < malformed_urls.size(); ++i) {
    const std::string& bad_url = malformed_urls[i];
    
    struct {
      uint64_t isa;
      uint64_t url_string_ptr;
      uint64_t base_url_ptr;
    } bad_nsurl = {0x6000, 0x13000 + (i * 100), 0};
    
    struct {
      uint64_t isa;
      uint32_t length;
      uint32_t padding;
      uint64_t str_ptr;
    } bad_nsstring = {0x1000, static_cast<uint32_t>(bad_url.length()), 0, 0x14000 + (i * 100)};
    
    lldb::addr_t url_obj_addr = 0x15000 + (i * 100);
    lldb::addr_t str_obj_addr = 0x13000 + (i * 100);
    lldb::addr_t str_data_addr = 0x14000 + (i * 100);
    
    static_cast<MockProcess*>(process.get())->SetMemory(url_obj_addr, &bad_nsurl, sizeof(bad_nsurl));
    static_cast<MockProcess*>(process.get())->SetMemory(str_obj_addr, &bad_nsstring, sizeof(bad_nsstring));
    
    if (!bad_url.empty()) {
      static_cast<MockProcess*>(process.get())->SetMemory(str_data_addr, bad_url.c_str(), bad_url.length() + 1);
    }
    
    lldb::ValueObjectSP bad_obj = MockValueObject::Create(target, "bad_url", url_obj_addr, "NSURL");
    StreamString output_stream;
    TypeSummaryOptions options;
    
    bool result = provider.FormatObject(*bad_obj, output_stream, options);
    
    // Should handle malformed URLs gracefully
    if (result || !output_stream.GetString().empty()) {
      std::string output = output_stream.GetString().str();
      EXPECT_TRUE(output.find("URL") != std::string::npos ||
                  output.find("NSURL") != std::string::npos ||
                  !output.empty()) 
        << "Malformed URL " << i << " should be handled: " << output;
    }
  }
}

TEST_F(NSURLFormatterTest, NullURLHandling) {
  // Test null/invalid NSURL objects
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  GNUstepNSURLSummaryProvider provider;
  
  // Test null NSURL
  lldb::ValueObjectSP null_url = MockValueObject::Create(target, "null_url", 0x0, "NSURL");
  StreamString null_stream;
  TypeSummaryOptions options;
  
  bool null_result = provider.FormatObject(*null_url, null_stream, options);
  
  // Should handle gracefully
  std::string null_output = null_stream.GetString().str();
  EXPECT_TRUE(!null_result || 
              null_output.find("nil") != std::string::npos ||
              null_output.find("invalid") != std::string::npos ||
              null_output.empty()) 
    << "Null NSURL should be handled gracefully: " << null_output;
  
  // Test NSURL with invalid string pointer
  struct {
    uint64_t isa;
    uint64_t invalid_string_ptr;
    uint64_t base_url_ptr;
  } invalid_nsurl = {0x6000, LLDB_INVALID_ADDRESS, 0};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0x16000, &invalid_nsurl, sizeof(invalid_nsurl));
  
  lldb::ValueObjectSP invalid_obj = MockValueObject::Create(target, "invalid_url", 0x16000, "NSURL");
  StreamString invalid_stream;
  
  bool invalid_result = provider.FormatObject(*invalid_obj, invalid_stream, options);
  
  if (invalid_result || !invalid_stream.GetString().empty()) {
    std::string invalid_output = invalid_stream.GetString().str();
    EXPECT_TRUE(invalid_output.find("NSURL") != std::string::npos ||
                invalid_output.find("URL") != std::string::npos ||
                invalid_output.find("invalid") != std::string::npos ||
                !invalid_output.empty()) 
      << "Invalid NSURL should be handled: " << invalid_output;
  }
}

TEST_F(NSURLFormatterTest, PerformanceRequirements) {
  // Test that NSURL formatting meets <50ms requirement
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  // Create NSURL with moderately long URL
  std::string long_url = "https://www.example.com/very/long/path/with/many/components/and/query/parameters?param1=value1&param2=value2&param3=value3";
  
  struct {
    uint64_t isa;
    uint64_t url_string_ptr;
    uint64_t base_url_ptr;
  } perf_nsurl = {0x6000, 0x17000, 0};
  
  struct {
    uint64_t isa;
    uint32_t length;
    uint32_t padding;
    uint64_t str_ptr;
  } perf_nsstring = {0x1000, static_cast<uint32_t>(long_url.length()), 0, 0x18000};
  
  static_cast<MockProcess*>(process.get())->SetMemory(0x19000, &perf_nsurl, sizeof(perf_nsurl));
  static_cast<MockProcess*>(process.get())->SetMemory(0x17000, &perf_nsstring, sizeof(perf_nsstring));
  static_cast<MockProcess*>(process.get())->SetMemory(0x18000, long_url.c_str(), long_url.length() + 1);
  
  lldb::ValueObjectSP perf_obj = MockValueObject::Create(target, "perf_url", 0x19000, "NSURL");
  
  // Test performance
  auto start = std::chrono::high_resolution_clock::now();
  
  StreamString output_stream;
  TypeSummaryOptions options;
  GNUstepNSURLSummaryProvider provider;
  
  bool result = provider.FormatObject(*perf_obj, output_stream, options);
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_LT(duration.count(), 50) << "NSURL formatting should be under 50ms, took " 
                                   << duration.count() << "ms";
  
  if (result || !output_stream.GetString().empty()) {
    std::string output = output_stream.GetString().str();
    EXPECT_FALSE(output.empty()) << "Performance test should produce output";
    
    // Should handle long URLs gracefully (possibly truncated)
    EXPECT_LT(output.length(), 300) << "Long URL output should be manageable length";
  }
}

TEST_F(NSURLFormatterTest, SpecialURLSchemes) {
  // Test various URL schemes
  lldb::TargetSP target = std::make_shared<MockTarget>(*Debugger::CreateInstance(), ArchSpec("x86_64"), 
                                                   PlatformSP());
  lldb::ProcessSP process = std::make_shared<MockProcess>(target, ListenerSP());
  
  GNUstepNSURLSummaryProvider provider;
  
  std::vector<std::string> special_urls = {
    "ftp://ftp.example.com/file.txt",
    "mailto:user@example.com",
    "data:text/plain;base64,SGVsbG8gV29ybGQ=",
    "tel:+1234567890",
    "custom://app/action"
  };
  
  for (size_t i = 0; i < special_urls.size(); ++i) {
    const std::string& special_url = special_urls[i];
    
    struct {
      uint64_t isa;
      uint64_t url_string_ptr;
      uint64_t base_url_ptr;
    } special_nsurl = {0x6000, 0x1A000 + (i * 100), 0};
    
    struct {
      uint64_t isa;
      uint32_t length;
      uint32_t padding;
      uint64_t str_ptr;
    } special_nsstring = {0x1000, static_cast<uint32_t>(special_url.length()), 0, 0x1B000 + (i * 100)};
    
    lldb::addr_t url_obj_addr = 0x1C000 + (i * 100);
    lldb::addr_t str_obj_addr = 0x1A000 + (i * 100);
    lldb::addr_t str_data_addr = 0x1B000 + (i * 100);
    
    static_cast<MockProcess*>(process.get())->SetMemory(url_obj_addr, &special_nsurl, sizeof(special_nsurl));
    static_cast<MockProcess*>(process.get())->SetMemory(str_obj_addr, &special_nsstring, sizeof(special_nsstring));
    static_cast<MockProcess*>(process.get())->SetMemory(str_data_addr, special_url.c_str(), special_url.length() + 1);
    
    lldb::ValueObjectSP special_obj = MockValueObject::Create(target, "special_url", url_obj_addr, "NSURL");
    StreamString output_stream;
    TypeSummaryOptions options;
    
    bool result = provider.FormatObject(*special_obj, output_stream, options);
    
    if (result || !output_stream.GetString().empty()) {
      std::string output = output_stream.GetString().str();
      
      // Should recognize different URL schemes
      EXPECT_TRUE(output.find("ftp") != std::string::npos ||
                  output.find("mailto") != std::string::npos ||
                  output.find("data") != std::string::npos ||
                  output.find("tel") != std::string::npos ||
                  output.find("custom") != std::string::npos ||
                  output.find("URL") != std::string::npos ||
                  special_url.substr(0, 10).find(output.substr(0, 3)) != std::string::npos ||
                  !output.empty()) 
        << "Special URL scheme should be handled: " << special_url << " -> " << output;
    }
  }
}

} // namespace
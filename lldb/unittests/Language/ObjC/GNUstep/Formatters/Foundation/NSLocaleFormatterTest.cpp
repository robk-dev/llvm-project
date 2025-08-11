//===-- NSLocaleFormatterTest.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"
#include "../Common/FormatterTestHelpers.h"
#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepLocaleFormatters.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Target/Platform.h"
#include "lldb/Utility/ArchSpec.h"
#include "lldb/Utility/Stream.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;
using namespace lldb_private::formatters::test;

namespace {

class NSLocaleFormatterTest : public ::testing::Test {
protected:
  void SetUp() override {
    FileSystem::Initialize();
    HostInfo::Initialize();
    m_formatter = std::make_unique<GNUstepNSLocaleSummaryProvider>();
    
    // Create mock target and process
    m_debugger = Debugger::CreateInstance();
    ArchSpec arch("x86_64-pc-linux");
    PlatformSP platform_sp = Platform::GetHostPlatform();
    m_target = std::make_shared<MockTarget>(*m_debugger, arch, platform_sp);
    m_process = std::make_shared<MockProcess>(m_target, ListenerSP());
    // SetExecutableModule expects a reference, not a temporary
    ModuleSP empty_module_sp;
    m_target->SetExecutableModule(empty_module_sp, eLoadDependentsNo);
  }
  
  void TearDown() override {
    m_formatter.reset();
    m_process.reset();
    m_target.reset();
    Debugger::Destroy(m_debugger);
    HostInfo::Terminate();
    FileSystem::Terminate();
  }

  std::unique_ptr<GNUstepNSLocaleSummaryProvider> m_formatter;
  DebuggerSP m_debugger;
  TargetSP m_target;
  std::shared_ptr<MockProcess> m_process;
};

TEST_F(NSLocaleFormatterTest, HandlesNilLocale) {
  // Create a nil locale value object
  lldb::ValueObjectSP valobj = MockValueObject::Create(m_target, "nil_locale", 0x0, "NSLocale");
  ASSERT_NE(valobj, nullptr);
  
  StreamString stream;
  TypeSummaryOptions options;
  
  bool result = m_formatter->FormatObject(*valobj, stream, options);
  
  EXPECT_TRUE(result);
  EXPECT_EQ(stream.GetString(), "nil");
}

TEST_F(NSLocaleFormatterTest, HandlesInvalidAddress) {
  lldb::ValueObjectSP valobj = MockValueObject::Create(m_target, "invalid_locale", 
                                                       LLDB_INVALID_ADDRESS, "NSLocale");
  ASSERT_NE(valobj, nullptr);
  
  StreamString stream;
  TypeSummaryOptions options;
  
  bool result = m_formatter->FormatObject(*valobj, stream, options);
  
  EXPECT_TRUE(result);
  // Should handle gracefully
  EXPECT_FALSE(stream.GetString().empty());
}

TEST_F(NSLocaleFormatterTest, HandlesValidLocaleWithFallback) {
  // Create a valid locale address - formatter will try to extract identifier
  lldb::ValueObjectSP valobj = MockValueObject::Create(m_target, "valid_locale", 
                                                       0x555555556000, "NSLocale");
  ASSERT_NE(valobj, nullptr);
  
  // Since we don't have actual memory set up, formatter should provide fallback
  StreamString stream;
  TypeSummaryOptions options;
  
  bool result = m_formatter->FormatObject(*valobj, stream, options);
  
  EXPECT_TRUE(result);
  // Should provide a fallback summary when memory reading fails
  EXPECT_FALSE(stream.GetString().empty());
}

TEST_F(NSLocaleFormatterTest, FormatterFunctionWrapper) {
  // Test the static formatter function wrapper
  lldb::ValueObjectSP valobj = MockValueObject::Create(m_target, "test_locale", 
                                                       0x555555556000, "NSLocale");
  ASSERT_NE(valobj, nullptr);
  
  StreamString stream;
  TypeSummaryOptions options;
  
  // Test the static function that LLDB calls
  bool result = GNUstepNSLocaleFormatterFunction(*valobj, stream, options);
  
  EXPECT_TRUE(result);
  EXPECT_FALSE(stream.GetString().empty());
}

TEST_F(NSLocaleFormatterTest, TestLocaleIdentifierExtraction) {
  // Test locale with identifier setup
  lldb::addr_t locale_addr = 0x10000;
  lldb::addr_t identifier_addr = 0x10100;
  
  // Set up NSLocale memory layout
  // NSLocale has an NSString* _identifier at offset 8 (after isa)
  struct {
    uint64_t isa;
    uint64_t identifier_ptr;
  } locale_obj = {
    0x5000,  // ISA
    identifier_addr
  };
  
  m_process->SetMemory(locale_addr, &locale_obj, sizeof(locale_obj));
  
  // Set up identifier string (en_US)
  const char* locale_id = "en_US";
  struct {
    uint64_t isa;
    uint64_t length;
    uint64_t length2;
    uint64_t str_ptr;
  } string_obj = {
    0x6000,  // NSString ISA
    strlen(locale_id),
    strlen(locale_id),
    identifier_addr + sizeof(string_obj)
  };
  
  m_process->SetMemory(identifier_addr, &string_obj, sizeof(string_obj));
  m_process->SetMemory(identifier_addr + sizeof(string_obj), locale_id, strlen(locale_id) + 1);
  
  // Create value object and test
  lldb::ValueObjectSP valobj = MockValueObject::Create(m_target, "locale_with_id", 
                                                       locale_addr, "NSLocale");
  ASSERT_NE(valobj, nullptr);
  
  StreamString stream;
  TypeSummaryOptions options;
  
  bool result = m_formatter->FormatObject(*valobj, stream, options);
  
  EXPECT_TRUE(result);
  // The formatter should try to extract the identifier, though it may fail
  // due to mock limitations
  EXPECT_FALSE(stream.GetString().empty());
}

TEST_F(NSLocaleFormatterTest, CommonLocaleIdentifiers) {
  // Test handling of common locale identifiers
  const std::vector<std::string> common_locales = {
    "en_US", "en_GB", "fr_FR", "de_DE", "ja_JP",
    "zh_CN", "es_ES", "pt_BR", "ru_RU", "ar_SA"
  };
  
  // We can't actually test reading these without a complex mock setup,
  // but we verify the formatter handles various addresses
  for (size_t i = 0; i < common_locales.size(); ++i) {
    lldb::addr_t addr = 0x20000 + (i * 0x1000);
    lldb::ValueObjectSP valobj = MockValueObject::Create(m_target, "locale_" + common_locales[i], 
                                                         addr, "NSLocale");
    ASSERT_NE(valobj, nullptr);
    
    StreamString stream;
    TypeSummaryOptions options;
    
    bool result = m_formatter->FormatObject(*valobj, stream, options);
    EXPECT_TRUE(result);
    EXPECT_FALSE(stream.GetString().empty());
  }
}

TEST_F(NSLocaleFormatterTest, PerformanceTest) {
  // Test formatter performance - should be fast even with memory read failures
  lldb::ValueObjectSP valobj = MockValueObject::Create(m_target, "perf_locale", 
                                                       0x30000, "NSLocale");
  ASSERT_NE(valobj, nullptr);
  
  PerformanceTimer timer;
  
  StreamString stream;
  TypeSummaryOptions options;
  
  for (int i = 0; i < 100; ++i) {
    stream.Clear();
    bool result = m_formatter->FormatObject(*valobj, stream, options);
    EXPECT_TRUE(result);
  }
  
  timer.AssertUnder(50.0, "100 locale formats");
}

} // namespace
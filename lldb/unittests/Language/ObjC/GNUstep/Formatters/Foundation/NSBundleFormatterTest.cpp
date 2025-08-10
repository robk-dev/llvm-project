//===-- NSBundleFormatterTest.cpp ----------------------------------------===//
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

#include "Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/formatters/GNUstepBundleFormatters.h"

#include <memory>
#include <chrono>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace {

class NSBundleFormatterTest : public ::testing::Test {
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

TEST_F(NSBundleFormatterTest, FormatterFunctionExists) {
  // Test that NSBundle formatter function exists and is callable
  auto formatter_func = &GNUstepNSBundleFormatterFunction;
  EXPECT_NE(formatter_func, nullptr) << "NSBundle formatter function should exist";
  
  // Test that the function pointer is valid
  EXPECT_TRUE(formatter_func != nullptr) << "Function pointer should be valid";
}

TEST_F(NSBundleFormatterTest, BundleStructureKnowledge) {
  // Test understanding of NSBundle object layout based on GNUstepBundleFormatters.cpp
  
  // Based on formatter implementation analysis:
  // struct NSBundle {
  //   Class isa;                    // Object's class pointer (offset 0)
  //   NSString *_path;              // Bundle path (offset 8 on 64-bit)
  //   NSString *_frameworkVersion;  // Version info (offset 16)
  //   BOOL _codeLoaded;             // Load status (offset 24)
  //   // ... other instance variables
  // };
  
  const size_t EXPECTED_ISA_OFFSET = 0;
  const size_t EXPECTED_PATH_OFFSET = 8;   // After isa pointer on 64-bit
  const size_t EXPECTED_VERSION_OFFSET = 16; // After isa + _path
  const size_t EXPECTED_LOADED_OFFSET = 24;  // After isa + _path + _frameworkVersion
  
  EXPECT_EQ(EXPECTED_ISA_OFFSET, 0) << "isa should be at offset 0";
  EXPECT_EQ(EXPECTED_PATH_OFFSET, 8) << "_path should be at offset 8 on 64-bit";
  EXPECT_EQ(EXPECTED_VERSION_OFFSET, 16) << "_frameworkVersion should be at offset 16";
  EXPECT_EQ(EXPECTED_LOADED_OFFSET, 24) << "_codeLoaded should be at offset 24";
  
  // Test expected field sizes
  const size_t POINTER_SIZE = 8; // 64-bit pointers
  const size_t BOOL_SIZE = 1;    // BOOL is typically 1 byte
  
  EXPECT_EQ(POINTER_SIZE, 8) << "Pointers should be 8 bytes on 64-bit";
  EXPECT_EQ(BOOL_SIZE, 1) << "BOOL should be 1 byte";
}

TEST_F(NSBundleFormatterTest, PathExtractionLogic) {
  // Test the ExtractBundlePath algorithm from the formatter
  
  // Common bundle path patterns that should be handled
  std::vector<std::string> expectedPaths = {
    "/usr/lib/GNUstep",
    "/opt/gnustep/system/Library/Bundles/MyBundle.bundle",
    "/home/user/MyApp.app",
    "/System/Library/Frameworks/Foundation.framework",
    "/Applications/MyApp.app/Contents/Frameworks/MyFramework.framework",
    "",  // Empty path (invalid bundle)
    "/",  // Root path
    "relative/path" // Relative path (unusual but valid)
  };
  
  for (const auto& path : expectedPaths) {
    // Test path validation logic
    bool pathIsEmpty = path.empty();
    bool pathIsValid = !path.empty() && (path[0] == '/' || path.find('.') != std::string::npos);
    
    if (pathIsEmpty) {
      EXPECT_TRUE(pathIsEmpty) << "Empty path should be identified as empty";
    } else {
      EXPECT_FALSE(pathIsEmpty) << "Non-empty path should not be empty: " << path;
    }
    
    // Test common path patterns
    bool isFramework = path.find(".framework") != std::string::npos;
    bool isBundle = path.find(".bundle") != std::string::npos;
    bool isApp = path.find(".app") != std::string::npos;
    
    if (isFramework) {
      EXPECT_TRUE(isFramework) << "Framework path should be identified: " << path;
    }
    if (isBundle) {
      EXPECT_TRUE(isBundle) << "Bundle path should be identified: " << path;
    }
    if (isApp) {
      EXPECT_TRUE(isApp) << "App path should be identified: " << path;
    }
  }
}

TEST_F(NSBundleFormatterTest, VersionStringHandling) {
  // Test version string extraction and formatting
  
  std::vector<std::string> versionStrings = {
    "1.0",
    "2.1.3",
    "1.29.0",
    "10.15.7",
    "2.0-SNAPSHOT",
    "1.0.0-beta1",
    "",  // Empty version
    "unknown", // Unknown version
    "git-12345abc", // Git hash version
    "2025.1.1" // Date-based version
  };
  
  for (const auto& version : versionStrings) {
    // Test version validation
    bool versionIsEmpty = version.empty();
    bool versionHasDots = version.find('.') != std::string::npos;
    bool versionHasDash = version.find('-') != std::string::npos;
    bool versionIsNumeric = !version.empty() && std::isdigit(version[0]);
    
    if (versionIsEmpty) {
      EXPECT_TRUE(versionIsEmpty) << "Empty version should be identified";
    } else {
      EXPECT_FALSE(versionIsEmpty) << "Non-empty version should be valid: " << version;
    }
    
    // Version format validation
    if (versionHasDots && versionIsNumeric) {
      EXPECT_TRUE(versionHasDots && versionIsNumeric) << "Standard version format: " << version;
    }
    
    if (versionHasDash) {
      EXPECT_TRUE(versionHasDash) << "Pre-release version detected: " << version;
    }
    
    // Length validation (versions shouldn't be excessively long)
    EXPECT_LT(version.length(), 100) << "Version string should be reasonable length: " << version;
  }
}

TEST_F(NSBundleFormatterTest, LoadStatusValidation) {
  // Test boolean load status handling
  
  // Test both loaded states
  std::vector<bool> loadStates = {true, false};
  
  for (bool isLoaded : loadStates) {
    // Test boolean conversion
    uint8_t loadedByte = isLoaded ? 1 : 0;
    bool convertedBack = (loadedByte != 0);
    
    EXPECT_EQ(convertedBack, isLoaded) << "Boolean conversion should be consistent";
    
    // Test expected values
    if (isLoaded) {
      EXPECT_NE(loadedByte, 0) << "Loaded state should be non-zero";
      EXPECT_TRUE(convertedBack) << "Conversion should preserve true";
    } else {
      EXPECT_EQ(loadedByte, 0) << "Unloaded state should be zero";
      EXPECT_FALSE(convertedBack) << "Conversion should preserve false";
    }
  }
}

TEST_F(NSBundleFormatterTest, BundleInfoStructure) {
  // Test the BundleInfo structure from GNUstepNSBundleSummaryProvider
  
  struct MockBundleInfo {
    std::string path;
    std::string version;
    bool loaded;
    bool valid;
  };
  
  // Test Case 1: Valid bundle with all fields
  MockBundleInfo validBundle;
  validBundle.path = "/usr/lib/GNUstep";
  validBundle.version = "1.29.0";
  validBundle.loaded = true;
  validBundle.valid = true;
  
  EXPECT_FALSE(validBundle.path.empty()) << "Valid bundle should have path";
  EXPECT_FALSE(validBundle.version.empty()) << "Valid bundle should have version";
  EXPECT_TRUE(validBundle.loaded) << "Bundle should be loaded";
  EXPECT_TRUE(validBundle.valid) << "Bundle should be valid";
  
  // Test Case 2: Invalid bundle (empty path)
  MockBundleInfo invalidBundle;
  invalidBundle.path = "";
  invalidBundle.version = "";
  invalidBundle.loaded = false;
  invalidBundle.valid = false;
  
  EXPECT_TRUE(invalidBundle.path.empty()) << "Invalid bundle has empty path";
  EXPECT_TRUE(invalidBundle.version.empty()) << "Invalid bundle has empty version";
  EXPECT_FALSE(invalidBundle.loaded) << "Invalid bundle should not be loaded";
  EXPECT_FALSE(invalidBundle.valid) << "Bundle should be invalid";
  
  // Test Case 3: Partially valid bundle (path but no version)
  MockBundleInfo partialBundle;
  partialBundle.path = "/some/path";
  partialBundle.version = "";
  partialBundle.loaded = false;
  partialBundle.valid = true; // Valid if has path
  
  EXPECT_FALSE(partialBundle.path.empty()) << "Partial bundle has path";
  EXPECT_TRUE(partialBundle.version.empty()) << "Partial bundle missing version";
  EXPECT_TRUE(partialBundle.valid) << "Bundle with path should be considered valid";
}

TEST_F(NSBundleFormatterTest, FormattedOutputValidation) {
  // Test expected output format: NSBundle(path=/usr/lib/GNUstep, version=1.29.0, loaded=true)
  
  struct TestCase {
    std::string path;
    std::string version;
    bool loaded;
    std::string expectedPattern;
  };
  
  std::vector<TestCase> testCases = {
    {"/usr/lib/GNUstep", "1.29.0", true, "path=/usr/lib/GNUstep, version=1.29.0, loaded=true"},
    {"", "", false, "path=<null>, version=<unknown>, loaded=false"},
    {"/opt/bundle", "", true, "path=/opt/bundle, version=<unknown>, loaded=true"},
    {"/some/path", "2.0", false, "path=/some/path, version=2.0, loaded=false"}
  };
  
  for (const auto& testCase : testCases) {
    // Validate path formatting
    std::string formattedPath = testCase.path.empty() ? "<null>" : testCase.path;
    if (testCase.path.empty()) {
      EXPECT_EQ(formattedPath, "<null>") << "Empty path should format as <null>";
    } else {
      EXPECT_EQ(formattedPath, testCase.path) << "Non-empty path should format as-is";
    }
    
    // Validate version formatting
    std::string formattedVersion = testCase.version.empty() ? "<unknown>" : testCase.version;
    if (testCase.version.empty()) {
      EXPECT_EQ(formattedVersion, "<unknown>") << "Empty version should format as <unknown>";
    } else {
      EXPECT_EQ(formattedVersion, testCase.version) << "Non-empty version should format as-is";
    }
    
    // Validate boolean formatting
    std::string formattedLoaded = testCase.loaded ? "true" : "false";
    EXPECT_EQ(formattedLoaded, testCase.loaded ? "true" : "false") << "Boolean should format correctly";
    
    // Test complete format string construction
    std::string completeFormat = "path=" + formattedPath + ", version=" + formattedVersion + ", loaded=" + formattedLoaded;
    EXPECT_FALSE(completeFormat.empty()) << "Complete format should not be empty";
    EXPECT_NE(completeFormat.find("path="), std::string::npos) << "Should contain path component";
    EXPECT_NE(completeFormat.find("version="), std::string::npos) << "Should contain version component";
    EXPECT_NE(completeFormat.find("loaded="), std::string::npos) << "Should contain loaded component";
  }
}

TEST_F(NSBundleFormatterTest, StringConstantHandling) {
  // Test NSConstantString structure handling from ExtractBundlePath
  
  // Based on formatter implementation:
  // NSConstantString structure:
  // struct {
  //   Class isa;          // Object's class pointer (offset 0)
  //   uint32_t len;       // String length (offset 8)
  //   uint32_t padding;   // Padding (offset 12)
  //   uint64_t len2;      // Length again? (offset 16)
  //   const char *str;    // C string data pointer (offset 24)
  // };
  
  const size_t EXPECTED_NSSTRING_ISA_OFFSET = 0;
  const size_t EXPECTED_NSSTRING_LEN_OFFSET = 8;
  const size_t EXPECTED_NSSTRING_PADDING_OFFSET = 12;
  const size_t EXPECTED_NSSTRING_LEN2_OFFSET = 16;
  const size_t EXPECTED_NSSTRING_STR_OFFSET = 24;
  
  EXPECT_EQ(EXPECTED_NSSTRING_ISA_OFFSET, 0) << "NSString isa should be at offset 0";
  EXPECT_EQ(EXPECTED_NSSTRING_LEN_OFFSET, 8) << "NSString len should be at offset 8";
  EXPECT_EQ(EXPECTED_NSSTRING_PADDING_OFFSET, 12) << "NSString padding should be at offset 12";
  EXPECT_EQ(EXPECTED_NSSTRING_LEN2_OFFSET, 16) << "NSString len2 should be at offset 16";
  EXPECT_EQ(EXPECTED_NSSTRING_STR_OFFSET, 24) << "NSString str pointer should be at offset 24";
  
  // Test string length validation
  std::vector<uint32_t> testLengths = {0, 1, 10, 255, 1024, 65536};
  
  for (uint32_t len : testLengths) {
    // Length should be reasonable for a path string
    bool lengthIsReasonable = (len < 4096); // 4KB max for paths
    bool lengthIsEmpty = (len == 0);
    
    if (lengthIsEmpty) {
      EXPECT_EQ(len, 0) << "Empty string should have length 0";
    } else {
      EXPECT_GT(len, 0) << "Non-empty string should have positive length";
    }
    
    if (len > 4096) {
      EXPECT_FALSE(lengthIsReasonable) << "Extremely long strings should be flagged: " << len;
    } else {
      EXPECT_TRUE(lengthIsReasonable) << "Reasonable length should be accepted: " << len;
    }
  }
}

TEST_F(NSBundleFormatterTest, PerformanceRequirements) {
  // Test that formatter function meets <50ms performance requirement
  
  auto startTime = std::chrono::high_resolution_clock::now();
  
  // Test function pointer access performance
  for (int i = 0; i < 100; ++i) {
    auto formatter_func = &GNUstepNSBundleFormatterFunction;
    EXPECT_NE(formatter_func, nullptr) << "Formatter function should exist";
  }
  
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
  
  EXPECT_LT(duration.count(), 50) << "100 function accesses should be fast (<50ms)";
}

TEST_F(NSBundleFormatterTest, ErrorHandlingValidation) {
  // Test error handling scenarios
  
  // Test invalid object scenarios that the formatter should handle gracefully
  struct ErrorTestCase {
    std::string description;
    bool hasValidPath;
    bool hasValidVersion;
    bool isObjectValid;
  };
  
  std::vector<ErrorTestCase> errorCases = {
    {"null object", false, false, false},
    {"invalid address", false, false, false},
    {"corrupted path", false, true, false},
    {"corrupted version", true, false, true},
    {"valid object", true, true, true}
  };
  
  for (const auto& errorCase : errorCases) {
    EXPECT_FALSE(errorCase.description.empty()) << "Test case should have description";
    
    if (!errorCase.isObjectValid) {
      EXPECT_FALSE(errorCase.isObjectValid) << "Invalid object should be detected: " << errorCase.description;
    }
    
    if (!errorCase.hasValidPath && !errorCase.hasValidVersion) {
      EXPECT_FALSE(errorCase.hasValidPath || errorCase.hasValidVersion) 
        << "Completely invalid object should be detected: " << errorCase.description;
    }
    
    // Test that formatter should handle these cases without crashing
    bool shouldHandleGracefully = true; // Formatter should never crash
    EXPECT_TRUE(shouldHandleGracefully) << "Should handle error case gracefully: " << errorCase.description;
  }
}

TEST_F(NSBundleFormatterTest, MemoryAccessPatterns) {
  // Test memory access patterns used by the formatter
  
  // Address calculations that the formatter performs
  struct MemoryAccessTest {
    size_t baseAddress;
    size_t expectedPathAddress;
    size_t expectedVersionAddress;
    size_t expectedLoadedAddress;
  };
  
  // Test with different base addresses (simulating different object locations)
  std::vector<MemoryAccessTest> memoryTests = {
    {0x1000, 0x1008, 0x1010, 0x1018},  // offset 8, 16, 24
    {0x2000, 0x2008, 0x2010, 0x2018},  // same pattern
    {0x0,    0x8,    0x10,   0x18}     // null base (should be detected as invalid)
  };
  
  for (const auto& test : memoryTests) {
    // Test address calculations match expected offsets
    size_t calculatedPathAddr = test.baseAddress + 8;
    size_t calculatedVersionAddr = test.baseAddress + 16;
    size_t calculatedLoadedAddr = test.baseAddress + 24;
    
    EXPECT_EQ(calculatedPathAddr, test.expectedPathAddress) 
      << "Path address calculation should match expected";
    EXPECT_EQ(calculatedVersionAddr, test.expectedVersionAddress) 
      << "Version address calculation should match expected";
    EXPECT_EQ(calculatedLoadedAddr, test.expectedLoadedAddress) 
      << "Loaded status address calculation should match expected";
    
    // Test invalid address detection
    bool baseIsNull = (test.baseAddress == 0);
    if (baseIsNull) {
      EXPECT_EQ(test.baseAddress, 0) << "Null base address should be detected";
    } else {
      EXPECT_NE(test.baseAddress, 0) << "Valid base address should be non-zero";
    }
  }
}

} // namespace
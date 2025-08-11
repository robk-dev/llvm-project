add_lldb_unittest(LanguageObjCGNUstepTests
  Core/GNUstepTaggedPointerTest.cpp
  Core/GNUstepIntrospectorTest.cpp
  Core/GNUstepRuntimeTest.cpp
  Core/GNUstepRuntimeAPITest.cpp  # Re-enabled with mock implementation
  Core/GNUstepDeclVendorTest.cpp
  Core/GNUstepClassDescriptorTest.cpp      # New comprehensive class descriptor tests
  Core/GNUstepDynamicTypeSimpleTest.cpp     # New dynamic type resolution tests - simplified approach working
  Core/GNUstepObjectDescriptionSimpleTest.cpp # New object description tests - simplified approach working
  Core/GNUstepMemoryManagementTest.cpp     # New memory management (CallObjCCopyClassList fix) tests
  Core/GNUstepRuntimeSymbolLoadingTest.cpp # New runtime symbol loading (LoadRuntimeSymbols) tests
  Core/GNUstepStepThroughTrampolineTest.cpp # New step-through trampoline (GetStepThroughTrampolinePlan) tests
  Core/ISADescriptorMappingTest.cpp        # New ISA-to-descriptor mapping tests
  
  # Formatter tests with comprehensive validation - ALL PLACEHOLDERS REPLACED
  Formatters/Collections/NSArrayFormatterTest.cpp
  Formatters/Collections/NSDictionaryFormatterTest.cpp
  Formatters/Collections/NSSetFormatterTest.cpp
  Formatters/Collections/NSIndexSetFormatterTest.cpp
  Formatters/Collections/NSOrderedSetFormatterTest.cpp
  Formatters/Primitives/NSDecimalNumberFormatterTest.cpp  # Now has real validation tests
  Formatters/Text/NSCharacterSetFormatterTest.cpp         # Now has real character set tests
  Formatters/Foundation/NSBundleFormatterTest.cpp         # New comprehensive Bundle formatter test
  
  # Foundation Formatter Tests - Temporarily disabled while fixing compilation issues
  Formatters/Foundation/NSValueFormatterTest.cpp          # IdDispatcher routing & generic formatter tests - ENABLED FOR TESTING
  Formatters/Foundation/NSJSONSerializationFormatterTest.cpp  # JSON formatting & options tests - ENABLED FOR TESTING
  Formatters/Foundation/NSProxyFormatterTest.cpp          # Proxy type detection & target identification tests - ENABLED FOR TESTING
  
  # New Comprehensive Foundation Formatter Tests - Fixed compilation issues and re-enabled
  Formatters/Foundation/NSDataFormatterTest.cpp           # Data length, preview, & performance tests - ENABLED FOR TESTING
  Formatters/Foundation/NSURLFormatterTest.cpp            # URL string extraction & scheme handling tests - ENABLED FOR TESTING
  Formatters/Foundation/NSUUIDFormatterTest.cpp           # UUID formatting & special value tests - RE-ENABLED
  Formatters/Foundation/NSErrorFormatterTest.cpp          # Error domain, code, & userInfo tests - RE-ENABLED
  Formatters/Foundation/NSDateFormatterTest.cpp           # Date interval & timezone handling tests - RE-ENABLED
  Formatters/Foundation/NSLocaleFormatterTest.cpp         # Locale identifier & property extraction tests
  
  # Integration tests - Disabled: requires liblldb linkage (SB API)
  # Integration/EndToEndFormatterTest.cpp                   # Requires SB API - temporarily disabled

  PARTIAL_SOURCES_INTENDED

  LINK_LIBS
    lldbPluginGNUstepObjCRuntime
    lldbPluginObjCLanguage
    lldbPluginPlatformLinux
    lldbTarget
    lldbUtility
    lldbCore
    lldbDataFormatters
    lldbSymbol
    lldbHost
)

# Integration tests that require full LLDB API (SB API)
add_lldb_unittest(LanguageObjCGNUstepIntegrationTests
  Integration/DynamicTypeIntegrationTest.cpp  # Dynamic type resolution integration tests
  
  PARTIAL_SOURCES_INTENDED

  LINK_LIBS
    lldbPluginGNUstepObjCRuntime
    liblldb
    lldbAPI
    lldbCore
    lldbHost
    lldbInterpreter
)
# Minimal unit tests for GNUstep LLDB Plugin
# Tests only components that don't require runtime

add_lldb_unittest(LanguageObjCGNUstepTests
  GNUstepUtilitiesTest.cpp  # Tests for helper utilities that don't need runtime

  PARTIAL_SOURCES_INTENDED

  LINK_LIBS
    lldbPluginGNUstepObjCRuntime
    lldbPluginObjCLanguage
    lldbPluginPlatformLinux
    lldbTarget
    lldbUtility
    lldbUtilityHelpers
    lldbCore
    lldbDataFormatters
    lldbSymbol
    lldbHost
)
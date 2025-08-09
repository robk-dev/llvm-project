add_lldb_unittest(LanguageObjCGNUstepTests
  GNUstepIntrospectorTest.cpp
  GNUstepFormattersTest.cpp
  GNUstepTaggedPointerTest.cpp

  LINK_LIBS
    lldbPluginGNUstepObjCRuntime
    lldbPluginObjCLanguage
    lldbTarget
    lldbUtility
    lldbCore
    lldbDataFormatters
)
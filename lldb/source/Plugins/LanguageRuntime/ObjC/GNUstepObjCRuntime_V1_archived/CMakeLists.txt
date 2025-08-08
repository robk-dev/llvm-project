add_lldb_library(lldbPluginGNUstepObjCRuntime PLUGIN
  GNUstepObjCRuntime.cpp
  GNUstepObjCRuntimeClean.cpp
  GNUstepObjCDeclVendor.cpp
  GNUstepRuntimeAPI.cpp
  GNUstepSyntheticProvider.cpp
  GNUstepStringSummaryProvider.cpp
  GNUstepNSString.cpp
  GNUstepNumberSummaryProvider.cpp
  GNUstepNSDate.cpp
  GNUstepArraySyntheticProvider.cpp
  GNUstepArraySummaryProvider.cpp
  GNUstepNSArray.cpp
  GNUstepNSSet.cpp
  GNUstepNSDictionary.cpp
  GNUstepUniversalProvider.cpp
  GNUstepCustomClass.cpp
  GNUstepUtilities.cpp
  GNUstepCollectionUtilities.cpp
  ISAResolver.cpp
  RuntimeIntrospector.cpp

  LINK_LIBS
    lldbCore
    lldbDataFormatters
    lldbExpression
    lldbHost
    lldbSymbol
    lldbTarget
    lldbUtility
    lldbPluginTypeSystemClang
    lldbPluginObjCRuntime

  LINK_COMPONENTS
    Support
  )
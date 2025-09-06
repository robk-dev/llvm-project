add_lldb_library(lldbPluginGNUstepObjCRuntime PLUGIN
  GNUstepObjCRuntime.cpp
  GNUstepObjCRuntimeIntrospector.cpp
  GNUstepObjCRuntimeUtilities.cpp
  GNUstepObjCDeclVendor.cpp
  GNUstepClassDescriptorV2.cpp
  formatters/GNUstepUniversalFormatter.cpp

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
    lldbPluginClangCommon

  LINK_COMPONENTS
    Support
  )

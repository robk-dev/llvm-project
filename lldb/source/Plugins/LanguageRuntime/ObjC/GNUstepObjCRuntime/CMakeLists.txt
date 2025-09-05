add_lldb_library(lldbPluginGNUstepObjCRuntime PLUGIN
  GNUstepObjCRuntime.cpp
  GNUstepObjCRuntimeIntrospector.cpp
  GNUstepObjCRuntimeUtilities.cpp
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

  LINK_COMPONENTS
    Support
  )

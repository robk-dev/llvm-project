add_lldb_library(lldbPluginGNUstepObjCRuntime PLUGIN
  GNUstepObjCRuntime.cpp
  GNUstepClassDescriptor.cpp
  GNUstepObjCRuntimeIntrospector.cpp
  GNUstepObjCDeclVendor.cpp
  GNUstepRuntimeV2API.cpp
  formatters/GNUstepIdDispatcher.cpp
  formatters/GNUstepFormattersRegistry.cpp

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

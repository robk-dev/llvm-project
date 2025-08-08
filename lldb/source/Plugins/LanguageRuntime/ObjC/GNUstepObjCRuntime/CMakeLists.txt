add_lldb_library(lldbPluginGNUstepObjCRuntime PLUGIN
  GNUstepObjCRuntime.cpp
  GNUstepClassDescriptor.cpp
  GNUstepObjCRuntimeIntrospector.cpp
  GNUstepObjCDeclVendor.cpp
  GNUstepRuntimeV2API.cpp
  GNUstepFormatters.cpp
  formatters/GNUstepFormattersBase.cpp
  formatters/GNUstepIdDispatcher.cpp
  formatters/GNUstepStringFormatters.cpp
  formatters/GNUstepNumberFormatters.cpp
  formatters/GNUstepArrayFormatters.cpp
  formatters/GNUstepDictionaryFormatters.cpp
  formatters/GNUstepSetFormatters.cpp
  formatters/GNUstepDateFormatters.cpp
  formatters/GNUstepURLFormatters.cpp
  formatters/GNUstepErrorFormatters.cpp
  formatters/GNUstepDataFormatters.cpp
  formatters/GNUstepUUIDFormatters.cpp
  formatters/GNUstepGenericFormatter.cpp
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

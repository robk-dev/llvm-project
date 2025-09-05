add_lldb_library(lldbPluginGNUstepObjCRuntime PLUGIN
  GNUstepObjCRuntime.cpp
  GNUstepObjCRuntimeIntrospector.cpp
  GNUstepObjCRuntimeUtilities.cpp
  formatters/GNUstepFormattersBase.cpp
  formatters/GNUstepIdDispatcher.cpp
  formatters/GNUstepStringFormatters.cpp
  formatters/GNUstepNumberFormatters.cpp
  formatters/GNUstepArrayFormatters.cpp
  formatters/GNUstepDictionaryFormatters.cpp
  formatters/GNUstepSetFormatters.cpp
  formatters/GNUstepGenericFormatter.cpp

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

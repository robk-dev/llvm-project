add_lldb_unittest(LanguageObjCTests
  ObjCLanguageTest.cpp

  LINK_LIBS
    lldbPluginObjCLanguage
)

add_subdirectory(GNUstep)

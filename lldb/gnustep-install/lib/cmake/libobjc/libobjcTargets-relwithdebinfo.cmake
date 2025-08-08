#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "objc" for configuration "RelWithDebInfo"
set_property(TARGET objc APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(objc PROPERTIES
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib/libobjc.so.4.6"
  IMPORTED_SONAME_RELWITHDEBINFO "libobjc.so.4.6"
  )

list(APPEND _IMPORT_CHECK_TARGETS objc )
list(APPEND _IMPORT_CHECK_FILES_FOR_objc "${_IMPORT_PREFIX}/lib/libobjc.so.4.6" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)

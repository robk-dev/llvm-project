cmake_minimum_required(VERSION 3.16)

# Cross-platform build for LLDB GNUstep Objective-C examples
# This unifies the previous Linux and UCRT64 Makefiles into a single CMake build.

project(LldbGnuStepExamples LANGUAGES C)

# --------------------------------------------------------------------------------------
# Configuration knobs (override with -DVAR=VALUE)
# --------------------------------------------------------------------------------------

# Objective-C runtime to use with GNUstep. Defaults differ by platform to match prior Makefiles.
if(WIN32)
  set(OBJC_RUNTIME_VERSION_DEFAULT "gnustep-2.0") # Critical for UCRT64 setup per prior Makefile
else()
  set(OBJC_RUNTIME_VERSION_DEFAULT "gnustep-2.1")
endif()
set(OBJC_RUNTIME_VERSION "${OBJC_RUNTIME_VERSION_DEFAULT}" CACHE STRING "Objective-C runtime to use (e.g. gnustep-2.0 or gnustep-2.1)")

# GNUstep roots and include/lib directories.
# On Linux, default to the local gnustep-install used by the repo; also search system paths.
# On Windows, prefer a Conan-provided root via CONAN_GNUSTEP_ROOT.
if(WIN32)
  set(CONAN_GNUSTEP_ROOT "$ENV{CONAN_GNUSTEP_ROOT}" CACHE PATH "Root where Conan installed GNUstep (contains include/ and GNUstep/System/Library)")
  if(CONAN_GNUSTEP_ROOT)
    set(GNUSTEP_INCLUDE_DIRS_DEFAULT
      "${CONAN_GNUSTEP_ROOT}/include"
      "${CONAN_GNUSTEP_ROOT}/GNUstep/System/Library/Headers")
    set(GNUSTEP_LIBRARY_DIRS_DEFAULT
      "${CONAN_GNUSTEP_ROOT}/lib"
      "${CONAN_GNUSTEP_ROOT}/GNUstep/System/Library/Libraries")
  else()
    set(GNUSTEP_INCLUDE_DIRS_DEFAULT "")
    set(GNUSTEP_LIBRARY_DIRS_DEFAULT "")
  endif()
else()
  # Linux defaults mirror Makefile.old
  get_filename_component(_EXAMPLES_DIR ${CMAKE_CURRENT_LIST_DIR} ABSOLUTE)
  get_filename_component(_LLDB_DIR ${_EXAMPLES_DIR}/.. ABSOLUTE)
  set(_GNUSTEP_ROOT "${_LLDB_DIR}/gnustep-install")
  set(GNUSTEP_INCLUDE_DIRS_DEFAULT
    "${_GNUSTEP_ROOT}/include"
    "${_GNUSTEP_ROOT}/include/GNUstepBase"
    "${_GNUSTEP_ROOT}/include/Foundation"
    "/usr/local/include/GNUstep"
    "/usr/include/GNUstep")
  set(GNUSTEP_LIBRARY_DIRS_DEFAULT
    "${_GNUSTEP_ROOT}/lib"
    "/usr/local/lib")
endif()

set(GNUSTEP_INCLUDE_DIRS "${GNUSTEP_INCLUDE_DIRS_DEFAULT}" CACHE PATH "List of include directories for GNUstep (semicolon-separated)")
set(GNUSTEP_LIBRARY_DIRS "${GNUSTEP_LIBRARY_DIRS_DEFAULT}" CACHE PATH "List of library directories for GNUstep (semicolon-separated)")

# Build type default to Debug for symbols unless user specified one
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

# Location of LLDB binaries for debug targets; can be overridden by user.
# Default to the sibling top-level build dir: <repo>/build/bin/{lldb,lldb-server}.
get_filename_component(_EXAMPLES_DIR ${CMAKE_CURRENT_LIST_DIR} ABSOLUTE)
get_filename_component(_REPO_ROOT ${_EXAMPLES_DIR}/../.. ABSOLUTE)
set(LLDB_BIN_DEFAULT "${_REPO_ROOT}/build/bin/lldb")
set(LLDB_SERVER_BIN_DEFAULT "${_REPO_ROOT}/build/bin/lldb-server")
set(LLDB_BIN "${LLDB_BIN_DEFAULT}" CACHE FILEPATH "Path to lldb executable for debug-* targets")
set(LLDB_SERVER_BIN "${LLDB_SERVER_BIN_DEFAULT}" CACHE FILEPATH "Path to lldb-server for debug-* targets")
get_filename_component(LLDB_BIN_DIR "${LLDB_BIN}" DIRECTORY)

# --------------------------------------------------------------------------------------
# Source files (explicit list to match the Makefiles)
# --------------------------------------------------------------------------------------
set(EXAMPLE_SOURCES
  simple_test.m
  custom_class_test.m
  foundation_test.m
  string_test.m
  test_nsstring_formatter.m
  test_nsnumber_formatter.m
  array_test.m
  simple_array_test.m
  test_nsnumber_comprehensive.m
  test_nsarray_comprehensive.m
  dictionary_test.m
  nsset_test.m
  test_collections_formatter.m
  test_method_enum.m
  test_indexset.m
  test_decimalnumber.m
  test_characterset.m
  test_data_url_uuid.m
  test_userdefaults.m
  test_tagged_pointers.m
  test_stepping_trampoline.m
  test_object_checker.m
  test_exception_resolver.m
)

# --------------------------------------------------------------------------------------
# Common compile and link setup
# --------------------------------------------------------------------------------------

set(COMMON_OBJC_COMPILE_OPTIONS
  -fobjc-runtime=${OBJC_RUNTIME_VERSION}
  -fblocks
  -fno-strict-aliasing
  -fexceptions
  -fobjc-exceptions
  -fno-omit-frame-pointer
  -fconstant-string-class=NSConstantString
  -DGNUSTEP
  -DGNUSTEP_BASE_LIBRARY=1
)

if(WIN32)
  list(APPEND COMMON_OBJC_COMPILE_OPTIONS
    -DGNU_GUI_LIBRARY=1
    -DGNUSTEP_RUNTIME=1
    -D_NONFRAGILE_ABI=1
    -DGNUSTEP_WITH_DLL
    -D_NATIVE_OBJC_EXCEPTIONS
    -DGSWARN
    -DGSDIAGNOSE
    -Wno-import
  )
else()
  # Linux debug info defaults from Makefile.old
  list(APPEND COMMON_OBJC_COMPILE_OPTIONS -gdwarf-5 -Wno-error=int-conversion)
endif()

set(COMMON_LIBS
  gnustep-base
  objc
  pthread
  m
)

# BlocksRuntime is required on Unix; on UCRT64 it's typically not a separate lib.
if(UNIX)
  list(APPEND COMMON_LIBS BlocksRuntime)
endif()

set(UNIX_EXTRA_LINK_LIBS)
set(WIN32_EXTRA_LINK_LIBS)
set(WIN32_LINK_OPTIONS)
set(UNIX_LINK_OPTIONS)

if(WIN32)
  list(APPEND WIN32_EXTRA_LINK_LIBS
    ws2_32
    advapi32
    comctl32
    user32
    comdlg32
    mpr
    netapi32
    stdc++
    gcc_s
  )
  list(APPEND WIN32_LINK_OPTIONS
    -Wl,--enable-auto-import
    -fuse-ld=lld
  )
else()
  # Add rpaths to GNUstep lib dirs for convenience when running from build dir
  foreach(_libdir IN LISTS GNUSTEP_LIBRARY_DIRS)
    if(_libdir)
      list(APPEND UNIX_LINK_OPTIONS "-Wl,-rpath,${_libdir}")
    endif()
  endforeach()
endif()

include(CheckLanguage)

# Allow overriding the Objective-C compiler; default to clang if available
if(NOT CMAKE_OBJC_COMPILER)
  find_program(_CLANG_OBJC clang)
  if(_CLANG_OBJC)
    set(CMAKE_OBJC_COMPILER "${_CLANG_OBJC}" CACHE FILEPATH "Objective-C compiler" FORCE)
  endif()
endif()

check_language(OBJC)
enable_language(OBJC)

include(CMakePrintHelpers)
cmake_print_variables(OBJC_RUNTIME_VERSION GNUSTEP_INCLUDE_DIRS GNUSTEP_LIBRARY_DIRS)

foreach(src ${EXAMPLE_SOURCES})
  get_filename_component(exe ${src} NAME_WE)
  add_executable(${exe} ${src})
  target_include_directories(${exe} PRIVATE ${GNUSTEP_INCLUDE_DIRS})
  if(GNUSTEP_LIBRARY_DIRS)
    # link_directories is directory scope; prefer target_link_directories for CMP0079+ behavior
    cmake_policy(PUSH)
    if(POLICY CMP0079)
      cmake_policy(SET CMP0079 NEW)
    endif()
    target_link_directories(${exe} PRIVATE ${GNUSTEP_LIBRARY_DIRS})
    cmake_policy(POP)
  endif()
  target_compile_options(${exe} PRIVATE ${COMMON_OBJC_COMPILE_OPTIONS})

  if(WIN32)
    target_link_libraries(${exe} PRIVATE ${COMMON_LIBS} ${WIN32_EXTRA_LINK_LIBS})
    target_link_options(${exe} PRIVATE ${WIN32_LINK_OPTIONS})
  else()
    target_link_libraries(${exe} PRIVATE ${COMMON_LIBS} ${UNIX_EXTRA_LINK_LIBS})
    if(UNIX_LINK_OPTIONS)
      target_link_options(${exe} PRIVATE ${UNIX_LINK_OPTIONS})
    endif()
  endif()
endforeach()

# Convenience target to build all examples
add_custom_target(examples ALL)
foreach(src ${EXAMPLE_SOURCES})
  get_filename_component(exe ${src} NAME_WE)
  add_dependencies(examples ${exe})

  # Debug target: runs LLDB on the example binary with environment correctly set
  if(UNIX)
    add_custom_target(debug-${exe}
      COMMAND ${CMAKE_COMMAND} -E env
              PATH="${LLDB_BIN_DIR}:$ENV{PATH}"
              LLDB_DEBUGSERVER_PATH=${LLDB_SERVER_BIN}
              ${LLDB_BIN} $<TARGET_FILE:${exe}>
      DEPENDS ${exe}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Launching LLDB for ${exe}"
      VERBATIM)
  else()
    add_custom_target(debug-${exe}
      COMMAND ${LLDB_BIN} $<TARGET_FILE:${exe}>
      DEPENDS ${exe}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Launching LLDB for ${exe}"
      VERBATIM)
  endif()
endforeach()

# Meta-target to help tasks.json pick a dynamic debug target name
add_custom_target(debug-one
  COMMENT "Use 'cmake --build . --target debug-<example>' to debug a specific example")

# Helpful status messages
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Objective-C runtime: ${OBJC_RUNTIME_VERSION}")
if(WIN32)
  if(NOT CONAN_GNUSTEP_ROOT)
    message(WARNING "CONAN_GNUSTEP_ROOT not set. Set it to your Conan-installed GNUstep prefix to populate include/lib paths.")
  endif()
else()
  message(STATUS "GNUstep guessed root: ${_GNUSTEP_ROOT}")
endif()

# Usage notes (shown at configure time)
message(STATUS "\nHow to build examples:\n"
  "  mkdir -p build-examples && cd build-examples\n"
  "  cmake -S .. -B . \n"
  "        -DOBJC_RUNTIME_VERSION=${OBJC_RUNTIME_VERSION} \n"
  "        -DGNUSTEP_INCLUDE_DIRS='${GNUSTEP_INCLUDE_DIRS}' \n"
  "        -DGNUSTEP_LIBRARY_DIRS='${GNUSTEP_LIBRARY_DIRS}'\n"
  "  cmake --build . -j\n")

# --------------------------------------------------------------------------------------
# LLDB debug helper targets (debug-<example>)
# --------------------------------------------------------------------------------------

# Guess LLVM/LLDB build bin dir relative to this examples folder; allow override
get_filename_component(_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LLDB_BIN_DEFAULT "${_REPO_ROOT}/build/bin/lldb")
set(LLDB_SERVER_BIN_DEFAULT "${_REPO_ROOT}/build/bin/lldb-server")

set(LLDB_BIN "${LLDB_BIN_DEFAULT}" CACHE FILEPATH "Path to LLDB executable to use for debug targets")
set(LLDB_SERVER_BIN "${LLDB_SERVER_BIN_DEFAULT}" CACHE FILEPATH "Path to lldb-server to use for debug targets")

function(add_lldb_debug_target exe)
  if(NOT TARGET ${exe})
    message(FATAL_ERROR "add_lldb_debug_target: target ${exe} does not exist")
  endif()
  set(tgt "debug-${exe}")
  # Prepend LLDB bin dir to PATH so that helper tools can be found
  get_filename_component(_LLDB_DIR "${LLDB_BIN}" DIRECTORY)
  add_custom_target(${tgt}
    COMMAND ${CMAKE_COMMAND} -E env
            PATH=${_LLDB_DIR}:$ENV{PATH}
            LLDB_DEBUGSERVER_PATH=${LLDB_SERVER_BIN}
            ${LLDB_BIN} ${exe}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${exe}
    VERBATIM
    COMMENT "Launching LLDB for ${exe}")
endfunction()

# Create debug targets for all example executables
foreach(src ${EXAMPLE_SOURCES})
  get_filename_component(exe ${src} NAME_WE)
  if(TARGET ${exe})
    add_lldb_debug_target(${exe})
  endif()
endforeach()

# A convenience default debug target (choose a representative example)
if(TARGET custom_class_test)
  add_custom_target(debug DEPENDS debug-custom_class_test)
endif()

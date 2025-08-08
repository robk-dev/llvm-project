# Install script for directory: /home/robk/code/llvm-project/lldb/libobjc2

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/home/robk/code/llvm-project/lldb/gnustep-install")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/robk/code/llvm-project/lldb/gnustep-build/libobjc2/_deps/robinmap-build/cmake_install.cmake")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libobjc.so.4.6" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libobjc.so.4.6")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libobjc.so.4.6"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/robk/code/llvm-project/lldb/gnustep-build/libobjc2/libobjc.so.4.6")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libobjc.so.4.6" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libobjc.so.4.6")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libobjc.so.4.6")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libobjc.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libobjc.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libobjc.so"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/robk/code/llvm-project/lldb/gnustep-build/libobjc2/libobjc.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libobjc.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libobjc.so")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libobjc.so")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/objc" TYPE FILE FILES
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/Availability.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/Object.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/Protocol.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/capabilities.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/developer.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/encoding.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/hooks.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/message.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/objc-api.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/objc-arc.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/objc-auto.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/objc-class.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/objc-exception.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/objc-runtime.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/objc-visibility.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/objc.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/runtime-deprecated.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/runtime.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/slot.h"
    "/home/robk/code/llvm-project/lldb/gnustep-build/libobjc2/objc/objc-config.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/blocks_private.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/objc/blocks_runtime.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "/home/robk/code/llvm-project/lldb/libobjc2/Block.h"
    "/home/robk/code/llvm-project/lldb/libobjc2/Block_private.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/libobjc/libobjcTargets.cmake")
    file(DIFFERENT EXPORT_FILE_CHANGED FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/libobjc/libobjcTargets.cmake"
         "/home/robk/code/llvm-project/lldb/gnustep-build/libobjc2/CMakeFiles/Export/lib/cmake/libobjc/libobjcTargets.cmake")
    if(EXPORT_FILE_CHANGED)
      file(GLOB OLD_CONFIG_FILES "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/libobjc/libobjcTargets-*.cmake")
      if(OLD_CONFIG_FILES)
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/libobjc/libobjcTargets.cmake\" will be replaced.  Removing files [${OLD_CONFIG_FILES}].")
        file(REMOVE ${OLD_CONFIG_FILES})
      endif()
    endif()
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libobjc" TYPE FILE FILES "/home/robk/code/llvm-project/lldb/gnustep-build/libobjc2/CMakeFiles/Export/lib/cmake/libobjc/libobjcTargets.cmake")
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libobjc" TYPE FILE FILES "/home/robk/code/llvm-project/lldb/gnustep-build/libobjc2/CMakeFiles/Export/lib/cmake/libobjc/libobjcTargets-debug.cmake")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libobjc" TYPE FILE FILES
    "/home/robk/code/llvm-project/lldb/gnustep-build/libobjc2/libobjcConfig.cmake"
    "/home/robk/code/llvm-project/lldb/gnustep-build/libobjc2/libobjcConfigVersion.cmake"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/home/robk/code/llvm-project/lldb/gnustep-build/libobjc2/libobjc.pc")
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/home/robk/code/llvm-project/lldb/gnustep-build/libobjc2/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")

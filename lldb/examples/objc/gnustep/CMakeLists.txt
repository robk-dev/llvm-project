cmake_minimum_required(VERSION 3.16)
project(GNUstepExamples LANGUAGES C OBJC)

# Detect platform
if(WIN32)
    message(STATUS "Configuring for Windows platform")
    set(GNUSTEP_RUNTIME "gnustep-2.0")
    set(USE_CONAN OFF)
    
    # Check for Conan configuration
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../conanbuildinfo.mak")
        message(STATUS "Found Conan configuration, parsing...")
        file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../conanbuildinfo.mak" CONAN_LINES)
        
        # Parse include directories
        foreach(LINE ${CONAN_LINES})
            if(LINE MATCHES "^CONAN_INCLUDE_DIRS_GNUSTEP.*\\+=.*")
                string(REGEX REPLACE "^CONAN_INCLUDE_DIRS_GNUSTEP.*\\+= *" "" INCLUDE_PATH "${LINE}")
                string(STRIP "${INCLUDE_PATH}" INCLUDE_PATH)
                if(NOT "${INCLUDE_PATH}" STREQUAL "")
                    list(APPEND CONAN_INCLUDE_PATHS "${INCLUDE_PATH}")
                    set(USE_CONAN ON)
                endif()
            elseif(LINE MATCHES "^CONAN_LIB_DIRS_GNUSTEP.*\\+=.*")
                string(REGEX REPLACE "^CONAN_LIB_DIRS_GNUSTEP.*\\+= *" "" LIB_PATH "${LINE}")
                string(STRIP "${LIB_PATH}" LIB_PATH)
                if(NOT "${LIB_PATH}" STREQUAL "")
                    list(APPEND CONAN_LIB_PATHS "${LIB_PATH}")
                endif()
            endif()
        endforeach()
        
        if(USE_CONAN)
            message(STATUS "Using Conan-provided GNUstep libraries")
            message(STATUS "Conan include paths: ${CONAN_INCLUDE_PATHS}")
            message(STATUS "Conan library paths: ${CONAN_LIB_PATHS}")
        endif()
    endif()
    
    # Windows-specific setup
    if(USE_CONAN)
        # Use Conan paths
        include_directories(${CONAN_INCLUDE_PATHS})
        link_directories(${CONAN_LIB_PATHS})
        set(GNUSTEP_LIBRARIES objc kqueue gnustep-base)
    else()
        # Fallback to default Windows paths
        find_path(GNUSTEP_INCLUDE_DIR
            NAMES Foundation/Foundation.h
            PATHS C:/tools/msys64/ucrt64/include/GNUstep
                  C:/msys64/ucrt64/include/GNUstep
                  C:/GNUstep/include
            PATH_SUFFIXES GNUstep
        )
        
        find_library(GNUSTEP_BASE_LIBRARY
            NAMES gnustep-base
            PATHS C:/tools/msys64/ucrt64/lib
                  C:/msys64/ucrt64/lib
                  C:/GNUstep/lib
        )
        
        if(GNUSTEP_INCLUDE_DIR AND GNUSTEP_BASE_LIBRARY)
            include_directories(${GNUSTEP_INCLUDE_DIR})
            get_filename_component(GNUSTEP_LIB_DIR ${GNUSTEP_BASE_LIBRARY} DIRECTORY)
            link_directories(${GNUSTEP_LIB_DIR})
            set(GNUSTEP_LIBRARIES objc gnustep-base)
        else()
            message(FATAL_ERROR "GNUstep libraries not found on Windows")
        endif()
    endif()
    
    # Use lld linker on Windows
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    
else()
    # Linux setup
    message(STATUS "Configuring for Linux platform")
    set(GNUSTEP_RUNTIME "gnustep-2.1")
    
    # Find GNUstep using pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GNUSTEP REQUIRED gnustep-base)
    
    if(GNUSTEP_FOUND)
        message(STATUS "Found GNUstep via pkg-config")
        include_directories(${GNUSTEP_INCLUDE_DIRS})
        link_directories(${GNUSTEP_LIBRARY_DIRS})
        set(GNUSTEP_LIBRARIES ${GNUSTEP_LIBRARIES} objc BlocksRuntime pthread m)
    else()
        # Fallback to manual search
        find_path(GNUSTEP_INCLUDE_DIR
            NAMES Foundation/Foundation.h
            PATHS /usr/local/include/GNUstep
                  /usr/include/GNUstep
            PATH_SUFFIXES GNUstep
        )
        
        if(GNUSTEP_INCLUDE_DIR)
            include_directories(${GNUSTEP_INCLUDE_DIR})
            set(GNUSTEP_LIBRARIES gnustep-base objc BlocksRuntime pthread m)
        else()
            message(FATAL_ERROR "GNUstep libraries not found on Linux")
        endif()
    endif()
endif()

# Set compiler for building (prefer clang from build-stage1 if available)
if(NOT CMAKE_OBJC_COMPILER)
    find_program(STAGE1_CLANG 
        NAMES clang
        PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../../build-stage1/bin
        NO_DEFAULT_PATH
    )
    
    if(STAGE1_CLANG)
        message(STATUS "Using stage1 clang: ${STAGE1_CLANG}")
        set(CMAKE_OBJC_COMPILER ${STAGE1_CLANG})
        set(CMAKE_C_COMPILER ${STAGE1_CLANG})
    else()
        message(STATUS "Stage1 clang not found, using system clang")
    endif()
endif()

# Common compiler flags
set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-runtime=${GNUSTEP_RUNTIME}")
set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fblocks")
set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fno-strict-aliasing")
set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fexceptions")
set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-exceptions")
set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fno-omit-frame-pointer")
set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fconstant-string-class=NSConstantString")
set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1 -DDEBUG=1")

# Debug flags
set(CMAKE_OBJC_FLAGS_DEBUG "-g -O0")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")

if(NOT WIN32)
    # Linux-specific linker flags
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,/usr/local/lib")
endif()

# Define executables
add_executable(simple_test simple_test.m)
add_executable(custom_class_test custom_class_test.m)

# Link libraries
target_link_libraries(simple_test ${GNUSTEP_LIBRARIES})
target_link_libraries(custom_class_test ${GNUSTEP_LIBRARIES})

# Set output directory to match current structure
set_target_properties(simple_test custom_class_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Find LLDB binaries for debug targets
find_program(LLDB_BIN 
    NAMES lldb
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../../build/bin
          ${CMAKE_CURRENT_SOURCE_DIR}/../../../build-stage1/bin
          /usr/local/llvm-reldeb/bin
    NO_DEFAULT_PATH
)

find_program(LLDB_SERVER_BIN
    NAMES lldb-server
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../../build/bin  
          ${CMAKE_CURRENT_SOURCE_DIR}/../../../build-stage1/bin
          /usr/local/llvm-reldeb/bin
    NO_DEFAULT_PATH
)

if(LLDB_BIN AND LLDB_SERVER_BIN)
    message(STATUS "Found LLDB: ${LLDB_BIN}")
    message(STATUS "Found LLDB Server: ${LLDB_SERVER_BIN}")
    
    # Add debug targets that launch LLDB
    add_custom_target(debug-simple_test
        COMMAND ${CMAKE_COMMAND} -E env 
                LLDB_DEBUGSERVER_PATH=${LLDB_SERVER_BIN}
                LD_LIBRARY_PATH=/usr/local/lib:${CMAKE_CURRENT_SOURCE_DIR}/../../../build/lib:$ENV{LD_LIBRARY_PATH}
                ${LLDB_BIN} $<TARGET_FILE:simple_test>
        DEPENDS simple_test
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Launching LLDB for simple_test"
    )
    
    add_custom_target(debug-custom_class_test  
        COMMAND ${CMAKE_COMMAND} -E env
                LLDB_DEBUGSERVER_PATH=${LLDB_SERVER_BIN}
                LD_LIBRARY_PATH=/usr/local/lib:${CMAKE_CURRENT_SOURCE_DIR}/../../../build/lib:$ENV{LD_LIBRARY_PATH}
                ${LLDB_BIN} $<TARGET_FILE:custom_class_test>
        DEPENDS custom_class_test
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Launching LLDB for custom_class_test"
    )
else()
    message(WARNING "LLDB binaries not found. Debug targets will not be available.")
endif()

# Status message
message(STATUS "GNUstep Examples Configuration Complete")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Runtime: ${GNUSTEP_RUNTIME}")
message(STATUS "  Objective-C Compiler: ${CMAKE_OBJC_COMPILER}")
message(STATUS "  Libraries: ${GNUSTEP_LIBRARIES}")
if(LLDB_BIN)
    message(STATUS "  LLDB: ${LLDB_BIN}")
endif()
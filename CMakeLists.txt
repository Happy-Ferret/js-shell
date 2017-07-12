cmake_minimum_required(VERSION 3.5)
project(js-shell CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CheckCXXCompilerFlag)

if(NOT ${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    option(ENABLE_ASAN "Enable address sanitizer." OFF)
endif()
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    option(ENABLE_JS_DEBUG "Build sources with -DJS_DEBUG." OFF)
endif()

set(SANITIZE_CXXFLAGS)
set(SANITIZE_LDFLAGS)
if(ENABLE_ASAN)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR
       "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        list(APPEND SANITIZE_CXXFLAGS
            "-fsanitize=address -fno-omit-frame-pointer")
        list(APPEND SANITIZE_LDFLAGS
            "-fsanitize=address")
    else()
        message(WARNING "Option ENABLE_ASAN only supported with clang and gcc.")
    endif()
endif()

set(GNUCXX_MINIMUM_VERSION "4.8")
set(CLANGCXX_MINIMUM_VERSION "3.5")
set(CXX_STANDARD_TAG "c++14")

# Helper macro. Add -O0 in Debug configuration to prevent any optimization.
# Makes gdb users much happier.
macro(patch_cmake_cxx_debug_flags)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS_DEBUG "-O0 ${CMAKE_CXX_FLAGS_DEBUG}")
    endif()
endmacro()

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    patch_cmake_cxx_debug_flags()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS CLANGCXX_MINIMUM_VERSION)
        message(FATAL_ERROR
            "Minimum required clang++ version: ${CLANGCXX_MINIMUM_VERSION}")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=${CXX_STANDARD_TAG}")
    if(ENABLE_LIBCXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    patch_cmake_cxx_debug_flags()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS GNUCXX_MINIMUM_VERSION)
        message(FATAL_ERROR
            "Minimum required g++ version: ${GNUCXX_MINIMUM_VERSION}")
    endif()

    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9")
        set(CMAKE_CXX_FLAGS
            "${CMAKE_CXX_FLAGS} -std=c++1y")
    else()
        set(CMAKE_CXX_FLAGS
            "${CMAKE_CXX_FLAGS} -std=${CXX_STANDARD_TAG}")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic")
    endif()

    # Use colorized output on terminal if supported (GCC 4.9 onwards)
    CHECK_CXX_COMPILER_FLAG("-fdiagnostics-color=auto" GCC_HAS_COLOR)
    if("${GCC_HAS_COLOR}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=auto")
    endif()
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "Intel")
    message(WARNING "Intel C++ not supported.")
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 17.0)
        message(FATAL_ERROR "Visual Studio must be at least 2012")
    endif()

    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS
            "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} /D_SCL_SECURE_NO_WARNINGS /DNOMINMAX")

    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 18.0)
        # Make Google Test compile with Visual Studio 2012
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_VARIADIC_MAX=10")
    endif()
else()
    message(AUTHOR_WARNING
        "Could not determine compiler ID: ${CMAKE_CXX_COMPILER_ID}")
endif()

find_package(SpiderMonkey REQUIRED)
message(STATUS "${SPIDERMONKEY_INCLUDE_DIRS}")
message(STATUS "${SPIDERMONKEY_LIBRARIES}")
if(ENABLE_JS_DEBUG)
    add_definitions(-DJS_DEBUG)
endif()
if(MSVC)
    add_definitions(-DXP_WIN -DWIN32)
endif()

set(_include_dirs ${SPIDERMONKEY_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
set(_link_libs ${SPIDERMONKEY_LIBRARIES})

find_package(JSRDBG)
if(JSRDBG_FOUND)
    add_definitions(-DUSE_REMOTE_DEBUGGER)
    list(APPEND _include_dirs ${JSRDBG_INCLUDE_DIR})
    list(APPEND _link_libs ${JSRDBG_LIBRARIES})
endif()

include_directories(${_include_dirs})

# JANUS comes with its own set of SpiderMonkey headers and those are in a
# different directory layout
add_definitions(-DEWSJS_STANDALONE_SPIDERMONKEY)

add_executable(run main.cpp)
target_link_libraries(run ${_link_libs})
set_target_properties(run PROPERTIES
    LINKER_LANGUAGE CXX
    COMPILE_FLAGS "${SANITIZE_CXXFLAGS}"
    LINK_FLAGS "${SANITIZE_LDFLAGS}")

# vim:et sw=4 ts=4

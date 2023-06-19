cmake_minimum_required(VERSION 3.18)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(OW_DEVELOPING "Set up the build system for developing." OFF)

include(version)

project(
    owiz-lang
    VERSION ${OW_VERSION_NUMBER}
    DESCRIPTION "The OWIZ programming language"
    LANGUAGES C CXX
)

option(OW_BUILD_IPO        "Enable inter-procedural optimization."                               ON)
option(OW_BUILD_RUNTIME    "Build the library and the executable command line interface."        ON)
option(OW_BUILD_STATIC     "Build static library (archive); otherwise shared (dynamic) library" OFF)
option(OW_DOC_SOURCE       "Generate documentation for source code."                            OFF)
option(OW_DOC_HEADER       "Generate documentation for public header files."                    OFF)
option(OW_DOC_LANGUAGE     "Generate documentation from doc/*.md."                              OFF)
option(OW_TEST             "Build test files and prepare for ctest command."                     ON)
option(OW_PACK             "Configure for cpack command."                                       OFF)
option(OW_PACK_HEADER      "Include header files when installing."                               ON)
option(OW_PACK_RELA_RPATH  "Use relative RPATH when installing."                                 ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if(NOT CMAKE_BUILD_TYPE AND NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release")
    message(STATUS "Set build type to ${CMAKE_BUILD_TYPE}")
endif()

if(OW_BUILD_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _res OUTPUT _out LANGUAGES C)
    if(_res)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "IPO not supported: ${_out}.")
    endif()
    unset(_res)
    unset(_out)
endif()

if(NOT OW_DEVELOPING)
    message(NOTICE "Set `OW_DEVELOPING` to `ON` if you are developing!")
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC") # MSVC
    add_compile_options(
        /W4 /utf-8 /Zc:__cplusplus,inline,preprocessor
        $<$<COMPILE_LANGUAGE:C>:/wd4200> # warning C4200: nonstandard extension used : zero-sized array in struct/union
        /wd4706 # warning C4706: assignment within conditional expression
        /FI sal.h
        #/analyze
    )
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS) # Use "unsafe" functions.
else() # GCC, Clang, ...
    add_compile_options(
        -Wall -Wextra -Wpedantic -pipe -pthread
        $<$<COMPILE_LANGUAGE:C>:-Werror=incompatible-pointer-types>
        $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>
        $<$<COMPILE_LANGUAGE:C>:-Werror=return-type>
    )
    add_link_options(
        -pthread
    )
endif()

add_subdirectory("src")

if(OW_DOC_LANGUAGE)
    add_subdirectory("doc")
endif()

if(OW_TEST)
    enable_testing()
    add_subdirectory("test")
    message(STATUS "`OW_TEST` is ON. Use `ctest` to run tests.")
endif()

if(OW_PACK)
    set(CPACK_PACKAGE_CHECKSUM   "MD5")
    set(CPACK_STRIP_FILES         TRUE)
    include(CPack)
    message(STATUS "`OW_PACK` is ON. Use `cpack` to generate packages.")
endif()

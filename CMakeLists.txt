cmake_minimum_required(VERSION 3.18)

project(
	ow-lang
	LANGUAGES C
)

option(OW_DEVELOPING "Set up the build system for developing." OFF)
option(OW_BUILD_IPO "Enable inter-procedural optimization." ON)
option(OW_DOC_SOURCE "Generate documentation for source code." OFF)
option(OW_DOC_HEADER "Generate documentation for public header files." OFF)
option(OW_DOC_LANGUAGE "Generate documentation from doc/*.md." OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)

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

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC") # MSVC
	add_compile_options(
		/W4 /utf-8 /Zc:__cplusplus,inline,preprocessor
		<$<COMPILE_LANGUAGE:C>:/wd4200> # warning C4200: nonstandard extension used : zero-sized array in struct/union
		/wd4706 # warning C4706: assignment within conditional expression
	)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS) # Use "unsafe" functions.
else() # GCC, Clang, ...
	add_compile_options(
		-Wall -Wextra -Wpedantic -pipe
		$<$<COMPILE_LANGUAGE:C>:-Werror=incompatible-pointer-types>
		$<$<COMPILE_LANGUAGE:C>:-Werror=return-type>
	)
endif()

add_subdirectory("src")

if(OW_DOC_LANGUAGE)
	add_subdirectory("doc")
endif()

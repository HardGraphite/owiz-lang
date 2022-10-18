option(OW_DEBUG_LEXER "Compile debugging code for lexer." OFF)
option(OW_DEBUG_PARSER "Compile debugging code for parser." OFF)

## Collect source files.
set(ow_src "ow.c")
foreach(sub_dir IN ITEMS
		"compat"
		"compiler"
		"utilities"
	)
	aux_source_directory(${sub_dir} ow_src)
endforeach()

## Define target.
add_executable(ow ${ow_src})

## Add include directories.
target_include_directories(ow PRIVATE
	"${CMAKE_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}"
)

## Add "OW_DEBUG_XXX" flags.
foreach(opt_name IN ITEMS
		OW_DEBUG_LEXER
		OW_DEBUG_PARSER
	)
	if(${opt_name})
		target_compile_definitions(ow PRIVATE "${opt_name}=1")
		message(STATUS "`${opt_name}` is enabled")
	endif()
endforeach()

## Rules to generate machine/version.h file.
if(OW_DEVELOPING)
	set(version_file "${CMAKE_SOURCE_DIR}/version.txt")
	set(version_header_file "${CMAKE_CURRENT_SOURCE_DIR}/machine/version.h")
	add_custom_command(
		OUTPUT "${version_header_file}"
		COMMAND "${CMAKE_SOURCE_DIR}/tool/version.py"
			"${version_file}" -H "${version_header_file}"
		DEPENDS "${version_file}"
		COMMENT "Generating ${version_header_file}"
		VERBATIM
	)
	target_sources(ow PRIVATE ${version_header_file}) # As dependencies.
	unset(version_file)
	unset(version_header_file)
endif()

## Rules to generate compiler/ast_node_xxx.h files.
if(OW_DEVELOPING)
	set(compiler_ast_node_xxx_files
		"${CMAKE_CURRENT_SOURCE_DIR}/compiler/ast_node_funcs.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/compiler/ast_node_list.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/compiler/ast_node_structs.h"
	) # See compiler/ast_node_def.py .
	add_custom_command(
		OUTPUT ${compiler_ast_node_xxx_files}
		COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/compiler/ast_node_def.py"
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/compiler/ast_node_def.ini"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/compiler"
		COMMENT "Generating compiler/ast_node_*.h files"
		VERBATIM
	)
	target_sources(ow PRIVATE ${compiler_ast_node_xxx_files}) # As dependencies.
	unset(compiler_ast_node_xxx_files)
endif()

## Source documentations.
if(OW_DOC_SOURCE OR OW_DOC_HEADER)
	find_package(Doxygen)

	if(NOT DOXYGEN_FOUND)
		message(FATAL_ERROR "Cannot find doxygen.")
	endif()

	set(DOXYGEN_FILE_PATTERNS "*.h")
	set(DOXYGEN_EXCLUDE_PATTERNS ".*;*/compiler/ast_node_*.h")
	set(DOXYGEN_EXCLUDE_SYMBOLS "_*;ELEM")
	set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
	set(DOXYGEN_EXTRACT_ALL YES) # ??
	set(DOXYGEN_FULL_PATH_NAMES NO)
	set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C YES)
	set(DOXYGEN_GENERATE_TREEVIEW YES)
	set(DOXYGEN_SHOW_NAMESPACES NO)
	set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)
	set(DOXYGEN_WARN_NO_PARAMDOC NO)

	set(doxygen_in)
	if(OW_DOC_SOURCE)
		list(APPEND doxygen_in "${CMAKE_SOURCE_DIR}/src")
	endif()
	if(OW_DOC_HEADER)
		list(APPEND doxygen_in "${CMAKE_SOURCE_DIR}/include")
	endif()
	doxygen_add_docs(ow_doc_source ${doxygen_in} ALL)
	unset(doxygen_in)
endif()

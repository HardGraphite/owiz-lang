option(OW_DEBUG_LEXER "Compile debugging code for lexer." OFF)
option(OW_DEBUG_PARSER "Compile debugging code for parser." OFF)
option(OW_DEBUG_CODEGEN "Compile debugging code for code generator." OFF)
option(OW_DEBUG_MEMORY "Compile debugging code for memory management." OFF)

## Collect source files.
set(ow_main_src "ow.c")
set(ow_src "api.c")
foreach(sub_dir IN ITEMS
		"bytecode"
		"compat"
		"compiler"
		"machine"
		"modules"
		"objects"
		"utilities"
	)
	aux_source_directory(${sub_dir} ow_src)
endforeach()

## Define target.
set(ow_out_shared_dir "${CMAKE_CURRENT_BINARY_DIR}/shared")
set(ow_out_static_dir "${CMAKE_CURRENT_BINARY_DIR}/static")
set(ow_exe_targets)
set(ow_lib_targets)
if(OW_BUILD_SHARED)
	add_library(ow_lib_shared SHARED ${ow_src})
	add_executable(ow_exe_shared ${ow_main_src})
	target_link_libraries(ow_exe_shared PRIVATE ow_lib_shared)
	set_target_properties(
		ow_exe_shared ow_lib_shared PROPERTIES
		OUTPUT_NAME "ow"
		ARCHIVE_OUTPUT_DIRECTORY ${ow_out_shared_dir}
		LIBRARY_OUTPUT_DIRECTORY ${ow_out_shared_dir}
		RUNTIME_OUTPUT_DIRECTORY ${ow_out_shared_dir}
	)
	if(WIN32)
		set_target_properties(ow_lib_shared PROPERTIES PREFIX "lib")
	endif()
	list(APPEND ow_exe_targets ow_exe_shared)
	list(APPEND ow_lib_targets ow_lib_shared)
	message(STATUS "Output directory for `OW_BUILD_SHARED` is ${ow_out_shared_dir}.")
endif()
if(OW_BUILD_STATIC)
	add_library(ow_lib_static STATIC ${ow_src})
	add_executable(ow_exe_static ${ow_main_src})
	target_link_libraries(ow_exe_static PRIVATE ow_lib_static)
	set_target_properties(
		ow_exe_static ow_lib_static PROPERTIES
		OUTPUT_NAME "ow"
		ARCHIVE_OUTPUT_DIRECTORY ${ow_out_static_dir}
		LIBRARY_OUTPUT_DIRECTORY ${ow_out_static_dir}
		RUNTIME_OUTPUT_DIRECTORY ${ow_out_static_dir}
	)
	set_target_properties(ow_exe_static PROPERTIES ENABLE_EXPORTS TRUE)
	if(WIN32)
		set_target_properties(ow_lib_shared PROPERTIES PREFIX "lib")
	endif()
	list(APPEND ow_exe_targets ow_exe_static)
	list(APPEND ow_lib_targets ow_lib_static)
	message(STATUS "Output directory for `OW_BUILD_STATIC` is ${ow_out_static_dir}.")
endif()

## Add include directories.
foreach(tgt IN LISTS ow_exe_targets ow_lib_targets)
	target_include_directories(
		${tgt} PRIVATE
		"${CMAKE_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_SOURCE_DIR}"
	)
endforeach()

# Add definitions for library targets.
foreach(tgt IN LISTS ow_lib_targets)
	target_compile_definitions(
		${tgt} PRIVATE
		"OW_EXPORT_API=1"
	)
endforeach()

## Add "OW_DEBUG_XXX" flags.
foreach(opt_name IN ITEMS
		OW_DEBUG_LEXER
		OW_DEBUG_PARSER
		OW_DEBUG_CODEGEN
		OW_DEBUG_MEMORY
	)
	if(${opt_name})
		foreach(tgt IN LISTS ow_lib_targets)
			target_compile_definitions(${tgt} PRIVATE "${opt_name}=1")
		endforeach()
		message(STATUS "`${opt_name}` is enabled")
	endif()
endforeach()

## Rules to generate machine/version.h file.
if(OW_DEVELOPING)
	set(version_file "${CMAKE_SOURCE_DIR}/VERSION.txt")
	set(version_header_file "${CMAKE_CURRENT_SOURCE_DIR}/machine/version.h")
	add_custom_command(
		OUTPUT "${version_header_file}"
		COMMAND "${CMAKE_SOURCE_DIR}/tool/version.py"
			"${version_file}" -H "${version_header_file}"
		DEPENDS "${version_file}"
		COMMENT "Generating ${version_header_file}"
		VERBATIM
	)
	foreach(tgt IN LISTS ow_lib_targets)
		target_sources(${tgt} PRIVATE ${version_header_file}) # As dependencies.
	endforeach()
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
	foreach(tgt IN LISTS ow_lib_targets)
		target_sources(${tgt} PRIVATE ${compiler_ast_node_xxx_files}) # As dependencies.
	endforeach()
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
	set(DOXYGEN_SORT_MEMBER_DOCS NO)
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

## Installation configurations.
if(ow_exe_targets AND ow_lib_targets)
	if(WIN32)
		set(ow_install_exe_dir ".")
		set(ow_install_lib_dir ".")
		set(ow_install_inc_dir "include")
	else()
		include(GNUInstallDirs)
		set(ow_install_exe_dir ${CMAKE_INSTALL_BINDIR})
		set(ow_install_lib_dir ${CMAKE_INSTALL_LIBDIR})
		set(ow_install_inc_dir ${CMAKE_INSTALL_INCLUDEDIR})
	endif()
	if(OW_BUILD_SHARED AND OW_PACK_RELA_RPATH AND UNIX)
		set_target_properties(ow_exe_shared PROPERTIES INSTALL_RPATH
			"$<IF:$<PLATFORM_ID:Darwin>,@executable_path,$ORIGIN>/../${ow_install_lib_dir}")
	endif()
	file(GLOB ow_install_headers "${CMAKE_SOURCE_DIR}/include/*.h")
	install(TARGETS ${ow_exe_targets} DESTINATION ${ow_install_exe_dir})
	install(TARGETS ${ow_lib_targets} DESTINATION ${ow_install_lib_dir})
	if(OW_PACK_HEADER)
		install(FILES ${ow_install_headers} DESTINATION ${ow_install_inc_dir})
	endif()
	if(OW_BUILD_SHARED AND OW_BUILD_STATIC)
		message(WARNING "Both `OW_BUILD_SHARED` and `OW_BUILD_STATIC` is ON, \
and the generated binary files may conflict when installing and packing.")
	endif()
endif()

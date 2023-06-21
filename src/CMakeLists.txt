include(FindReadline)
set(OW_LIB_READLINE_USE_LIBEDIT ${LibReadline_IS_LibEdit})

include(mkcode)
if(WIN32)
    include(wintools)
endif()

###################################
##### Options and definitions #####
###################################

##### Switches. #####

option(OW_DEBUG_LEXER        "Compile debugging code for lexer."                                OFF)
option(OW_DEBUG_PARSER       "Compile debugging code for parser."                               OFF)
option(OW_DEBUG_CODEGEN      "Compile debugging code for code generator."                       OFF)
option(OW_DEBUG_MEMORY       "Compile debugging code for memory management."                    OFF)
option(OW_BUILD_BYTECODE_DUMP_COMMENT "Print operand comment in `ow_bytecode_dump()`."           ON)

##### Names and variables. #####

set(ow_conf_header_dir             "${CMAKE_CURRENT_BINARY_DIR}/config")
set(OW_FILENAME_EXTENSION_SRC      ".ow" ) # Source code.
set(OW_FILENAME_EXTENSION_BTC      ".owc") # Bytecode.
set(OW_FILENAME_EXTENSION_OBJ      ".owo") # Native dynamic library.
set(OW_BUILD_OUTPUT_NAME           "owiz")
set(OW_BUILD_COMPILER_NAME         "${CMAKE_C_COMPILER_ID}")
set(OW_BUILD_COMPILER_VERSION      "${CMAKE_C_COMPILER_VERSION}")

##### Debug logging. #####

if(OW_DEBUG_LOGGING)
    if(NOT DEFINED OW_DEBUG_LOGGING_ENVNAME)
        string(TOUPPER "${OW_BUILD_OUTPUT_NAME}_DEBUGLOG" OW_DEBUG_LOGGING_ENVNAME)
        set(OW_DEBUG_LOGGING_ENVNAME "${OW_DEBUG_LOGGING_ENVNAME}" CACHE
            STRING "Environment variable name for controlling debug logging.")
    endif()

    if(NOT DEFINED OW_DEBUG_LOGGING_DEFAULT)
        set(OW_DEBUG_LOGGING_DEFAULT "" CACHE
            STRING "The default value for debug logging controlling. Leave it empty to disable.")
    endif()
endif()

##### Module lists. #####

include("${CMAKE_CURRENT_SOURCE_DIR}/modules/modules_conf.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/modules/modules_util.cmake")

if(NOT DEFINED OW_EMBEDDED_MODULES OR NOT DEFINED OW_DYNAMIC_MODULES)
    set(OW_EMBEDDED_MODULES "*;-%")
    set(OW_DYNAMIC_MODULES "%")

    set(mod_list_common_suffix)
    if(NOT LibReadline_FOUND)
        list(APPEND mod_list_common_suffix "-readline")
    endif()
    foreach(mod_list IN ITEMS OW_EMBEDDED_MODULES OW_DYNAMIC_MODULES)
        list(APPEND ${mod_list} ${mod_list_common_suffix})
    endforeach()
    unset(mod_list_common_suffix)

    set(OW_EMBEDDED_MODULES ${OW_EMBEDDED_MODULES} CACHE
        STRING "List of embedded modules. ${ow_mod_util_conf_list_doc}")
    set(OW_DYNAMIC_MODULES ${OW_DYNAMIC_MODULES} CACHE
        STRING "List of dynamic modules. ${ow_mod_util_conf_list_doc}")
endif()

###################################
##### Source code generations #####
###################################

##### Generate config header: "definitions.h". #####

file(CONFIGURE OUTPUT "${ow_conf_header_dir}/definitions.h" @ONLY CONTENT [==[
#pragma once
#define  OW_FILENAME_EXTENSION_SRC  "@OW_FILENAME_EXTENSION_SRC@"
#define  OW_FILENAME_EXTENSION_BTC  "@OW_FILENAME_EXTENSION_BTC@"
#define  OW_FILENAME_EXTENSION_OBJ  "@OW_FILENAME_EXTENSION_OBJ@"
#define  OW_BUILD_COMPILER_NAME     "@OW_BUILD_COMPILER_NAME@"
#define  OW_BUILD_COMPILER_VERSION  "@OW_BUILD_COMPILER_VERSION@"
#define  OW_DEBUG_LOGGING_ENVNAME   "@OW_DEBUG_LOGGING_ENVNAME@"
#define  OW_DEBUG_LOGGING_DEFAULT   "@OW_DEBUG_LOGGING_DEFAULT@"
]==])

##### Generate config header: "options.h". #####

file(CONFIGURE OUTPUT "${ow_conf_header_dir}/options.h" @ONLY CONTENT [==[
#pragma once
#cmakedefine01  OW_DEBUG_LOGGING
#cmakedefine01  OW_DEBUG_MEMORY
#cmakedefine01  OW_DEBUG_LEXER
#cmakedefine01  OW_DEBUG_PARSER
#cmakedefine01  OW_DEBUG_CODEGEN
#cmakedefine01  OW_BUILD_BYTECODE_DUMP_COMMENT
#cmakedefine01  OW_LIB_READLINE_USE_LIBEDIT
]==])

##### Generate config header: "version.h". #####

file(CONFIGURE OUTPUT "${ow_conf_header_dir}/version.h" @ONLY CONTENT [==[
#pragma once
#define  OW_VERSION_MAJOR   @OW_VERSION_MAJOR@
#define  OW_VERSION_MINOR   @OW_VERSION_MINOR@
#define  OW_VERSION_PATCH   @OW_VERSION_PATCH@
#define  OW_VERSION_STRING  "@OW_VERSION_STRING@"
]==])

##### Generate compiler/ast_node_xxx.h files. #####

if(OW_DEVELOPING)
    set(compiler_ast_node_xxx_files
        # See "compiler/ast_node_def.py".
        "${CMAKE_CURRENT_SOURCE_DIR}/compiler/ast_node_funcs.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/compiler/ast_node_list.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/compiler/ast_node_structs.h"
    )
    add_custom_command(
        OUTPUT ${compiler_ast_node_xxx_files}
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/compiler/ast_node_def.py"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/compiler/ast_node_def.ini"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/compiler"
        COMMENT "Generating compiler/ast_node_*.h files"
        VERBATIM
    )
    add_custom_target(
        ow_ast_node_xxx_files
        ALL
        DEPENDS ${compiler_ast_node_xxx_files}
        COMMENT "Generated compiler/ast_node_*.h files"
    )
    unset(compiler_ast_node_xxx_files)
endif()

#############################################################
##### Targets: executable, library, and dynamic modules #####
#############################################################

##### Executable and library. #####

set(ow_exe_src "toplevels/start.c")
set(ow_lib_src "toplevels/api.c")
foreach(
    sub_dir IN
    ITEMS "bytecode" "compat" "compiler" "machine" "objects" "utilities"
)
    aux_source_directory(${sub_dir} ow_lib_src)
endforeach()

set(ow_exe_targets)
set(ow_lib_targets)

if(NOT OW_BUILD_STATIC) # shared library
    ## Lib.
    add_library(ow_lib_shared SHARED ${ow_lib_src})
    list(APPEND ow_lib_targets ow_lib_shared)
    ## Exe.
    add_executable(ow_exe_shared ${ow_exe_src})
    target_link_libraries(ow_exe_shared PRIVATE ow_lib_shared)
    list(APPEND ow_exe_targets ow_exe_shared)
else() # static library
    ## Lib.
    add_library(ow_lib_static STATIC ${ow_lib_src})
    list(APPEND ow_lib_targets ow_lib_static)
    ## Exe.
    add_executable(ow_exe_static ${ow_exe_src})
    target_link_libraries(ow_exe_static PRIVATE ow_lib_static)
    set_target_properties(ow_exe_static PROPERTIES ENABLE_EXPORTS TRUE)
    list(APPEND ow_exe_targets ow_exe_static)
endif()

foreach(tgt IN LISTS ow_exe_targets ow_lib_targets)
    set_target_properties(${tgt} PROPERTIES OUTPUT_NAME "${OW_BUILD_OUTPUT_NAME}")
endforeach()

foreach(tgt IN LISTS ow_lib_targets)
    target_compile_definitions(${tgt} PRIVATE "OWIZ_EXPORT_API=1")
    if(OW_DEVELOPING)
        add_dependencies(${tgt} ow_ast_node_xxx_files)
    endif()
endforeach()

if(WIN32)
    foreach(tgt IN LISTS ow_lib_targets)
        set_target_properties(${tgt} PROPERTIES PREFIX "lib")
        ow_win_target_add_rc(${tgt})
    endforeach()
    foreach(tgt IN LISTS ow_exe_targets)
        ow_win_target_add_rc(${tgt})
    endforeach()
endif()

##### Modules (embedded and dynamic). #####

ow_mod_util_collect_embedded_modules("${OW_EMBEDDED_MODULES}" ow_embedded_mods)
ow_mod_util_collect_dynamic_modules("${OW_DYNAMIC_MODULES}" ow_dynamic_mods)

set(ow_mod_targets)

## Embedded modules.
foreach(tgt IN LISTS ow_lib_targets)
    get_target_property(tgt_type ${tgt} TYPE)
    if(tgt_type STREQUAL "STATIC_LIBRARY")
        set(scope "PUBLIC")
    else()
        set(scope "PRIVATE")
    endif()
    foreach(mod_name IN LISTS ow_embedded_mods)
        ow_mod_util_add_sources(${tgt} PRIVATE ${mod_name})
        ow_mod_util_link_libraries(${tgt} ${scope} ${mod_name})
        message(STATUS "Add embedded module `${mod_name}'.")
    endforeach()
endforeach()
ow_make_c_macro_list("OW_EMBEDDED_MODULE_LIST" "${ow_embedded_mods}" tmp_str)
string(CONCAT tmp_str "// \"@OW_EMBEDDED_MODULES@\"\n\n" "${tmp_str}")
file(CONFIGURE OUTPUT "${ow_conf_header_dir}/module_list.h" @ONLY CONTENT "${tmp_str}")
unset(tmp_str)

## Dynamic modules.
foreach(mod_name IN LISTS ow_dynamic_mods)
    set(tgt "ow_mod_${mod_name}")
    add_library(${tgt} MODULE)
    target_compile_definitions(${tgt} PRIVATE "OW_EXPORT_MOD=1")
    set_target_properties(
        ${tgt} PROPERTIES
        PREFIX ""
        OUTPUT_NAME ${mod_name}
        SUFFIX ${OW_FILENAME_EXTENSION_OBJ}
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/modules"
    )
    ow_mod_util_add_sources(${tgt} PRIVATE ${mod_name})
    ow_mod_util_link_libraries(${tgt} PRIVATE ${mod_name})
    list(APPEND ow_mod_targets ${tgt})
    message(STATUS "Add dynamic module `${mod_name}'.")
endforeach()

##### Include directories. #####
foreach(tgt IN LISTS ow_exe_targets ow_lib_targets ow_mod_targets)
    target_include_directories(
        ${tgt} PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
    )
endforeach()

##### Exclude from ALL #####

if(NOT OW_BUILD_RUNTIME)
    set_target_properties(
        ${ow_exe_targets} ${ow_lib_targets} ${ow_mod_targets}
        PROPERTIES EXCLUDE_FROM_ALL TRUE
    )
endif()

#################################
##### Source documentations #####
#################################

if(OW_DOC_SOURCE OR OW_DOC_HEADER)
    find_package(Doxygen)

    if(NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "Cannot find doxygen.")
    endif()

    set(DOXYGEN_PROJECT_LOGO "${CMAKE_SOURCE_DIR}/doc/logo.svg")
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

#######################################
##### Installation configurations #####
#######################################

##### Install directories. #####

if(WIN32)
    set(ow_install_exe_dir ".")
    set(ow_install_lib_dir ".")
    set(ow_install_inc_dir "include")
    set(ow_install_mod_dir "lib")
else()
    include(GNUInstallDirs)
    set(ow_install_exe_dir ${CMAKE_INSTALL_BINDIR})
    set(ow_install_lib_dir ${CMAKE_INSTALL_LIBDIR})
    set(ow_install_inc_dir ${CMAKE_INSTALL_INCLUDEDIR})
    set(ow_install_mod_dir "${CMAKE_INSTALL_LIBDIR}/owiz")
endif()

##### RPATH. #####

if(DEFINED ow_exe_shared AND OW_PACK_RELA_RPATH AND UNIX)
    set_target_properties(
        ow_exe_shared PROPERTIES INSTALL_RPATH
        "$<IF:$<PLATFORM_ID:Darwin>,@executable_path,$ORIGIN>/../${ow_install_lib_dir}"
    )
endif()

##### Install rules. #####

if(OW_BUILD_RUNTIME)
    install(TARGETS ${ow_exe_targets} DESTINATION ${ow_install_exe_dir})
    install(TARGETS ${ow_lib_targets} DESTINATION ${ow_install_lib_dir})
    install(TARGETS ${ow_mod_targets} DESTINATION ${ow_install_mod_dir})
endif()

if(OW_PACK_HEADER)
    file(GLOB ow_install_headers "${CMAKE_SOURCE_DIR}/include/*.h")
    install(FILES ${ow_install_headers} DESTINATION ${ow_install_inc_dir})
endif()

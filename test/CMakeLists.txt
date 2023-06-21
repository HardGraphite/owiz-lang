###############################
##### Test configurations #####
###############################

if(NOT OW_BUILD_STATIC)
    set(ow_test_lib ow_lib_shared)
    set(ow_test_exe ow_exe_shared)
else()
    set(ow_test_lib ow_lib_static)
    set(ow_test_exe ow_exe_static)
endif()

set(OW_TEST_SCRIPT_FILE_EXT "ow")

#####################################
##### Functions to create tests #####
#####################################

# Convert input file name to test nane.
function(ow_test_make_name var file_name)
    get_filename_component(name ${file} NAME_WE)
    if (name MATCHES "([a-z]+)_(.+)")
        set(name "${CMAKE_MATCH_1}:${CMAKE_MATCH_2}")
    endif()
    set(${var} "${name}" PARENT_SCOPE)
endfunction()

# Add test from native code.
function(ow_test_add_native file)
    ow_test_make_name(test_name ${file})
    get_filename_component(out_name ${file} NAME_WE)
    set(tgt_name "ow_test_${out_name}")
    add_executable(${tgt_name} ${file})
    set_target_properties(${tgt_name} PROPERTIES OUTPUT_NAME ${out_name})
    target_link_libraries(${tgt_name} PRIVATE ${ow_test_lib})
    target_include_directories(
        ${tgt_name} PRIVATE
        "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/include"
    )
    add_test(
        NAME ${test_name}
        COMMAND ${out_name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction()

# Add test from non-native code.
function(ow_test_add_script file)
    ow_test_make_name(test_name ${file})
    add_test(
        NAME ${test_name}
        COMMAND ${ow_test_exe} ${file}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction()

#########################################
##### Collect sources and add tests #####
#########################################

file(GLOB ow_test_sub_paths "${CMAKE_CURRENT_SOURCE_DIR}/*")

foreach(sub_path IN LISTS ow_test_sub_paths)
    if (sub_path MATCHES ".+\\.cc?$")
        ow_test_add_native(${sub_path})
    elseif(sub_path MATCHES ".+\\.${OW_TEST_SCRIPT_FILE_EXT}$")
        ow_test_add_script(${sub_path})
    endif()
endforeach()

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

set(OW_TEST_SCRIPTS_GLOB "*.ow")

###############################
##### Native test sources #####
###############################

file(GLOB ow_test_c_files "*.c" "*.cc")
foreach(file IN LISTS ow_test_c_files)
    get_filename_component(out_name ${file} NAME_WE)
    set(tgt_name "ow_test_${out_name}")
    add_executable(${tgt_name} ${file})
    set_target_properties(${tgt_name} PROPERTIES OUTPUT_NAME ${out_name})
    target_include_directories(${tgt_name} PRIVATE "${CMAKE_SOURCE_DIR}/include")
    target_link_libraries(${tgt_name} PRIVATE ${ow_test_lib})
    add_test(
        NAME ${out_name}
        COMMAND ${out_name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()

###################################
##### Non-native test sources #####
###################################

file(GLOB ow_test_ow_files "${OW_TEST_SCRIPTS_GLOB}")
foreach(file IN LISTS ow_test_ow_files)
    get_filename_component(test_name ${file} NAME_WE)
    set(tgt_name "ow_test_${test_name}")
    add_test(
        NAME ${test_name}
        COMMAND ${ow_test_exe} ${file}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()

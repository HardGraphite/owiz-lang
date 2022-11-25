if(OW_BUILD_SHARED)
	set(ow_test_lib ow_lib_shared)
	set(ow_test_exe ow_exe_shared)
elseif(OW_BUILD_STATIC)
	set(ow_test_lib ow_lib_static)
	set(ow_test_exe ow_exe_static)
else()
	message(FATAL_ERROR
		"Neither `OW_BUILD_SHARED` nor `OW_BUILD_STATIC` is ON. Cannot do tests.")
endif()

file(GLOB ow_test_c_files "*.c")
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

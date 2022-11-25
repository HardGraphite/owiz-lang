if(OW_DOC_LANGUAGE)
	find_program(pandoc "pandoc" REQUIRED)

	function(md2html file_in file_out)
		get_filename_component(title "${file_out}" NAME_WLE)
		add_custom_command(
			OUTPUT "${file_out}"
			COMMAND "${pandoc}" "${file_in}" -o "${file_out}"
				--standalone --metadata "title=${title}" --variable "title:"
			DEPENDS "${file_in}"
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
			COMMENT "${file_in} -> ${file_out}"
			VERBATIM
		)
	endfunction()

	set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/html")
	file(MAKE_DIRECTORY "${out_dir}")

	file(GLOB md_files "${CMAKE_CURRENT_SOURCE_DIR}/*.md")
	set(html_files)
	foreach(md_file IN LISTS md_files)
		get_filename_component(html_file "${md_file}" NAME_WLE)
		set(html_file "${out_dir}/${html_file}.html")
		md2html("${md_file}" "${html_file}")
		list(APPEND html_files "${html_file}")
	endforeach()

	add_custom_target(ow_doc_language ALL DEPENDS ${html_files})
endif()

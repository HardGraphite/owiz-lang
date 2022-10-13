option(OW_DEBUG_LEXER "Compile debuggin code for lexer." OFF)

# Collect source files.
set(ow_src "ow.c")
foreach(sub_dir IN ITEMS
		"compat"
		"compiler"
		"utilities"
	)
	aux_source_directory(${sub_dir} ow_src)
endforeach()

# Define target.
add_executable(ow ${ow_src})

# Add include directories.
target_include_directories(ow PRIVATE
	"${CMAKE_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}"
)

# Add "OW_DEBUG_XXX" flags.
foreach(opt_name IN ITEMS
		OW_DEBUG_LEXER
	)
	if(${opt_name})
		target_compile_definitions(ow PRIVATE "${opt_name}=1")
		message(NOTICE "`${opt_name}` is enabled")
	endif()
endforeach()

include_guard(DIRECTORY)

# Make a macro list in C.
function(ow_make_c_macro_list list_name list_data out_var)
    if(NOT list_data)
        set(${out_var} "#define ${list_name}\n" PARENT_SCOPE)
        return()
    endif()
    string(JOIN ") \\\n\tELEM(" tmp_str ${list_data})
    set(${out_var} "#define ${list_name} \\\n\tELEM(${tmp_str})\n" PARENT_SCOPE)
endfunction()

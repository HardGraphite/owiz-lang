include_guard(DIRECTORY)

find_path(LibReadline_INCLUDE_DIR NAMES readline/readline.h)
find_library(LibReadline_LIBRARY NAMES readline)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	LibReadline DEFAULT_MSG
	LibReadline_INCLUDE_DIR LibReadline_LIBRARY
)

if(NOT LibReadline_FOUND)

set(LibReadline_IS_LibEdit TRUE)

find_path(LibReadline_INCLUDE_DIR NAMES editline/readline.h)
find_library(LibReadline_LIBRARY NAMES edit)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	LibReadline DEFAULT_MSG
	LibReadline_INCLUDE_DIR LibReadline_LIBRARY
)

endif()

mark_as_advanced(LibReadline_INCLUDE_DIR LibReadline_LIBRARY)

include_guard(DIRECTORY)

include(version)

# Add resource script file to target.
function(ow_win_target_add_rc target)
	set(icon_file_path "${CMAKE_SOURCE_DIR}/doc/logo.ico")

	get_target_property(target_type ${target} TYPE)
	if(target_type STREQUAL "EXECUTABLE")
		set(rc_filetype "VFT_APP")
	elseif(target_type STREQUAL "SHARED_LIBRARY")
		set(rc_filetype "VFT_DLL")
	elseif(target_type STREQUAL "STATIC_LIBRARY")
		set(rc_filetype "VFT_STATIC_LIB")
	else()
		set(rc_filetype "VFT_UNKNOWN")
	endif()

	get_target_property(original_filename ${target} OUTPUT_NAME)
	get_target_property(target_prefix ${target} PREFIX)
	if(target_prefix)
		set(original_filename "${target_prefix}${original_filename}")
	endif()
	get_target_property(target_suffix ${target} SUFFIX)
	if(NOT target_suffix)
		if(target_type STREQUAL "EXECUTABLE")
			set(target_suffix ".exe")
		elseif(target_type STREQUAL "SHARED_LIBRARY")
			set(target_suffix ".dll")
		elseif(target_type STREQUAL "STATIC_LIBRARY")
			set(target_suffix ".lib")
		else()
			set(target_suffix "")
		endif()
	endif()
	set(original_filename "${original_filename}${target_suffix}")

	set(rc_file_path "${CMAKE_CURRENT_BINARY_DIR}/${target}.rc")
	file(CONFIGURE OUTPUT "${rc_file_path}" @ONLY CONTENT [==[
#include <winver.h>

#define OW_VERSION_NUMBER  @OW_VERSION_MAJOR@,@OW_VERSION_MINOR@,@OW_VERSION_PATCH@,0
#define OW_VERSION_STRING  "@OW_VERSION_STRING@\0"

VS_VERSION_INFO VERSIONINFO
FILEVERSION     OW_VERSION_NUMBER
PRODUCTVERSION  OW_VERSION_NUMBER
FILEOS         	VOS__WINDOWS32
FILETYPE       	@rc_filetype@
FILESUBTYPE     VFT2_UNKNOWN
BEGIN
	BLOCK "StringFileInfo"
	BEGIN
		BLOCK "040904B0"
		BEGIN
			VALUE "CompanyName",      "OwizLang"
			VALUE "FileDescription",  "Owiz Programming Language"
			VALUE "FileVersion",      OW_VERSION_STRING
			VALUE "InternalName",     "ow"
			VALUE "LegalCopyright",   ""
			VALUE "OriginalFilename", "@original_filename@\0"
			VALUE "ProductName",      "OWIZ"
			VALUE "ProductVersion",   OW_VERSION_STRING
		END
	END
	BLOCK "VarFileInfo"
	BEGIN
		VALUE "Translation", 0x409, 1200
	END
END

1 ICON "@icon_file_path@"
	]==])

	target_sources(${target} PRIVATE ${rc_file_path})
endfunction()

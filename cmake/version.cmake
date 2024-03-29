include_guard(DIRECTORY)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" OW_VERSION_STRING)
string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" OW_VERSION_NUMBER ${OW_VERSION_STRING})
set(OW_VERSION_MAJOR ${CMAKE_MATCH_1})
set(OW_VERSION_MINOR ${CMAKE_MATCH_2})
set(OW_VERSION_PATCH ${CMAKE_MATCH_3})

message(STATUS "Owiz version ${OW_VERSION_NUMBER} (${OW_VERSION_STRING})")

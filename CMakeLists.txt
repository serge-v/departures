cmake_minimum_required(VERSION 2.6)

find_package(CURL REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/version.c" "${CMAKE_CURRENT_BINARY_DIR}/version.h"
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/common/genversion.sh
	ARGS ${CMAKE_CURRENT_BINARY_DIR}
	DEPENDS *.c *.h
	${CMAKE_CURRENT_SOURCE_DIR}/common/genversion.sh CMakeLists.txt
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "Generating version."
)

add_executable(departures
	departures.c
	stations.c
	parser.c
	util.c
	common/net.c
	common/struct.c
	${CMAKE_CURRENT_BINARY_DIR}/version.c)

target_link_libraries(departures ${CURL_LIBRARY})


cmake_minimum_required(VERSION 2.6)

find_package(CURL REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include(../common/macros.cmake)
include_directories(..)

gen_version_c()

gen_resource_c(api_help.txt)

add_subdirectory(../common "${CMAKE_BINARY_DIR}/common")

add_executable(departures
	${CMAKE_CURRENT_BINARY_DIR}/api_help.txt.c
	departures.c
	stations.c
	parser.c
	util.c
	${CMAKE_CURRENT_BINARY_DIR}/version.c)

target_link_libraries(
	departures
	${CURL_LIBRARY}
	svc
)

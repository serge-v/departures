cmake_minimum_required(VERSION 2.6)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(CURL REQUIRED)
find_package(LibXml2 REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include(../../w/common/macros.cmake)
include_directories(${LIBXML2_INCLUDE_DIR})
include_directories(${CURL_INCLUDE_DIR})
include_directories(../../w)

gen_version_c()

gen_resource_c(api_help.txt)

add_subdirectory(../../w/common "${CMAKE_BINARY_DIR}/common")

add_executable(departures
	${CMAKE_CURRENT_BINARY_DIR}/api_help.txt.c
	departures.c
	stations.c
	parser.c
	util.c
	${CMAKE_CURRENT_BINARY_DIR}/version.c)

add_dependencies(departures mkresource)

target_link_libraries(
	departures
	${CURL_LIBRARY}
	svc
)

install(TARGETS departures RUNTIME DESTINATION bin)

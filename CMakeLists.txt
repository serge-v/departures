cmake_minimum_required(VERSION 2.6)

enable_testing()

set(CMAKE_BUILD_TYPE Debug)

include_directories("/usr/local/include")

find_library(curl_LIBRARY NAMES curl PATHS "/usr/local/lib")

add_custom_command(OUTPUT "version.c" "version.h"
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/genversion.sh
	DEPENDS
	[!v][!e][!r]*.c
	[!v][!e][!r]*.h
	*.sh
	CMakeLists.txt)

add_executable(departures
	departures.c
	stations.c
	parser.c
	util.c
	common/net.c
	version.c)

target_link_libraries(departures ${curl_LIBRARY})

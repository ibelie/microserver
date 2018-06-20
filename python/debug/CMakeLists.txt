# Copyright 2017-2018 ibelie, Chen Jie, Joungtao. All rights reserved.
# Use of this source code is governed by The MIT License
# that can be found in the LICENSE file.

project(microserver)

cmake_minimum_required(VERSION 2.8)

set(SOURCE_FILES
	"${PROJECT_SOURCE_DIR}/main.c"
	"${PROJECT_SOURCE_DIR}/../microserver/tcp_client.c"
	"${PROJECT_SOURCE_DIR}/../c/client.c"
	"${PROJECT_SOURCE_DIR}/../c/map.c"
	"${PROJECT_SOURCE_DIR}/../c/port.c"
	"${PROJECT_SOURCE_DIR}/../c/tcp.c"
	"${PROJECT_SOURCE_DIR}/../c/tree.c"
)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
include_directories("${PROJECT_SOURCE_DIR}/../c")

if(WIN32)
	set(CompilerFlags
		CMAKE_CXX_FLAGS
		CMAKE_CXX_FLAGS_DEBUG
		CMAKE_CXX_FLAGS_RELEASE
		CMAKE_C_FLAGS
		CMAKE_C_FLAGS_DEBUG
		CMAKE_C_FLAGS_RELEASE
	)
	foreach(CompilerFlag ${CompilerFlags})
		string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
	endforeach()
	foreach(CompilerFlag ${CompilerFlags})
		string(REPLACE "/MDd" "/MTd" ${CompilerFlag} "${${CompilerFlag}}")
	endforeach()
	include_directories("C:/Python27/include")
	target_link_libraries(${PROJECT_NAME} "C:/Python27/libs/python27.lib")
else(WIN32)
	find_package(PythonLibs REQUIRED)
	if(PYTHONLIBS_FOUND)
		message("Python found " ${PYTHONLIBS_VERSION_STRING})
		include_directories(${PYTHON_INCLUDE_DIRS})
		target_link_libraries(${PROJECT_NAME} ${PYTHON_LIBRARIES})
	endif()
endif(WIN32)

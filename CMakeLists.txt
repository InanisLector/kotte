cmake_minimum_required(VERSION 3.25)

set(PROJECT_NAME kotte)
project(${PROJECT_NAME} VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENTIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(NOT DEFINED US) # Update Submodules
	set(US ON)
endif()

include(cmake/sources.cmake)
include(cmake/external.cmake)

list(LENGTH EXTERNAL_AUTOUPDATE_DEPS autoupdate_deps_count)
if(US AND autoupdate_deps_count GREATER 0)
	unset(US CACHE)

	if(NOT EXISTS "${PROJECT_SOURCE_DIR}/.git")
		message(FATAL_ERROR
			"Trying to update submodules without the root .git folder. "
			"Use -DUS=OFF to compile without updating the modules."
		)
	endif()

	find_package(Git REQUIRED)
	foreach(dep IN LISTS EXTERNAL_AUTOUPDATE_DEPS)
		file(RELATIVE_PATH module_path "${PROJECT_SOURCE_DIR}" "${EXTERNAL_DIR}/${dep}")
		execute_process(
			COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive -- ${module_path}
			WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
			RESULT_VARIABLE git_result
		)

		if(NOT git_result EQUAL 0)
			message(FATAL_ERROR
				"Failed to update submodule '${dep}'. "
				"Use -DUS=OFF to skip updating"
			)
		endif()
	endforeach()
endif()

unset(US CACHE)

#Including the body

add_executable(${PROJECT_NAME} ${APP_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
	"${PROJECT_SOURCE_DIR}/kotte/include"
)

#Including submodules

foreach(dep IN LISTS EXTERNAL_DEPS)
	if(NOT EXISTS "${EXTERNAL_DIR}/${dep}/CMakeLists.txt")
		message(FATAL_ERROR
			"External dependency '${dep}' is missing. "
			"Run: git submodule update --init --recursive"
		)
	endif()

	add_subdirectory("${EXTERNAL_DIR}/${dep}")
endforeach()

target_include_directories(${PROJECT_NAME} PRIVATE
	${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(${PROJECT_NAME} PRIVATE
	${EXTERNAL_LINK_TARGETS}
)

#Including interface modules

foreach(dep IN LISTS EXTERNAL_INTERFACES)
	if(NOT EXISTS "${EXTERNAL_DIR}/${dep}/include")
		message(FATAL_ERROR
			"External precompiled dependency '${dep}' is missing. "
		)
	endif()

	add_library(${dep} INTERFACE)
	target_include_directories(${dep} SYSTEM INTERFACE "${EXTERNAL_DIR}/${dep}/include")
endforeach()

target_link_libraries(${PROJECT_NAME} PRIVATE
	${EXTERNAL_INTERFACES}
)

#Warning levels

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

#exe path

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

cmake_minimum_required(VERSION 3.12)

project(
	ct-sql
	VERSION 0.1.0
	LANGUAGES CXX
)

# Determine if this is the top-level project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(DEFAULT_OPTION_VALUE ON)
else()
    set(DEFAULT_OPTION_VALUE OFF)
endif()

option(CT_SQL_BUILD_TESTS "ct-sql tests should be built" ${DEFAULT_OPTION_VALUE})
option(CT_SQL_BUILD_BENCH "ct-sql benchmarks should be built" ${DEFAULT_OPTION_VALUE})
option(CT_SQL_USE_SYSTEM_MYSQL "Use system MySQL or MariaDB" ON)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(CPM)

function(setup_ct_target project_name)
	set_target_properties(${project_name} PROPERTIES
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
		LINKER_LANGUAGE CXX
	)

	if (MSVC)
		target_compile_definitions(${project_name} INTERFACE _CRT_SECURE_NO_WARNINGS)
	endif()
endfunction()

add_subdirectory(include)

if (CT_SQL_BUILD_TESTS)
	add_subdirectory(test)
endif()

if (CT_SQL_BUILD_BENCH)
	add_subdirectory(bench)
endif()



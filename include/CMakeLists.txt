
add_library(ct-sql INTERFACE)
setup_ct_target(ct-sql)

target_include_directories(ct-sql INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if (CT_SQL_USE_SYSTEM_MYSQL)
	find_package(MySQL REQUIRED)
	target_link_libraries(ct-sql INTERFACE mysql)
endif()


message(STATUS "Building ct-sql-test")
add_executable(ct-sql-test "test.cpp")

setup_ct_target(ct-sql-test)
target_link_libraries(ct-sql-test PRIVATE ct-sql)

target_include_directories(ct-sql-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

CPMAddPackage(
  NAME googletest
  GITHUB_REPOSITORY google/googletest
  VERSION 1.16.0
  OPTIONS 
    "INSTALL_GTEST OFF" 
    "gtest_force_shared_crt ON"
)

set_target_properties(gtest gmock PROPERTIES CXX_STANDARD 20)
target_link_libraries(ct-sql-test PRIVATE gtest gmock)


# On Windows, the DLL needs to be in the same folder as the executable to run
if (WIN32 AND CT_SQL_USE_SYSTEM_MYSQL)
    if (NOT ${MYSQL_FOUND})
        find_package(MySQL REQUIRED)
    endif()
    get_filename_component(MYSQL_LIB_DIR ${MYSQL_LIBRARY} DIRECTORY)

    set(MYSQL_DLL_NAMES "libmysql.dll" "libmariadb.dll")
    set(FOUND_DLL "")

    # Iterate through the names and find the first one that exists
    foreach(DLL_NAME IN ITEMS ${MYSQL_DLL_NAMES})
        if (EXISTS "${MYSQL_LIB_DIR}/${DLL_NAME}")
            set(FOUND_DLL "${MYSQL_LIB_DIR}/${DLL_NAME}")
            break()
        endif()
    endforeach()

    # If a DLL was found, add a custom command to copy it
    if(FOUND_DLL)
        add_custom_command(
            TARGET ct-sql-test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FOUND_DLL}"
            "$<TARGET_FILE_DIR:ct-sql-test>"
            COMMENT "Copying MySQL/MariaDB DLL to build directory."
        )
    endif()
endif()

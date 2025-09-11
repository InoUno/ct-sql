# Try to find MYSQL
# MYSQL_FOUND - System has MYSQL
# MYSQL_LIBRARY - The libraries needed to use MYSQL
# MYSQL_INCLUDE_DIR - The MYSQL include directories

if(WIN32)
    file(GLOB mariadb-dir "$ENV{ProgramFiles}/MariaDB*" "$ENV{SystemDrive}/MariaDB*")
    set(mariadb-dir-lib ${mariadb-dir})
    list(TRANSFORM mariadb-dir-lib REPLACE ".+" "\\0/lib")

    set(mariadb-dir-include ${mariadb-dir})
    list(TRANSFORM mariadb-dir-include REPLACE ".+" "\\0/include/mysql")
else()
    set(mariadb-dir-lib "")
    set(mariadb-dir-include "")
endif()

# Try finding mariadb library first
find_library(MYSQL_LIBRARY
    NAMES
        mariadb mariadbclient libmariadb libmariadb64
    PATHS
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib
        /opt/homebrew/lib
        ${mariadb-dir-lib}
    NO_DEFAULT_PATH
)

if(NOT ${MYSQL_LIBRARY})
    # Fallback to trying to find mysql
    find_library(MYSQL_LIBRARY
        NAMES
            mysql libmysql mysqlclient libmysql64 mariadb mariadbclient libmariadb libmariadb64
        PATHS
            /usr/lib
            /usr/lib/x86_64-linux-gnu
            /usr/local/lib
            /usr/lib/mysql
            /usr/local/lib/mysql
            /usr/local/mysql/lib
            /usr/local/mysql/lib/mysql
            /opt/mysql/mysql/lib
            /opt/mysql/mysql/lib/mysql
            /opt/homebrew/lib
            $ENV{ProgramFiles}/MySQL/*/lib
            $ENV{SystemDrive}/MySQL/*/lib
        NO_DEFAULT_PATH
    )
endif()


find_path(MYSQL_INCLUDE_DIR
    NAMES
        mysql.h
    PATHS
        /usr/include/mysql
        /usr/include/mariadb
        /usr/local/include/mysql
        /usr/local/include/mariadb
        /opt/homebrew/opt/mysql/include/mysql
        /opt/homebrew/opt/mariadb/include/mysql
        ${mariadb-dir-include}
        $ENV{ProgramFiles}/MySQL/*/include
        $ENV{SystemDrive}/MySQL/*/include
    NO_DEFAULT_PATH
)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(MySQL DEFAULT_MSG MYSQL_LIBRARY MYSQL_INCLUDE_DIR)

message(STATUS "MYSQL_FOUND: ${MYSQL_FOUND}")
message(STATUS "MYSQL_LIBRARY: ${MYSQL_LIBRARY}")
message(STATUS "MYSQL_INCLUDE_DIR: ${MYSQL_INCLUDE_DIR}")

if (${MYSQL_FOUND})
    add_library(mysql INTERFACE)
    target_link_libraries(mysql INTERFACE ${MYSQL_LIBRARY})
    target_include_directories(mysql SYSTEM INTERFACE ${MYSQL_INCLUDE_DIR})
endif()

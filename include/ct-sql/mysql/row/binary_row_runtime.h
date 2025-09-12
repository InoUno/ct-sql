
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <mysql.h>

#include "./binary_row_base.h"
#include "./runtime_row_base.h"
#include "ct-sql/common.h"

namespace ct_sql
{
    class MySqlBinaryRowRuntime : public MySqlBinaryRowBase, public MySqlRuntimeRowBase<MySqlBinaryRowRuntime>
    {
    public:
        inline explicit MySqlBinaryRowRuntime(MYSQL_STMT* stmt, MYSQL_FIELD* fields, MYSQL_BIND& bind, std::unordered_map<std::string, size_t>& column_map)
        : MySqlBinaryRowBase(stmt, fields, bind)
        , MySqlRuntimeRowBase<MySqlBinaryRowRuntime>(column_map)
        {
        }
    };
}

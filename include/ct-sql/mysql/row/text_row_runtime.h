
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <mysql.h>

#include "ct-sql/common.h"

namespace ct_sql
{
    class MySqlTextRowRuntime : public MySqlTextRowBase, public MySqlRuntimeRowBase<MySqlTextRowRuntime>
    {
    public:
        inline explicit MySqlTextRowRuntime(MYSQL_ROW row, unsigned long* lengths, std::unordered_map<std::string, size_t>& column_map)
        : MySqlTextRowBase(row, lengths)
        , MySqlRuntimeRowBase<MySqlTextRowRuntime>(column_map)
        {
        }
    };
}

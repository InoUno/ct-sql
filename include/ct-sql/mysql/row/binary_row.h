
#pragma once

#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <mysql.h>

#include "./binary_row_base.h"
#include "./comptime_row_base.h"
#include "ct-sql/common.h"
#include "ct-sql/query_parsing.h"

namespace ct_sql
{
    template <StringLiteral Query>
    class MySqlBinaryRow : public MySqlBinaryRowBase, public MySqlComptimeRowBase<MySqlBinaryRow<Query>, Query>
    {
    public:
        inline explicit MySqlBinaryRow(MYSQL_STMT* stmt, MYSQL_FIELD* fields, MYSQL_BIND& bind, std::unordered_map<std::string, size_t>& column_map)
        : MySqlBinaryRowBase(stmt, fields, bind)
        , MySqlComptimeRowBase<MySqlBinaryRow<Query>, Query>(fields, column_map)
        {
        }
    };
}

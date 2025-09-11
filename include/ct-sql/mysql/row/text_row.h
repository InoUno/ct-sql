
#pragma once

#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include <mysql.h>

#include "./comptime_row_base.h"
#include "./text_row_base.h"
#include "ct-sql/common.h"
#include "ct-sql/query_parsing.h"

namespace ct_sql
{
    template <StringLiteral Query>
    class MySqlTextRow : public MySqlTextRowBase, public MySqlComptimeRowBase<MySqlTextRow<Query>, Query>
    {
    private:
        using QueryColumns = ParseColumns<Query>;

    public:
        inline explicit MySqlTextRow(MYSQL_ROW row, MYSQL_FIELD* fields, unsigned long* lengths, std::unordered_map<std::string, size_t>& column_map)
        : MySqlTextRowBase(row, lengths)
        , MySqlComptimeRowBase<MySqlTextRow<Query>, Query>(fields, column_map)
        {
        }

        // Non-copyable
        MySqlTextRow(const MySqlTextRow&) = delete;
        MySqlTextRow& operator=(const MySqlTextRow&) = delete;

        // Movable
        inline MySqlTextRow(MySqlTextRow&& other) noexcept
        : MySqlTextRowBase(other.row_, other.lengths_)
        , MySqlComptimeRowBase<MySqlTextRow<Query>, Query>(other.fields_, other.column_map_)
        {
        }
    };
}

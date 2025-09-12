
#pragma once

#include <cstring>
#include <optional>

#include <mysql.h>

#include "ct-sql/common.h"
#include "ct-sql/query_parsing.h"

#include "../row/binary_row.h"

namespace ct_sql
{
    template <StringLiteral Query>
    class MySqlBinaryResultSet
    {
    private:
        MYSQL_STMT* stmt_;
        MYSQL_RES* metadata_;

        // Only used and populated, if columns are gotten via regular string arguments.
        std::unordered_map<std::string, size_t> column_map_;

        MYSQL_BIND bind_;
        unsigned long length_;
        char buffer_[CT_SQL_MAX_BUFFER_LENGTH];

        using QueryColumns = ParseColumns<Query>;

    public:
        inline explicit MySqlBinaryResultSet(MYSQL_STMT* stmt)
        : stmt_(stmt)
        , metadata_(nullptr)
        {
            if (stmt_)
            {
                metadata_ = mysql_stmt_result_metadata(stmt);
                if (!metadata_)
                {
                    return;
                }

                std::memset(&bind_, 0, sizeof(bind_));
                bind_.buffer        = &buffer_;
                bind_.buffer_length = CT_SQL_MAX_BUFFER_LENGTH;
                bind_.length        = &length_;
            }
        }

        inline ~MySqlBinaryResultSet()
        {
            if (metadata_)
            {
                mysql_free_result(metadata_);
            }
        }

        // Movable
        inline MySqlBinaryResultSet(MySqlBinaryResultSet&& other) noexcept
        : stmt_(other.stmt_)
        , metadata_(other.metadata_)
        , bind_(std::move(other.bind_))
        {
            other.stmt_     = nullptr;
            other.metadata_ = nullptr;
        }

        inline MySqlBinaryResultSet& operator=(MySqlBinaryResultSet&& other) noexcept
        {
            if (this != &other)
            {
                stmt_     = other.stmt_;
                metadata_ = other.metadata_;
                bind_     = std::move(other.bind_);

                other.stmt_ = nullptr;
            }
            return *this;
        }

        inline std::optional<MySqlBinaryRow<Query>> next()
        {
            if (!stmt_)
            {
                return std::nullopt;
            }

            auto status = mysql_stmt_fetch(stmt_);
            if (status == 1 || status == MYSQL_NO_DATA)
            {
                return std::nullopt;
            }

            return std::make_optional<MySqlBinaryRow<Query>>(stmt_, metadata_->fields, bind_, column_map_);
        }

        inline constexpr size_t column_count() const
        {
            return QueryColumns::column_count;
        }

        inline size_t get_row_count() const
        {
            return mysql_stmt_num_rows(stmt_);
        }

        inline size_t get_affected_row_count() const
        {
            return mysql_stmt_affected_rows(stmt_);
        }

        template <size_t ColumnIndex>
        constexpr std::string_view column_name()
        {
            return QueryColumns::template column_name<ColumnIndex>();
        }

        template <StringLiteral ColumnName>
        constexpr size_t column_index()
        {
            return QueryColumns::template column_index<ColumnName>();
        }
    };
}

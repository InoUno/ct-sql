
#pragma once

#include <optional>

#include <mysql.h>

#include "ct-sql/query_parsing.h"

#include "../row/text_row.h"

namespace ct_sql
{
    template <StringLiteral Query>
    class MySqlTextResultSet
    {
    private:
        MYSQL_RES* result_;

        size_t affected_rows_ = 0;

        // Only used and populated, if columns are gotten via regular string arguments.
        std::unordered_map<std::string, size_t> column_map_;
        MYSQL_FIELD* fields_;

        using QueryColumns = ParseColumns<Query>;

    public:
        inline explicit MySqlTextResultSet(MYSQL_RES* result)
        : result_(result)
        , fields_(nullptr)
        {
            if (result_)
            {
                fields_ = mysql_fetch_fields(result_);
            }
        }

        inline explicit MySqlTextResultSet(size_t affected_rows)
        : result_(nullptr)
        , fields_(nullptr)
        , affected_rows_(affected_rows)
        {
        }

        inline ~MySqlTextResultSet()
        {
            if (result_)
            {
                mysql_free_result(result_);
            }
        }

        // Non-copyable
        MySqlTextResultSet(const MySqlTextResultSet&)            = delete;
        MySqlTextResultSet& operator=(const MySqlTextResultSet&) = delete;

        // Movable
        inline MySqlTextResultSet(MySqlTextResultSet&& other) noexcept
        : result_(other.result_)
        {
            other.result_ = nullptr;
        }

        inline MySqlTextResultSet& operator=(MySqlTextResultSet&& other) noexcept
        {
            if (this != &other)
            {
                if (result_)
                {
                    mysql_free_result(result_);
                }

                result_ = other.result_;

                other.result_ = nullptr;
            }
            return *this;
        }

        inline std::optional<MySqlTextRow<Query>> next()
        {
            if (!result_)
            {
                return std::nullopt;
            }

            auto row = mysql_fetch_row(result_);
            if (row)
            {
                auto lengths = mysql_fetch_lengths(result_);
                return { MySqlTextRow<Query>(row, fields_, lengths, column_map_) };
            }
            else
            {
                return std::nullopt;
            }
        }

        static constexpr size_t column_count()
        {
            return QueryColumns::column_count;
        }

        inline size_t get_row_count() const
        {
            return result_ ? mysql_num_rows(result_) : 0;
        }

        inline size_t get_affected_row_count() const
        {
            return affected_rows_;
        }

        template <size_t ColumnIndex>
        static constexpr std::string_view column_name()
        {
            return QueryColumns::template column_name<ColumnIndex>();
        }

        template <StringLiteral ColumnName>
        static constexpr size_t column_index()
        {
            return QueryColumns::template column_index<ColumnName>();
        }
    };
}

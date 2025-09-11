
#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include <mysql.h>

#include "../row/text_row_runtime.h"

namespace ct_sql
{
    class MySqlTextResultSetRuntime
    {
    private:
        MYSQL_RES* result_;
        unsigned long* lengths_;

        size_t affected_rows_ = 0;

        std::unordered_map<std::string, size_t> column_map_;
        MYSQL_FIELD* fields_;

    public:
        inline explicit MySqlTextResultSetRuntime(MYSQL_RES* result)
        : result_(result)
        , lengths_(nullptr)
        , fields_(nullptr)
        {
            if (result_)
            {
                fields_ = mysql_fetch_fields(result_);

                auto num_fields = mysql_num_fields(result_);
                for (size_t i = 0; i < num_fields; ++i)
                {
                    column_map_[fields_[i].name] = i;
                }
            }
        }

        inline explicit MySqlTextResultSetRuntime(size_t affected_rows)
        : result_(nullptr)
        , lengths_(nullptr)
        , fields_(nullptr)
        , affected_rows_(affected_rows)
        {
        }

        inline ~MySqlTextResultSetRuntime()
        {
            if (result_)
            {
                mysql_free_result(result_);
            }
        }

        // Non-copyable
        MySqlTextResultSetRuntime(const MySqlTextResultSetRuntime&)            = delete;
        MySqlTextResultSetRuntime& operator=(const MySqlTextResultSetRuntime&) = delete;

        // Movable
        inline MySqlTextResultSetRuntime(MySqlTextResultSetRuntime&& other) noexcept
        : result_(other.result_)
        , lengths_(other.lengths_)
        , fields_(other.fields_)
        , column_map_(std::move(other.column_map_))
        {
            other.result_  = nullptr;
            other.lengths_ = nullptr;
            other.fields_  = nullptr;
        }

        inline MySqlTextResultSetRuntime& operator=(MySqlTextResultSetRuntime&& other) noexcept
        {
            if (this != &other)
            {
                if (result_)
                {
                    mysql_free_result(result_);
                }

                result_     = other.result_;
                lengths_    = other.lengths_;
                fields_     = other.fields_;
                column_map_ = std::move(other.column_map_);

                other.result_  = nullptr;
                other.lengths_ = nullptr;
                other.fields_  = nullptr;
            }
            return *this;
        }

        inline std::optional<MySqlTextRowRuntime> next()
        {
            if (!result_)
            {
                return std::nullopt;
            }

            auto row = mysql_fetch_row(result_);
            if (row)
            {
                auto lengths = mysql_fetch_lengths(result_);
                return std::make_optional<MySqlTextRowRuntime>(row, lengths, column_map_);
            }
            else
            {
                return std::nullopt;
            }
        }

        inline size_t get_row_count() const
        {
            return result_ ? mysql_num_rows(result_) : 0;
        }

        inline size_t get_affected_row_count() const
        {
            return affected_rows_;
        }

        inline size_t column_count() const
        {
            return result_ ? mysql_num_fields(result_) : 0;
        }

        inline std::string column_name(size_t index) const
        {
            if (!result_ || index >= column_count())
            {
                throw std::out_of_range("Column index out of range");
            }

            MYSQL_FIELD* fields_ = mysql_fetch_fields(result_);
            return fields_[index].name;
        }

        inline size_t column_index(std::string_view name) const
        {
            auto it = column_map_.find(name.data());
            return (it != column_map_.end()) ? it->second : static_cast<size_t>(-1);
        }
    };
}

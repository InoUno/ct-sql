
#pragma once

#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <mysql.h>

#include "ct-sql/common.h"

#include "../row/binary_row_runtime.h"

namespace ct_sql
{
    class MySqlBinaryResultSetRuntime
    {
    private:
        MYSQL_STMT* stmt_;
        MYSQL_RES* metadata_;
        MYSQL_FIELD* fields_;

        std::unordered_map<std::string, size_t> column_map_;

        MYSQL_BIND bind_;
        unsigned long length_;
        char buffer_[CT_SQL_MAX_BUFFER_LENGTH];

    public:
        inline explicit MySqlBinaryResultSetRuntime(MYSQL_STMT* stmt)
        : stmt_(stmt)
        , metadata_(nullptr)
        , fields_(nullptr)
        {
            if (!stmt_)
            {
                return;
            }
            metadata_ = mysql_stmt_result_metadata(stmt);
            if (!metadata_)
            {
                return;
            }

            fields_ = mysql_fetch_fields(metadata_);

            std::memset(&bind_, 0, sizeof(bind_));
            bind_.buffer        = &buffer_;
            bind_.buffer_length = CT_SQL_MAX_BUFFER_LENGTH;
            bind_.length        = &length_;

            auto num_fields = mysql_num_fields(metadata_);
            for (size_t i = 0; i < num_fields; ++i)
            {
                column_map_[fields_[i].name] = i;
            }
        }

        inline ~MySqlBinaryResultSetRuntime()
        {
            if (metadata_)
            {
                mysql_free_result(metadata_);
            }
        }

        // Movable
        inline MySqlBinaryResultSetRuntime(MySqlBinaryResultSetRuntime&& other) noexcept
        : stmt_(other.stmt_)
        , metadata_(other.metadata_)
        , fields_(other.fields_)
        , column_map_(std::move(other.column_map_))
        , bind_(std::move(other.bind_))
        {
            other.stmt_     = nullptr;
            other.metadata_ = nullptr;
            other.fields_   = nullptr;
        }

        inline MySqlBinaryResultSetRuntime& operator=(MySqlBinaryResultSetRuntime&& other) noexcept
        {
            if (this != &other)
            {
                stmt_       = other.stmt_;
                metadata_   = other.metadata_;
                fields_     = other.fields_;
                column_map_ = std::move(other.column_map_);
                bind_       = std::move(other.bind_);

                other.stmt_ = nullptr;
            }
            return *this;
        }

        inline std::optional<MySqlBinaryRowRuntime> next()
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

            return std::make_optional<MySqlBinaryRowRuntime>(stmt_, fields_, bind_, column_map_);
        }

        inline size_t column_count() const
        {
            return column_map_.size();
        }

        inline size_t get_row_count() const
        {
            return mysql_stmt_num_rows(stmt_);
        }

        inline size_t get_affected_row_count() const
        {
            return mysql_stmt_affected_rows(stmt_);
        }

        inline std::string_view column_name(size_t index) const
        {
            if (index < 0 || index >= column_count())
            {
                throw std::out_of_range("Column index out of range");
            }

            return fields_[index].name;
        }

        inline size_t column_index(std::string_view name) const
        {
            auto it = column_map_.find(name.data());
            return (it != column_map_.end()) ? it->second : static_cast<size_t>(-1);
        }
    };
}

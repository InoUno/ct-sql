
#pragma once

#include <array>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

#include <mysql.h>

namespace ct_sql
{
    class MySqlBinaryRowBase
    {
    protected:
        MYSQL_STMT* stmt_;
        MYSQL_FIELD* fields_;

        MYSQL_BIND& bind_;

    public:
        inline explicit MySqlBinaryRowBase(MYSQL_STMT* stmt, MYSQL_FIELD* fields, MYSQL_BIND& bind)
        : stmt_(stmt)
        , fields_(fields)
        , bind_(bind)
        {
        }

        template <typename T>
        inline T get_unchecked(size_t index) const
        {
            fetch_column(index);

            return parse_column<T>();
        }

        template <typename T>
        inline std::optional<T> get_opt_unchecked(size_t index) const
        {
            fetch_column(index);

            if (bind_.is_null_value)
            {
                return std::nullopt;
            }

            return { parse_column<T>() };
        }

        inline size_t byte_length_unchecked(size_t index) const
        {
            fetch_column(index);
            return *bind_.length;
        }

        template <typename T>
        inline size_t copy_to_unchecked(size_t index, T* target, size_t size) const
        {
            fetch_column(index);

            const size_t source_size = *bind_.length;
            const size_t copy_size   = std::min(source_size, size);

            std::memcpy(target, bind_.buffer, copy_size);

            return copy_size;
        }

        template <typename T, size_t N>
        inline size_t copy_to_unchecked(size_t index, T (&target)[N]) const
        {
            return copy_to_unchecked<T>(index, target, sizeof(T) * N);
        }

        template <typename T>
        inline size_t copy_to_unchecked(size_t index, std::vector<T>& target) const
        {
            fetch_column(index);

            const size_t source_size = *bind_.length;

            target.resize(source_size / sizeof(T));
            std::memcpy(target.data(), bind_.buffer, source_size);

            return source_size;
        }

    private:
        inline int fetch_column(size_t index) const
        {
            bind_.buffer_type   = fields_[index].type;
            bind_.buffer_length = CT_SQL_MAX_BUFFER_LENGTH;

            return mysql_stmt_fetch_column(stmt_, &bind_, static_cast<unsigned int>(index), 0);
        }

    private:
        template <typename T>
        inline T parse_column() const
        {
            if constexpr (std::is_same_v<T, std::string_view> || std::is_same_v<T, std::string>)
            {
                auto chars = reinterpret_cast<char*>(bind_.buffer);
                return T(chars, chars + *bind_.length);
            }
            else if constexpr (std::is_same_v<T, const char*>)
            {
                return reinterpret_cast<char*>(bind_.buffer);
            }
            else if constexpr (std::is_same_v<T, const void*>)
            {
                return reinterpret_cast<void*>(bind_.buffer);
            }
            else if constexpr (std::is_same_v<T, std::span<uint8_t>> || std::is_same_v<T, std::vector<uint8_t>>)
            {
                auto bytes = reinterpret_cast<uint8_t*>(bind_.buffer);
                return { bytes, bytes + *bind_.length };
            }
            else
            {
                return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(bind_.buffer));
            }
        }
    };
}

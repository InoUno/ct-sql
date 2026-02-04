
#pragma once

#include <array>
#include <chrono>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <vector>

#include <mysql.h>

#include "ct-sql/exception.h"

#ifdef WIN32
#define CT_SQL_MKTIME _mkgmtime
#else
#define CT_SQL_MKTIME timegm
#endif

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

        // Copyable
        MySqlBinaryRowBase(const MySqlBinaryRowBase&) = default;

        MySqlBinaryRowBase& operator=(const MySqlBinaryRowBase& other)
        {
            if (this != &other)
            {
                this->stmt_   = other.stmt_;
                this->fields_ = other.fields_;
                this->bind_   = other.bind_;
            }

            return *this;
        }

        // Movable
        inline MySqlBinaryRowBase(MySqlBinaryRowBase&& other) = default;

        inline MySqlBinaryRowBase& operator=(MySqlBinaryRowBase&& other)
        {
            this->stmt_   = other.stmt_;
            this->fields_ = other.fields_;
            this->bind_   = other.bind_;

            other.stmt_   = nullptr;
            other.fields_ = nullptr;

            return *this;
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
        inline static tm parse_tm_from_mysql_time(MYSQL_TIME* mt)
        {
            // Convert MYSQL_TIME to a tm struct
            struct tm t = { 0 };
            t.tm_year   = mt->year - 1900;
            t.tm_mon    = mt->month - 1;
            t.tm_mday   = mt->day;
            t.tm_hour   = mt->hour;
            t.tm_min    = mt->minute;
            t.tm_sec    = mt->second;

            return t;
        }

        inline static time_t parse_time_t_from_mysql_time(MYSQL_TIME* mt)
        {
            tm t = parse_tm_from_mysql_time(mt);

            // Uses _mkgmtime (Windows) or timegm (Linux) for UTC to avoid local timezone shifts
            time_t tt = CT_SQL_MKTIME(&t);
            return tt;
        }

        template <typename T>
        inline T parse_column() const
        {
            if constexpr (std::is_same_v<T, std::string_view> || std::is_same_v<T, std::string>)
            {
                // Strings
                auto chars = reinterpret_cast<char*>(bind_.buffer);
                return T(chars, chars + (*bind_.length));
            }
            else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>)
            {
                // Null-terminated char buffers
                return reinterpret_cast<char*>(bind_.buffer);
            }
            else if constexpr (std::is_same_v<T, const void*>)
            {
                return bind_.buffer;
            }
            else if constexpr (std::is_same_v<T, std::span<uint8_t>> || std::is_same_v<T, std::vector<uint8_t>>)
            {
                // Spans/vectors of bytes
                auto bytes = reinterpret_cast<uint8_t*>(bind_.buffer);
                return { bytes, bytes + (*bind_.length) };
            }
            else if constexpr (std::is_same_v<T, tm>)
            {
                // tm struct
                return parse_tm_from_mysql_time(static_cast<MYSQL_TIME*>(bind_.buffer));
            }
            else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>)
            {
                // System time-point

                // Ensure the buffer type actually contains time data
                if (bind_.buffer_type != MYSQL_TYPE_DATE && bind_.buffer_type != MYSQL_TYPE_DATETIME && bind_.buffer_type != MYSQL_TYPE_TIMESTAMP)
                {
                    // The field is not a time-related type.
                    throw CtSqlException(std::format("Type '%d' is can't be parsed as a time-point.", static_cast<int>(bind_.buffer_type)), 2);
                }

                auto mt = static_cast<MYSQL_TIME*>(bind_.buffer);
                auto tt = parse_time_t_from_mysql_time(mt);

                auto tp_sys = std::chrono::system_clock::from_time_t(tt) + std::chrono::microseconds(mt->second_part);

                return tp_sys;
            }

            else
            {
                // Parse out the corresponding type based on the buffer type
                switch (bind_.buffer_type)
                {
                case MYSQL_TYPE_TINY:
                    return static_cast<T>(*reinterpret_cast<char*>(bind_.buffer));
                case MYSQL_TYPE_SHORT:
                    return static_cast<T>(*reinterpret_cast<short*>(bind_.buffer));
                case MYSQL_TYPE_LONG:
                    return static_cast<T>(*reinterpret_cast<int*>(bind_.buffer));
                case MYSQL_TYPE_LONGLONG:
                    return static_cast<T>(*reinterpret_cast<long long*>(bind_.buffer));
                case MYSQL_TYPE_FLOAT:
                    return static_cast<T>(*reinterpret_cast<float*>(bind_.buffer));
                case MYSQL_TYPE_DOUBLE:
                    return static_cast<T>(*reinterpret_cast<double*>(bind_.buffer));

                // Date/timestamps
                case MYSQL_TYPE_DATE:
                case MYSQL_TYPE_DATETIME:
                case MYSQL_TYPE_TIMESTAMP:
                {
                    auto tt = parse_time_t_from_mysql_time(static_cast<MYSQL_TIME*>(bind_.buffer));
                    return static_cast<T>(tt);
                }

                // Decimals
                case MYSQL_TYPE_DECIMAL:
                case MYSQL_TYPE_NEWDECIMAL:
                {
                    // These are sent as null-terminated char buffers.
                    long double result = 0;
                    char* buffer_start = static_cast<char*>(bind_.buffer);
                    char* buffer_end   = buffer_start + (*bind_.length);

#if defined(__cpp_lib_to_chars) && !defined(__APPLE__)
                    // This branch is for compilers with full support
                    auto [ptr, ec] = std::from_chars(buffer_start, buffer_end, result);
                    if (ec == std::errc())
                    {
                        return static_cast<T>(result);
                    }
#else
                    // Fallback for Apple Clang and older libc++
                    char* end_ptr = nullptr;
                    result        = std::strtold(buffer_start, &end_ptr);

                    if (end_ptr != buffer_start)
                    {
                        return static_cast<T>(result);
                    }
#endif
                    else
                    {
                        // It could not parsed.
                        throw CtSqlException(std::format("Could not parse '%s' into a double long.", buffer_start), 1);
                    }
                }
                default:
                    break;
                }

                // Fallback to casting the buffer straight to the requested type
                return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(bind_.buffer));
            }
        }
    };
}

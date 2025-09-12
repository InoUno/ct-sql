
#pragma once

#include <cstring>
#include <span>
#include <vector>

#include <mysql.h>

namespace ct_sql
{
    class MySqlTextRowBase
    {
    protected:
        MYSQL_ROW row_;
        unsigned long* lengths_;

    public:
        inline explicit MySqlTextRowBase(MYSQL_ROW row, unsigned long* lengths)
        : row_(row)
        , lengths_(lengths)
        {
        }

        template <typename T>
        inline T get_unchecked(size_t index) const
        {
            if (this->row_[index] == nullptr)
            {
                return T();
            }

            return parse_column<T>(index);
        }

        template <typename T>
        inline std::optional<T> get_opt_unchecked(size_t index) const
        {
            if (this->row_[index] == nullptr)
            {
                return std::nullopt;
            }

            return { parse_column<T>(index) };
        }

        inline size_t byte_length_unchecked(size_t index) const
        {
            return this->lengths_[index];
        }

        template <typename T>
        inline size_t copy_to_unchecked(size_t index, T* target, size_t size) const
        {
            const size_t source_size = this->lengths_[index];
            const size_t copy_size   = std::min(source_size, size);

            std::memcpy(target, this->row_[index], copy_size);

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
            const size_t source_size = this->lengths_[index];

            target.resize(source_size / sizeof(T));
            std::memcpy(target.data(), this->row_[index], source_size);

            return source_size;
        }

    private:
        template <typename T>
        inline T parse_column(size_t index) const
        {
            if constexpr (std::is_same_v<T, std::string_view> || std::is_same_v<T, std::string>)
            {
                return T(this->row_[index], this->row_[index] + this->lengths_[index]);
            }
            else if constexpr (std::is_same_v<T, const char*>)
            {
                return reinterpret_cast<char*>(this->row_[index]);
            }
            else if constexpr (std::is_same_v<T, const void*>)
            {
                return reinterpret_cast<void*>(this->row_[index]);
            }
            else if constexpr (std::is_integral_v<T>)
            {
                return static_cast<T>(strtoul(this->row_[index], NULL, 10));
            }
            else if constexpr (std::is_enum_v<T>)
            {
                if constexpr (std::is_integral_v<std::underlying_type_t<T>>)
                {
                    return static_cast<T>(strtoul(this->row_[index], NULL, 10));
                }
            }
            else if constexpr (std::is_floating_point_v<T>)
            {
                return static_cast<T>(atof(this->row_[index]));
            }
            else if constexpr (std::is_same_v<T, std::span<uint8_t>> || std::is_same_v<T, std::vector<uint8_t>>)
            {
                const auto start = reinterpret_cast<uint8_t*>(this->row_[index]);
                return T(start, start + this->lengths_[index]);
            }
            else
            {
                static_assert(false, "Unsupported type");
                return T();
            }
        }
    };
}


#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <mysql.h>

#include "./binary_row_base.h"
#include "ct-sql/common.h"

namespace ct_sql
{
    template <typename Derived>
    class MySqlRuntimeRowBase
    {
    private:
        std::unordered_map<std::string, size_t>& column_map_;

        inline const Derived* as_derived() const
        {
            return static_cast<const Derived*>(this);
        }

    public:
        inline explicit MySqlRuntimeRowBase(std::unordered_map<std::string, size_t>& column_map)
        : column_map_(column_map)
        {
        }

        inline size_t column_count() const
        {
            return column_map_.size();
        }

        /*
         * Index getters
         */

        template <typename T>
        inline T get(size_t index) const
        {
            if (index < 0 || index >= column_count())
            {
                return T();
            }

            return as_derived()->template get_unchecked<T>(index);
        }

        inline auto get(size_t index) const
        {
            return get_index_proxy<const Derived> { as_derived(), index };
        }

        template <size_t Index>
        inline auto get() const
        {
            return this->get(Index);
        }

        template <typename T>
        inline std::optional<T> get_opt(size_t index) const
        {
            if (index < 0 || index >= column_count())
            {
                return std::nullopt;
            }

            return as_derived()->template get_opt_unchecked<T>(index);
        }

        inline auto get_opt(size_t index) const
        {
            return get_opt_index_proxy<const Derived> { as_derived(), index };
        }

        template <size_t Index>
        inline auto get_opt() const
        {
            return this->get_opt(Index);
        }

        template <size_t Index, typename T>
        inline std::optional<T> get_opt() const
        {
            return this->template get_opt<T>(Index);
        }

        /*
         * Column name getters
         */

        template <typename T>
        inline T get(std::string_view name) const
        {
            const auto index = column_map_.find(name.data());
            if (index == column_map_.end())
            {
                return T();
            }

            return as_derived()->template get_unchecked<T>(index->second);
        }

        inline auto get(std::string_view name) const
        {
            const auto index = column_map_.find(name.data());
            if (index == column_map_.end())
            {
                return get_index_proxy<const Derived> { as_derived(), static_cast<size_t>(-1) };
            }

            return get_index_proxy<const Derived> { as_derived(), index->second };
        }

        template <StringLiteral ColumnName, typename T>
        inline T get() const
        {
            return this->template get<T>(ColumnName.view());
        };

        template <StringLiteral ColumnName>
        inline auto get() const
        {
            return this->get(ColumnName.view());
        };

        template <StringLiteral ColumnName, typename T>
        inline std::optional<T> get_opt() const
        {
            return this->template get_opt<T>(ColumnName.view());
        };

        template <StringLiteral ColumnName>
        inline auto get_opt() const
        {
            return this->get_opt(ColumnName.view());
        };

        template <typename T>
        inline std::optional<T> get_opt(std::string_view name) const
        {
            const auto index = column_map_.find(name.data());
            if (index == column_map_.end())
            {
                return std::nullopt;
            }

            return as_derived()->template get_opt_unchecked<T>(index->second);
        }

        inline auto get_opt(std::string_view name) const
        {
            const auto index = column_map_.find(name.data());
            if (index == column_map_.end())
            {
                return get_opt_index_proxy<const Derived> { as_derived(), static_cast<size_t>(-1) };
            }

            return get_opt_index_proxy<const Derived> { as_derived(), index->second };
        }

        /*
         * Length
         */

        template <StringLiteral ColumnName>
        inline size_t byte_length() const
        {
            return byte_length(ColumnName.view());
        }

        template <size_t Index>
        inline size_t byte_length() const
        {
            return byte_length(Index);
        }

        inline std::optional<size_t> byte_length(size_t index) const
        {
            if (index < 0 || index >= column_count())
            {
                return std::nullopt;
            }

            return { as_derived()->byte_length_unchecked(index) };
        }

        inline std::optional<size_t> byte_length(std::string_view column) const
        {
            const auto index = column_map_.find(column.data());
            if (index == column_map_.end())
            {
                return std::nullopt;
            }

            return { as_derived()->byte_length_unchecked(index->second) };
        }

        /*
         * Copy to
         */

        template <StringLiteral ColumnName, typename T>
        inline size_t copy_to(T* target, size_t size) const
        {
            return copy_to<T>(ColumnName.view(), target, size);
        }

        template <StringLiteral ColumnName, typename T, size_t N>
        inline size_t copy_to(T (&target)[N]) const
        {
            return copy_to<ColumnName, T>(target, sizeof(T) * N);
        }

        template <StringLiteral ColumnName, typename T, size_t N>
        inline size_t copy_to(std::array<T, N>& target) const
        {
            return copy_to<ColumnName, T>(target.data(), sizeof(T) * N);
        }

        template <StringLiteral ColumnName, typename T>
        inline size_t copy_to(std::vector<T>& target) const
        {
            return copy_to<T>(ColumnName.view(), target);
        }

        template <typename T>
        inline size_t copy_to(std::string_view column, T* target, size_t size) const
        {
            const auto index = column_map_.find(column.data());
            if (index == column_map_.end())
            {
                return -1;
            }

            return as_derived()->template copy_to_unchecked<T>(index->second, target, size);
        }

        template <typename T, size_t N>
        inline size_t copy_to(std::string_view column, T (&target)[N]) const
        {
            return copy_to<T>(column, target, sizeof(T) * N);
        }

        template <typename T, size_t N>
        inline size_t copy_to(std::string_view column, std::array<T, N>& target) const
        {
            return copy_to<T>(column, target.data(), sizeof(T) * N);
        }

        template <typename T>
        inline size_t copy_to(std::string_view column, std::vector<T>& target) const
        {
            const auto index = column_map_.find(column.data());
            if (index == column_map_.end())
            {
                return -1;
            }

            return as_derived()->template copy_to_unchecked<T>(index->second, target);
        }

        template <size_t Index, typename T>
        inline size_t copy_to(T* target, size_t size) const
        {
            return copy_to<T>(Index, target, size);
        }

        template <size_t Index, typename T, size_t N>
        inline size_t copy_to(T (&target)[N]) const
        {
            return copy_to<T>(Index, target, sizeof(T) * N);
        }

        template <size_t Index, typename T, size_t N>
        inline size_t copy_to(std::array<T, N>& target) const
        {
            return copy_to<T>(Index, target.data(), sizeof(T) * N);
        }

        template <size_t Index, typename T>
        inline size_t copy_to(std::vector<T>& target) const
        {
            return copy_to<T>(Index, target);
        }

        template <typename T>
        inline size_t copy_to(size_t index, T* target, size_t size) const
        {
            if (index < 0 || index >= column_count())
            {
                return -1;
            }

            return as_derived()->template copy_to_unchecked<T>(index, target, size);
        }

        template <typename T, size_t N>
        inline size_t copy_to(size_t index, T (&target)[N]) const
        {
            return copy_to<T>(index, target, sizeof(T) * N);
        }

        template <typename T, size_t N>
        inline size_t copy_to(size_t index, std::array<T, N>& target) const
        {
            return copy_to<T>(index, target.data(), sizeof(T) * N);
        }

        template <typename T>
        inline size_t copy_to(size_t index, std::vector<T>& target) const
        {
            if (index < 0 || index >= column_count())
            {
                return -1;
            }

            return as_derived()->template copy_to_unchecked<T>(index, target);
        }
    };
}

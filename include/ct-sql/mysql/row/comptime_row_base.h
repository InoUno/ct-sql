
#pragma once

#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <mysql.h>

#include "./binary_row_base.h"
#include "ct-sql/common.h"
#include "ct-sql/query_parsing.h"

namespace ct_sql
{
    template <typename Derived, StringLiteral Query>
    class MySqlComptimeRowBase
    {
    protected:
        MYSQL_FIELD* fields_;
        std::unordered_map<std::string, size_t>& column_map_;

        using QueryColumns = ParseColumns<Query>;

        inline void ensure_populated_column_map() const
        {
            // Populate the column map if it's not been used before
            if (column_map_.empty())
            {
                auto num_fields = QueryColumns::column_count;
                for (size_t i = 0; i < num_fields; ++i)
                {
                    column_map_[fields_[i].name] = i;
                }
            }
        }

        inline const Derived* as_derived() const
        {
            return static_cast<const Derived*>(this);
        }

    public:
        inline explicit MySqlComptimeRowBase(MYSQL_FIELD* fields, std::unordered_map<std::string, size_t>& column_map)
        : fields_(fields)
        , column_map_(column_map)
        {
        }

        // Copyable
        MySqlComptimeRowBase(const MySqlComptimeRowBase&) = default;
        MySqlComptimeRowBase& operator=(const MySqlComptimeRowBase&) = default;

        // Movable
        inline MySqlComptimeRowBase(MySqlComptimeRowBase&& other) = default;
        inline MySqlComptimeRowBase& operator=(MySqlComptimeRowBase&& other) = default;

        static constexpr size_t column_count()
        {
            return QueryColumns::column_count;
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

        /*
         * Index getters
         */

        template <typename T>
        inline T get(size_t index) const
        {
            if (index < 0 || index >= QueryColumns::column_count)
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
            static_assert(Index >= 0 && Index < QueryColumns::column_count, "Index is out of range for the row.");

            return get_unchecked_index_proxy<const Derived> { as_derived(), Index };
        }

        template <typename T>
        inline std::optional<T> get_opt(size_t index) const
        {
            if (index < 0 || index >= QueryColumns::column_count)
            {
                return std::nullopt;
            }

            return as_derived()->template get_opt_unchecked<T>(index);
        }

        inline auto get_opt(size_t index) const
        {
            return get_opt_index_proxy<const Derived> { as_derived(), index };
        }

        template <size_t Index, typename T>
        inline std::optional<T> get_opt() const
        {
            static_assert(Index >= 0 && Index < QueryColumns::column_count, "Index is out of range for the query.");

            return as_derived()->template get_opt_unchecked<T>(Index);
        }

        template <size_t Index>
        inline auto get_opt() const
        {
            static_assert(Index >= 0 && Index < QueryColumns::column_count, "Index is out of range for the query.");

            return get_opt_unchecked_index_proxy<const Derived> { as_derived(), Index };
        }

        /*
         * Column name getters
         */

        template <StringLiteral ColumnName, typename T>
        inline T get() const
        {
            constexpr size_t index = QueryColumns::template column_index<ColumnName>();
            return as_derived()->template get_unchecked<T>(index);
        }

        template <StringLiteral ColumnName>
        inline auto get() const
        {
            return get_proxy<ColumnName, const Derived> { as_derived() };
        }

        template <typename T>
        inline T get(std::string_view column) const
        {
            ensure_populated_column_map();

            auto index = column_map_.find(column.data());
            if (index == column_map_.end())
            {
                return T();
            }
            return as_derived()->template get_unchecked<T>(index->second);
        }

        inline auto get(std::string_view column) const
        {
            ensure_populated_column_map();

            auto index = column_map_.find(column.data());
            if (index == column_map_.end())
            {
                return get_index_proxy<const Derived> { as_derived(), static_cast<size_t>(-1) };
            }
            return get_index_proxy<const Derived> { as_derived(), index->second };
        }

        template <StringLiteral ColumnName, typename T>
        inline std::optional<T> get_opt() const
        {
            constexpr size_t index = QueryColumns::template column_index<ColumnName>();
            return as_derived()->template get_opt_unchecked<T>(index);
        }

        template <StringLiteral ColumnName>
        inline auto get_opt() const
        {
            return get_opt_proxy<ColumnName, const Derived> { as_derived() };
        }

        template <typename T>
        inline std::optional<T> get_opt(std::string_view column) const
        {
            ensure_populated_column_map();

            auto index = column_map_.find(column.data());
            if (index == column_map_.end())
            {
                return std::nullopt;
            }

            return as_derived()->template get_opt_unchecked<T>(index->second);
        }

        inline auto get_opt(std::string_view column) const
        {
            ensure_populated_column_map();

            auto index = column_map_.find(column.data());
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
            constexpr size_t index = QueryColumns::template column_index<ColumnName>();
            return as_derived()->byte_length_unchecked(index);
        }

        template <size_t Index>
        inline size_t byte_length() const
        {
            static_assert(Index >= 0 && Index < QueryColumns::column_count, "Index is out of range for the query.");
            return as_derived()->byte_length_unchecked(Index);
        }

        inline std::optional<size_t> byte_length(size_t index) const
        {
            if (index < 0 || index >= QueryColumns::column_count)
            {
                return std::nullopt;
            }

            return { as_derived()->byte_length_unchecked(index) };
        }

        inline std::optional<size_t> byte_length(std::string_view column) const
        {
            ensure_populated_column_map();

            auto index = column_map_.find(column.data());
            if (index == column_map_.end())
            {
                return 0;
            }

            return { as_derived()->byte_length_unchecked(index->second) };
        }

        /*
         * Copy to
         */

        template <StringLiteral ColumnName, typename T>
        inline size_t copy_to(T* target, size_t size) const
        {
            constexpr size_t index = QueryColumns::template column_index<ColumnName>();
            return as_derived()->template copy_to_unchecked<T>(index, target, size);
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
            constexpr size_t index = QueryColumns::template column_index<ColumnName>();
            return as_derived()->template copy_to_unchecked<T>(index, target);
        }

        template <typename T>
        inline size_t copy_to(std::string_view column, T* target, size_t size) const
        {
            ensure_populated_column_map();

            auto index = column_map_.find(column.data());
            if (index == column_map_.end())
            {
                return -1;
            }

            return as_derived()->template copy_to_unchecked<T>(index, target, size);
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
            ensure_populated_column_map();

            auto index = column_map_.find(column.data());
            if (index == column_map_.end())
            {
                return -1;
            }

            return as_derived()->template copy_to_unchecked<T>(index, target);
        }

        template <size_t Index, typename T>
        inline size_t copy_to(T* target, size_t size) const
        {
            static_assert(Index >= 0 && Index < QueryColumns::column_count, "Index is out of range for the query.");
            return as_derived()->template copy_to_unchecked<T>(Index, target, size);
        }

        template <size_t Index, typename T, size_t N>
        inline size_t copy_to(T (&target)[N]) const
        {
            return copy_to<Index, T>(target, sizeof(T) * N);
        }

        template <size_t Index, typename T, size_t N>
        inline size_t copy_to(std::array<T, N>& target) const
        {
            return copy_to<Index, T>(target.data(), sizeof(T) * N);
        }

        template <size_t Index, typename T>
        inline size_t copy_to(std::vector<T>& target) const
        {
            static_assert(Index >= 0 && Index < QueryColumns::column_count, "Index is out of range for the query.");
            return as_derived()->template copy_to_unchecked<T>(Index, target);
        }

        template <typename T>
        inline size_t copy_to(size_t index, T* target, size_t size) const
        {
            if (index < 0 || index >= QueryColumns::column_count)
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
            if (index < 0 || index >= QueryColumns::column_count)
            {
                return -1;
            }

            return as_derived()->template copy_to_unchecked<T>(index, target);
        }
    };
}

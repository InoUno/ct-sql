#pragma once

#include <array>
#include <stdexcept>
#include <string_view>

#include "ct-sql/common.h"

namespace ct_sql
{
    // Helper to store column names extracted from SQL
    template <size_t MaxCols>
    struct ColumnExtractor
    {
        std::array<std::string_view, MaxCols> columns;
        size_t count = 0;

        constexpr void add_column(std::string_view col)
        {
            if (count < MaxCols)
            {
                columns[count++] = col;
            }
        }
    };

    // Safe constexpr character access
    template <size_t N>
    constexpr char safe_char_at(const char (&str)[N], size_t pos)
    {
        return (pos < N - 1) ? str[pos] : '\0';
    }

    constexpr char to_lower(char c)
    {
        return (c >= 'A' && c <= 'Z') ? c + 32 : c;
    }

    constexpr bool is_whitespace(char c)
    {
        return c == ' ' || c == '\t' || c == '\n';
    }

    template <size_t N, size_t M>
    constexpr size_t find_keyword(const char (&query)[N], const char (&keyword)[M], size_t start_pos = 0)
    {
        constexpr size_t len         = N - 1;
        constexpr size_t keyword_len = M - 1;

        for (size_t i = start_pos; i + keyword_len <= len; ++i)
        {
            bool match = true;
            for (size_t j = 0; j < keyword_len; ++j)
            {
                if (to_lower(safe_char_at(query, i + j)) != to_lower(keyword[j]))
                {
                    match = false;
                    break;
                }
            }

            if (match && (i + keyword_len >= len || is_whitespace(safe_char_at(query, i + keyword_len))))
            {
                return i + keyword_len;
            }
        }
        return -1;
    }

    constexpr bool starts_with_any_case(std::string_view str, std::string_view prefix, size_t offset = 0)
    {
        if (str.size() < prefix.size() + offset)
        {
            return false;
        }

        for (size_t i = 0; i < prefix.size(); i++)
        {
            const auto s_char = str[i + offset];
            const auto p_char = prefix[i];
            if (s_char == p_char)
            {
                continue;
            }

            // Try lowercase str char
            if (s_char + 32 == p_char && s_char >= 'A' && s_char <= 'Z')
            {
                continue;
            }

            // Try lowercase prefix char
            if (p_char + 32 == s_char && p_char >= 'A' && p_char <= 'Z')
            {
                continue;
            }

            // Did not match
            return false;
        }

        return true;
    }

    template <size_t N>
    constexpr size_t skip_whitespace(const char (&query)[N], size_t pos)
    {
        constexpr size_t len = N - 1;
        while (pos < len && is_whitespace(safe_char_at(query, pos)))
        {
            pos++;
        }
        return pos;
    }

    template <size_t N>
    constexpr std::pair<size_t, size_t> trim_bounds(const char (&query)[N], size_t start, size_t end)
    {
        // Trim leading whitespace
        while (start < end && is_whitespace(safe_char_at(query, start)))
        {
            start++;
        }
        // Trim trailing whitespace
        while (end > start && is_whitespace(safe_char_at(query, end - 1)))
        {
            end--;
        }
        return { start, end };
    }

    template <size_t N>
    constexpr size_t extract_column_name(const char (&query)[N], size_t col_start, size_t col_end)
    {
        // Handle aliases (find AS keyword from the back)
        for (size_t i = col_end - 2; i > col_start; --i)
        {
            if (to_lower(safe_char_at(query, i)) == 'a' && to_lower(safe_char_at(query, i + 1)) == 's' && (i + 2 >= col_end || is_whitespace(safe_char_at(query, i + 2))))
            {
                return skip_whitespace(query, i + 2);
            }
        }

        // If no AS, handle columns with the table name in them, which is separated by a dot
        size_t last_dot = -1;
        for (size_t i = col_start; i < col_end; ++i)
        {
            if (safe_char_at(query, i) == '.')
            {
                last_dot = i;
            }
        }
        return (last_dot != -1 && last_dot + 1 < col_end) ? last_dot + 1 : col_start;
    }

    template <StringLiteral Query>
    constexpr size_t count_max_possible_columns()
    {
        size_t count = 1;
        for (size_t pos = 0; pos <= Query.size(); ++pos)
        {
            if (safe_char_at(Query.value, pos) == ',')
            {
                count++;
            }
        }

        return count;
    }

    template <size_t MaxCols, size_t N>
    constexpr auto parse_select_columns(const char (&query)[N])
    {
        ColumnExtractor<MaxCols> extractor;
        constexpr size_t len = N - 1;

        // Find SELECT keyword
        size_t select_pos = find_keyword(query, "select");
        if (select_pos == -1)
        {
            return extractor;
        }

        // Skip whitespace after SELECT
        select_pos = skip_whitespace(query, select_pos);

        // Find FROM keyword
        size_t from_pos = find_keyword(query, "from", select_pos);
        if (from_pos == -1)
        {
            // No from, but could be a SELECT query without a table, so position at the end.
            from_pos = N - 1;
        }
        else
        {
            // Back to start of "FROM"
            from_pos -= 4;
        }

        // Parse comma-separated columns between SELECT and FROM
        size_t start = select_pos;
        for (size_t pos = select_pos; pos <= from_pos; ++pos)
        {
            if (pos == from_pos || safe_char_at(query, pos) == ',')
            {
                if (pos > start)
                {
                    auto [col_start, col_end] = trim_bounds(query, start, pos);
                    if (col_end > col_start)
                    {
                        col_start = extract_column_name(query, col_start, col_end);
                        if (col_end > col_start)
                        {
                            std::string_view col_name { query + col_start, col_end - col_start };
                            extractor.add_column(col_name);
                        }
                    }
                }
                start = pos + 1;
            }
        }

        return extractor;
    }

    template <size_t N>
    constexpr auto parse_bind_params(const char (&query)[N])
    {
        size_t count         = 0;
        constexpr size_t len = N - 1;

        for (size_t pos = 0; pos <= len; ++pos)
        {
            if (safe_char_at(query, pos) == '?')
            {
                count++;
            }
        }

        return count;
    }

    // Parse out columns from the query given as string literal template argument
    template <StringLiteral Query>
    struct ParseColumns
    {
        static constexpr auto parsed         = parse_select_columns<count_max_possible_columns<Query>()>(Query.value);
        static constexpr size_t column_count = parsed.count;

        template <StringLiteral ColumnName>
        static constexpr size_t column_index()
        {
            // Use a lambda to wrap the loop and make the result constexpr
            constexpr auto get_index = []() constexpr
            {
                constexpr auto columns = parsed.columns;
                for (size_t i = 0; i < column_count; ++i)
                {
                    if (columns[i] == ColumnName.view())
                    {
                        return i;
                    }
                }
                return static_cast<size_t>(-1);
            };

            constexpr size_t found_index = get_index();

            if constexpr (found_index == static_cast<size_t>(-1))
            {
                static_assert(false, "Column does not exist in query.");
            }

            return found_index;
        }

        template <size_t ColumnIndex>
        static constexpr std::string_view column_name()
        {
            if constexpr (ColumnIndex < 0 || ColumnIndex >= column_count)
            {
                static_assert(false, "Column does not exist in query.");
            }

            return parsed.columns[ColumnIndex];
        }

        static constexpr bool has_wildcard_column()
        {
            constexpr auto columns = parsed.columns;
            for (size_t i = 0; i < column_count; ++i)
            {
                if (columns[i] == "*")
                {
                    return true;
                }
            }
            return false;
        }

        static constexpr size_t bind_param_count()
        {
            return parse_bind_params(Query.value);
        }

        static constexpr auto get_columns()
        {
            return parsed.columns;
        }
    };
}

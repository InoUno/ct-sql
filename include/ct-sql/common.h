#pragma once

#ifndef CT_SQL_MAX_BUFFER_LENGTH
#define CT_SQL_MAX_BUFFER_LENGTH 2048
#endif // !CT_SQL_MAX_BUFFER_LENGTH

namespace ct_sql
{
    template <size_t N>
    struct StringLiteral;

    template <size_t N1, size_t N2>
    static constexpr StringLiteral<N1 + N2 - 1> ConcatLiterals(const char (&str1)[N1], const char (&str2)[N2]);

    // Compile-time string literal wrapper
    template <size_t N>
    struct StringLiteral
    {
        constexpr StringLiteral(const char (&str)[N])
        {
            std::copy_n(str, N, value);
        }

        constexpr StringLiteral()
        {
        }

        template <size_t N2>
        constexpr ct_sql::StringLiteral<N + N2 - 1> append(const char (&str2)[N2]) const
        {
            return ConcatLiterals(this->value, str2);
        }

        char value[N];
        constexpr size_t size() const
        {
            return N - 1;
        }

        constexpr const char* data() const
        {
            return value;
        }

        constexpr std::string_view view() const
        {
            return { value, N - 1 };
        }
    };

    template <size_t N1, size_t N2>
    static constexpr StringLiteral<N1 + N2 - 1> ConcatLiterals(const char (&str1)[N1], const char (&str2)[N2])
    {
        StringLiteral<N1 + N2 - 1> literal;
        std::copy_n(str1, N1 - 1, literal.value);
        std::copy_n(str2, N2, literal.value + N1 - 1);
        return literal;
    }

    /*
     * Proxies for getters on rows
     */

    template <StringLiteral ColumnName, typename Row>
    struct get_proxy
    {
        Row* row;

        template <typename T>
        inline operator T() const
        {
            return row->template get<ColumnName, T>();
        }
    };

    template <typename Row>
    struct get_index_proxy
    {
        Row* row;
        size_t index;

        template <typename T>
        inline operator T() const
        {
            return row->template get<T>(index);
        }
    };

    template <typename Row>
    struct get_unchecked_index_proxy
    {
        Row* row;
        size_t index;

        template <typename T>
        inline operator T() const
        {
            return row->template get_unchecked<T>(index);
        }
    };

    template <StringLiteral ColumnName, typename Row>
    struct get_opt_proxy
    {
        Row* row;

        template <typename T>
        inline operator std::optional<T>() const
        {
            return row->template get_opt<ColumnName, T>();
        }
    };

    template <typename Row>
    struct get_opt_index_proxy
    {
        Row* row;
        size_t index;

        template <typename T>
        inline operator std::optional<T>() const
        {
            return row->template get_opt<T>(index);
        }
    };

    template <typename Row>
    struct get_opt_unchecked_index_proxy
    {
        Row* row;
        size_t index;

        template <typename T>
        inline operator std::optional<T>() const
        {
            return row->template get_opt_unchecked<T>(index);
        }
    };

    // A general-purpose trait to check if a type is a pointer (smart or raw)
    template <typename T>
    struct is_pointer_like : std::is_pointer<T>
    {
    };

    template <typename T, typename Deleter>
    struct is_pointer_like<std::unique_ptr<T, Deleter>> : std::true_type
    {
    };

    template <typename T>
    struct is_pointer_like<std::shared_ptr<T>> : std::true_type
    {
    };

    template <typename T>
    struct is_pointer_like<std::weak_ptr<T>> : std::true_type
    {
    };

    // Alias for convenience
    template <typename T>
    inline constexpr bool is_pointer_like_v = is_pointer_like<T>::value;

    template <typename TResult, typename TError = std::string>
    struct CtResult
    {
        TResult result;
        TError error;

        inline CtResult(TResult _result)
        : result(std::move(_result))
        , error(TError())
        {
        }

        inline CtResult(TError _error)
        : result(TResult())
        , error(std::move(_error))
        {
        }

        // Overload for the arrow operator, enabled only for pointer-like types
        template <typename U = TResult, std::enable_if_t<is_pointer_like_v<U>, int> = 0>
        auto operator->()
        {
            if constexpr (std::is_pointer_v<U>)
            {
                return result;
            }
            else
            {
                return result.get();
            }
        }

        template <typename U = TResult, std::enable_if_t<is_pointer_like_v<U>, int> = 0>
        const auto operator->() const
        {
            if constexpr (std::is_pointer_v<U>)
            {
                return result;
            }
            else
            {
                return result.get();
            }
        }

        template <typename U = TResult, std::enable_if_t<is_pointer_like_v<U>, int> = 0>
        auto& operator*()
        {
            return *result;
        }

        template <typename U = TResult, std::enable_if_t<is_pointer_like_v<U>, int> = 0>
        const auto& operator*() const
        {
            return *result;
        }

        inline operator bool() const
        {
            return static_cast<bool>(result);
        }

        inline operator TResult()
        {
            return std::move(result);
        }

        inline TResult& operator*()
        {
            return result;
        }

        inline const TResult& operator*() const
        {
            return result;
        }

        inline bool operator==(const TResult& other) const
        {
            return result == other;
        }

        inline bool operator==(const CtResult& other) const
        {
            return result == other.result;
        }
    };
}

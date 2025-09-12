#pragma once

#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

#include <errmsg.h>
#include <mysql.h>

#include "./result_set/binary_result_set.h"
#include "./result_set/binary_result_set_runtime.h"
#include "./result_set/text_result_set.h"
#include "./result_set/text_result_set_runtime.h"

namespace ct_sql
{
    constexpr size_t MaxReconnectAttempts = 1;

    class MySqlConnection
    {
    private:
        MYSQL handle_;

        /// Statement cache for static strings that can be looked up via their pointer
        std::unordered_map<const char*, MYSQL_STMT*> cache_;
        /// Statement cache for dynamic strings
        std::unordered_map<std::string, MYSQL_STMT*> cache_strs_;

        // Connection information to enable reconnects
        std::string user_, password_, host_, db_;
        unsigned int port_ = 0;

        bool allow_reconnects_ = true;

    public:
        MySqlConnection()
        {
            mysql_init(&this->handle_);
        }

        ~MySqlConnection()
        {
            close_all_statements();
            mysql_close(&this->handle_);
        }

        MYSQL* get_handle()
        {
            return &this->handle_;
        }

        inline bool connect(const char* user, const char* password, const char* host, uint16_t port, const char* db)
        {
            user_     = user ? user : "";
            password_ = password ? password : "";
            host_     = host ? host : "";
            port_     = static_cast<unsigned int>(port);
            db_       = db ? db : "";

            return reconnect();
        }

        inline bool reconnect()
        {
            close_all_statements();
            if (!mysql_real_connect(&this->handle_, host_.data(), user_.data(), password_.data(), db_.data(), port_, nullptr, 0))
            {
                return false;
            }
            return true;
        }

        void set_allow_reconnects(bool allow = true)
        {
            allow_reconnects_ = allow;
        }

        template <StringLiteral Query>
        inline auto execute()
        {
            using Columns = ParseColumns<Query>;

            if constexpr (!Columns::has_wildcard_column())
            {
                // With no wildcard columns, we can parse the resulting columns at compile-time.
                auto res = base_text_query(Query.view());
                if (!res.error.empty())
                {
                    return CtResult<std::unique_ptr<MySqlTextResultSet<Query>>>(res.error);
                }
                else if (!res.result)
                {
                    return CtResult<std::unique_ptr<MySqlTextResultSet<Query>>>(std::make_unique<MySqlTextResultSet<Query>>(mysql_affected_rows(&this->handle_)));
                }
                return CtResult<std::unique_ptr<MySqlTextResultSet<Query>>>(std::make_unique<MySqlTextResultSet<Query>>(res.result));
            }
            else
            {
                // If there's a wildcard column, we can't determine the amount of resulting columns at compile-time,
                // so delegate it to runtime checks.
                auto res = base_text_query(Query.view());
                if (!res.error.empty())
                {
                    return CtResult<std::unique_ptr<MySqlTextResultSetRuntime>>(res.error);
                }
                else if (!res.result)
                {
                    return CtResult<std::unique_ptr<MySqlTextResultSetRuntime>>(std::make_unique<MySqlTextResultSetRuntime>(mysql_affected_rows(&this->handle_)));
                }
                return CtResult<std::unique_ptr<MySqlTextResultSetRuntime>>(std::make_unique<MySqlTextResultSetRuntime>(res.result));
            }
        }

        template <StringLiteral Query, typename... Args>
        inline auto prepared_uncached(Args&&... args)
        {
            return prepared<Query, false, Args...>(args...);
        }

        template <StringLiteral Query, bool DoCache = true, typename... Args>
        inline auto prepared(Args&&... args)
        {
            using Columns = ParseColumns<Query>;

            // Validate correct amount of bind parameters at compile-time
            constexpr size_t arg_count        = sizeof...(args);
            constexpr size_t bind_param_count = parse_bind_params(Query.value);
            static_assert(arg_count == bind_param_count, "Incorrect amount of bind parameters given to prepared SQL query");

            if constexpr (!Columns::has_wildcard_column())
            {
                // With no wildcard columns, we can parse the resulting columns at compile-time.
                auto res = base_prepared_query<false>(Query.data(), DoCache, args...);
                if (!res.error.empty())
                {
                    return CtResult<std::unique_ptr<MySqlBinaryResultSet<Query>>>(res.error);
                }
                return CtResult<std::unique_ptr<MySqlBinaryResultSet<Query>>>(std::make_unique<MySqlBinaryResultSet<Query>>(res));
            }
            else
            {
                // If there's a wildcard column, we can't determine the amount of resulting columns at compile-time,
                // so delegate it to runtime checks.
                auto res = base_prepared_query<false>(Query.data(), DoCache, args...);
                if (!res.error.empty())
                {
                    return CtResult<std::unique_ptr<MySqlBinaryResultSetRuntime>>(res.error);
                }
                return CtResult<std::unique_ptr<MySqlBinaryResultSetRuntime>>(std::make_unique<MySqlBinaryResultSetRuntime>(res));
            }
        }

        template <typename TString, typename... Args>
        inline CtResult<std::unique_ptr<MySqlBinaryResultSetRuntime>> prepared_uncached(TString query, Args&&... args)
        {
            return prepared<TString, false, Args...>(query, args...);
        }

        template <typename TString, bool DoCache = true, typename... Args>
        inline CtResult<std::unique_ptr<MySqlBinaryResultSetRuntime>> prepared(TString query, Args&&... args)
        {
            auto res = base_prepared_query<true>(query, DoCache, args...);
            if (!res.error.empty())
            {
                return { res.error };
            }
            return std::make_unique<MySqlBinaryResultSetRuntime>(res);
        }

        inline CtResult<std::unique_ptr<MySqlTextResultSetRuntime>> execute(std::string_view query)
        {
            auto res = base_text_query(query);
            if (!res.error.empty())
            {
                return res.error;
            }
            else if (!res.result)
            {
                return std::make_unique<MySqlTextResultSetRuntime>(mysql_affected_rows(&this->handle_));
            }
            return std::make_unique<MySqlTextResultSetRuntime>(res.result);
        }

        uint32_t get_timeout()
        {
            auto res = execute("SHOW VARIABLES LIKE 'wait_timeout';");
            if (auto row = res->next())
            {
                return row->get(1);
            }
            return 3600;
        }

        inline bool ping()
        {
            return mysql_ping(&this->handle_) == 0;
        }

        bool start_transaction()
        {
            if (this->execute("SET @@autocommit = 0;"))
            {
                return false;
            }

            if (this->execute("START TRANSACTION;"))
            {
                this->execute("SET @@autocommit = 1;");
                return false;
            }

            return true;
        }

        bool commit_transaction()
        {
            auto result = mysql_commit(&this->handle_);
            this->execute("SET @@autocommit = 1;");
            return result;
        }

        bool rollback_transaction()
        {
            auto result = this->execute("ROLLBACK;");
            this->execute("SET @@autocommit = 1;");
            return result != nullptr;
        }

        inline static std::unique_ptr<MySqlConnection> make(const char* user, const char* passwd, const char* host, uint16_t port, const char* db)
        {
            auto conn = std::make_unique<MySqlConnection>();
            if (!conn->connect(user, passwd, host, port, db))
            {
                auto error_msg = mysql_error(&conn->handle_);
                throw std::runtime_error(error_msg);
            }

            return conn;
        };

        inline size_t get_affected_row_count()
        {
            return mysql_affected_rows(&this->handle_);
        }

        template <StringLiteral Query>
        bool close_statement()
        {
            if (auto stmt = cache_.find(Query.data()); stmt != cache_.end())
            {
                mysql_stmt_close(stmt->second);
                cache_.erase(stmt);
                return true;
            }
            return false;
        }

        bool close_statement(const char* query)
        {
            if (auto stmt = cache_.find(query); stmt != cache_.end())
            {
                mysql_stmt_close(stmt->second);
                cache_.erase(stmt);
                return true;
            }
            return false;
        }

        bool close_statement(const std::string& query)
        {
            if (auto stmt = cache_strs_.find(query); stmt != cache_strs_.end())
            {
                mysql_stmt_close(stmt->second);
                cache_strs_.erase(stmt);
                return true;
            }
            return false;
        }

        inline void close_all_statements()
        {
            for (auto const& [key, val] : cache_)
            {
                mysql_stmt_close(val);
            }
            cache_.clear();

            for (auto const& [key, val] : cache_strs_)
            {
                mysql_stmt_close(val);
            }
            cache_strs_.clear();
        }

    private:
        inline CtResult<MYSQL_RES*> base_text_query(std::string_view query)
        {
            size_t attempts = 0;
            while (attempts <= MaxReconnectAttempts)
            {
                if (mysql_real_query(&this->handle_, query.data(), (unsigned long)query.size()))
                {
                    if (allow_reconnects_ && is_connection_lost(mysql_errno(&this->handle_)))
                    {
                        // Reconnect and retry the query
                        if (!reconnect())
                        {
                            return std::string(mysql_error(&this->handle_));
                        }
                        attempts++;
                        continue;
                    }

                    return std::string(mysql_error(&this->handle_));
                }

                MYSQL_RES* raw_result = mysql_store_result(&this->handle_);
                if (auto err_code = mysql_errno(&this->handle_))
                {
                    // Only try to reconnect if it is a safe query for retries
                    if (allow_reconnects_ && is_connection_lost(err_code) && is_safe_retry_query(query))
                    {
                        // Reconnect and retry the query
                        if (!reconnect())
                        {
                            return std::string(mysql_error(&this->handle_));
                        }
                        attempts++;
                        continue;
                    }

                    // Try to free the result and return the error
                    std::string msg = mysql_error(&this->handle_);
                    mysql_free_result(raw_result);
                    return msg;
                }

                return raw_result;
            }

            return std::string(mysql_error(&this->handle_));
        }

        template <bool CheckArgCount = false, typename... Args>
        inline CtResult<MYSQL_STMT*> base_prepared_query(const char* query, bool do_cache, Args&&... args)
        {
            auto stmt = get_prepared_statement(query, do_cache);
            if (!stmt)
            {
                return stmt;
            }

            return base_prepared_query_inner<CheckArgCount>(query, stmt, args...);
        }

        template <bool CheckArgCount = false, typename... Args>
        inline CtResult<MYSQL_STMT*> base_prepared_query(const std::string& query, bool do_cache, Args&&... args)
        {
            auto stmt = get_prepared_statement(query, do_cache);
            if (!stmt)
            {
                return stmt;
            }

            return base_prepared_query_inner<CheckArgCount>(query.data(), stmt, args...);
        }

        template <bool CheckArgCount = false, typename... Args>
        inline CtResult<MYSQL_STMT*> base_prepared_query_inner(const char* query, MYSQL_STMT* stmt, Args&&... args)
        {
            if constexpr (CheckArgCount)
            {
                constexpr size_t arg_count = sizeof...(args);
                auto param_count           = mysql_stmt_param_count(stmt);
                if (arg_count != param_count)
                {
                    // Incorrect amount of bind parameters given to prepared SQL query
                    return std::string("Incorrect amount of parameters passed to prepared query.");
                }
            }

            if constexpr (sizeof...(Args) > 0)
            {
                // Use a fixed-size array on the stack for the binds, since the number of arguments is known at compile-time.
                MYSQL_BIND binds[sizeof...(Args)];

                // Use a vector to manage the lifetime of temporary string data, as the number of strings is not known at compile-time.
                std::vector<std::string> str_data_holder;
                size_t index = 0;

                // Build the binds array recursively
                bind_params_recursive(binds, index, str_data_holder, args...);

                // Bind all parameters at once
                if (mysql_stmt_bind_param(stmt, binds))
                {
                    std::string msg = mysql_error(&this->handle_);
                    mysql_stmt_close(stmt);
                    return msg;
                }
            }

            size_t attempts = 0;
            while (attempts <= MaxReconnectAttempts)
            {
                // Execute the statement
                if (mysql_stmt_execute(stmt))
                {
                    if (allow_reconnects_ && is_connection_lost(mysql_errno(&this->handle_)))
                    {
                        // Reconnect and retry the query
                        if (!reconnect())
                        {
                            std::string msg = mysql_error(&this->handle_);
                            mysql_stmt_close(stmt);
                            return msg;
                        }
                        attempts++;
                        continue;
                    }

                    std::string msg = mysql_error(&this->handle_);
                    mysql_stmt_close(stmt);
                    return msg;
                }

                // Store the result set on the client-side
                if (mysql_stmt_store_result(stmt))
                {
                    // Only reconnect if it is a safe query for retries
                    if (allow_reconnects_ && is_connection_lost(mysql_errno(&this->handle_)) && is_safe_retry_query(query))
                    {
                        // Reconnect and retry the query
                        if (!reconnect())
                        {
                            std::string msg = mysql_error(&this->handle_);
                            mysql_stmt_close(stmt);
                            return msg;
                        }
                        attempts++;
                        continue;
                    }

                    std::string msg = mysql_error(&this->handle_);
                    mysql_stmt_close(stmt);
                    return msg;
                }

                return stmt;
            }

            return std::string(mysql_error(&this->handle_));
        }

        // Lookup const char* strings in pointer cache
        inline CtResult<MYSQL_STMT*, std::string> get_prepared_statement(const char* sql_query, bool do_cache = true)
        {
            if (auto res = cache_.find(sql_query); res != cache_.end())
            {
                return res->second;
            }

            auto stmt = mysql_stmt_init(&this->handle_);
            if (!stmt)
            {
                return std::string(mysql_error(&this->handle_));
            }

            size_t attempts = 0;
            while (attempts <= MaxReconnectAttempts)
            {
                if (mysql_stmt_prepare(stmt, sql_query, static_cast<unsigned long>(strlen(sql_query))))
                {
                    if (allow_reconnects_ && is_connection_lost(mysql_errno(&this->handle_)))
                    {
                        // Reconnect and retry the query
                        if (!reconnect())
                        {
                            std::string msg = mysql_error(&this->handle_);
                            mysql_stmt_close(stmt);
                            return msg;
                        }
                        attempts++;
                        continue;
                    }

                    std::string msg = mysql_error(&this->handle_);
                    mysql_stmt_close(stmt);
                    return msg;
                }

                if (do_cache)
                {
                    cache_[sql_query] = stmt;
                }
                return stmt;
            }

            std::string msg = mysql_error(&this->handle_);
            mysql_stmt_close(stmt);
            return msg;
        }

        // Lookup std::strings in string cache
        inline CtResult<MYSQL_STMT*, std::string> get_prepared_statement(const std::string& sql_query, bool do_cache = true)
        {
            if (auto res = cache_strs_.find(sql_query); res != cache_strs_.end())
            {
                return res->second;
            }

            auto stmt = mysql_stmt_init(&this->handle_);
            if (!stmt)
            {
                return std::string(mysql_error(&this->handle_));
            }

            size_t attempts = 0;
            while (attempts <= MaxReconnectAttempts)
            {
                if (mysql_stmt_prepare(stmt, sql_query.data(), static_cast<unsigned long>(sql_query.size())))
                {
                    if (allow_reconnects_ && is_connection_lost(mysql_errno(&this->handle_)))
                    {
                        // Reconnect and retry the query
                        if (!reconnect())
                        {
                            std::string msg = mysql_error(&this->handle_);
                            mysql_stmt_close(stmt);
                            return msg;
                        }
                        attempts++;
                        continue;
                    }

                    std::string msg = mysql_error(&this->handle_);
                    mysql_stmt_close(stmt);
                    return msg;
                }

                if (do_cache)
                {
                    cache_strs_[sql_query] = stmt;
                }
                return stmt;
            }

            std::string msg = mysql_error(&this->handle_);
            mysql_stmt_close(stmt);
            return msg;
        }

        // Base case for the recursion: when all parameters have been processed.
        inline void bind_params_recursive(MYSQL_BIND* binds, size_t& index, std::vector<std::string>& str_data_holder)
        {
        }

        // Recursive case: processes one parameter and then calls itself with the rest.
        template <typename First, typename... Rest>
        inline void bind_params_recursive(MYSQL_BIND* binds, size_t& index, std::vector<std::string>& str_data_holder, const First& current_param, const Rest&... rest)
        {
            MYSQL_BIND& bind = binds[index];
            std::memset(&bind, 0, sizeof(bind));

            if constexpr (std::is_same_v<First, const char*>)
            {
                // const char* can be used directly as the buffer, since it's lifetime is static
                bind.buffer_type   = MYSQL_TYPE_STRING;
                bind.buffer        = (void*)current_param;
                bind.buffer_length = strlen(current_param);
            }
            else if constexpr (std::is_same_v<First, std::string> || std::is_same_v<First, std::string_view>)
            {
                // Store a copy of the string to a vector to ensure its lifetime
                str_data_holder.push_back(current_param);
                bind.buffer_type   = MYSQL_TYPE_STRING;
                bind.buffer        = (void*)str_data_holder.back().c_str();
                bind.buffer_length = str_data_holder.back().length();
            }
            else if constexpr (std::is_same_v<First, char> || std::is_same_v<First, unsigned char>)
            {
                bind.buffer_type = MYSQL_TYPE_TINY;
                bind.buffer      = const_cast<void*>(reinterpret_cast<const void*>(&current_param));
            }
            else if constexpr (std::is_same_v<First, short> || std::is_same_v<First, unsigned short>)
            {
                bind.buffer_type = MYSQL_TYPE_SHORT;
                bind.buffer      = const_cast<void*>(reinterpret_cast<const void*>(&current_param));
            }
            else if constexpr (std::is_same_v<First, int> || std::is_same_v<First, unsigned int>)
            {
                bind.buffer_type = MYSQL_TYPE_LONG;
                bind.buffer      = const_cast<void*>(reinterpret_cast<const void*>(&current_param));
            }
            else if constexpr (std::is_same_v<First, long> || std::is_same_v<First, long long> || std::is_same_v<First, unsigned long> || std::is_same_v<First, unsigned long long>)
            {
                bind.buffer_type = MYSQL_TYPE_LONGLONG;
                bind.buffer      = const_cast<void*>(reinterpret_cast<const void*>(&current_param));
            }
            else if constexpr (std::is_same_v<First, float>)
            {
                bind.buffer_type = MYSQL_TYPE_FLOAT;
                bind.buffer      = const_cast<void*>(reinterpret_cast<const void*>(&current_param));
            }
            else if constexpr (std::is_same_v<First, double>)
            {
                bind.buffer_type = MYSQL_TYPE_DOUBLE;
                bind.buffer      = const_cast<void*>(reinterpret_cast<const void*>(&current_param));
            }
            else
            {
                // Handle unsupported types by raising an error
                static_assert(std::is_same_v<First, void*>, "Unsupported parameter type detected.");
            }

            if constexpr (std::is_unsigned_v<First>)
            {
                bind.is_unsigned = true;
            }

            index++;
            bind_params_recursive(binds, index, str_data_holder, rest...);
        }

        // Check if a query is safe to retry (i.e. non-modifying)
        static constexpr bool is_safe_retry_query(std::string_view query)
        {
            if (query.empty())
            {
                return false;
            }

            // Skip any non-ascii chars at the start
            size_t start = 0;
            while (start < query.size()
                   && (!(query[start] >= 'a' && query[start] <= 'z')
                       || !(query[start] >= 'A' && query[start] <= 'Z')))
            {
                start++;
            }

            return starts_with_any_case(query, "select", start)
                   || starts_with_any_case(query, "show", start)
                   || starts_with_any_case(query, "describe", start)
                   || starts_with_any_case(query, "explain", start)
                   || starts_with_any_case(query, "check", start);
        }

        // Check if the error code indicates that the connection was lost
        static bool is_connection_lost(int err_code)
        {
            return err_code == CR_SERVER_LOST || err_code == CR_SERVER_GONE_ERROR;
        }
    };
}

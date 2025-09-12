
#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "ct-sql.h"

#include "./matchers.h"

using namespace ct_sql;
using namespace testing;
using namespace std::chrono_literals;

std::unique_ptr<MySqlConnection> g_conn;

static std::unique_ptr<MySqlConnection> get_db_connection(const char* db = nullptr)
{
    const auto user     = std::getenv("CT_SQL_USER");
    const auto password = std::getenv("CT_SQL_PASSWORD");
    const auto host     = std::getenv("CT_SQL_HOST");
    const auto port     = std::getenv("CT_SQL_PORT");

    try
    {
        return MySqlConnection::make(user, password, host, port ? atoi(port) : 0, db);
    }
    catch (std::runtime_error err)
    {
        std::cerr << "Could not connect to database: " << err.what() << std::endl;
        exit(1);
    }
};

TEST(Select, Ints)
{
    auto res = g_conn->execute("SELECT tiny, small, big FROM __ct_sql_test_rows LIMIT 1;");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->get_row_count(), 1);

    auto row = res->next();
    ASSERT_TRUE(row);

    EXPECT_EQ((row->get<"tiny", char>()), 1);
    EXPECT_EQ((row->get<"small", short>()), 2);
    EXPECT_EQ((row->get<"big", long>()), 3);
}

TEST(Select, StringsAndFloats)
{
    auto res = g_conn->prepared<"SELECT firstname, floaty FROM __ct_sql_test_rows LIMIT 1;">();
    ASSERT_TRUE(res);
    ASSERT_EQ(res->get_row_count(), 1);

    auto row = res->next();
    ASSERT_TRUE(row);

    // Positive tests for string
    EXPECT_THAT(row->get<"firstname">(), IsStrSame("John"));
    EXPECT_THAT(row->get("firstname"), IsStrSame("John"));
    EXPECT_THAT(row->get<0>(), IsStrSame("John"));
    EXPECT_THAT(row->get(0), IsStrSame("John"));

    // Negative tests for string
    EXPECT_THAT(row->get<"firstname">(), Not(IsStrSame("Doe")));
    EXPECT_THAT(row->get("firstname"), Not(IsStrSame("Doe")));
    EXPECT_THAT(row->get<0>(), Not(IsStrSame("Doe")));
    EXPECT_THAT(row->get(0), Not(IsStrSame("Doe")));

    // Positive tests for float
    EXPECT_THAT(row->get<"floaty">(), IsFloatSame(5.0f));
    EXPECT_THAT(row->get("floaty"), IsFloatSame(5.0f));
    EXPECT_THAT(row->get<1>(), IsFloatSame(5.0f));
    EXPECT_THAT(row->get(1), IsFloatSame(5.0f));

    // Negative tests for float
    EXPECT_THAT(row->get<"floaty">(), Not(IsFloatSame(6.0f)));
    EXPECT_THAT(row->get("floaty"), Not(IsFloatSame(6.0f)));
    EXPECT_THAT(row->get<1>(), Not(IsFloatSame(6.0f)));
    EXPECT_THAT(row->get(1), Not(IsFloatSame(6.0f)));
}

TEST(Select, Wildcard)
{
    auto res = g_conn->prepared<"SELECT * FROM __ct_sql_test_rows LIMIT 1;">();
    ASSERT_TRUE(res);
    ASSERT_EQ(res->get_row_count(), 1);

    auto row = res->next();
    ASSERT_TRUE(row);

    EXPECT_THAT(row->get<"firstname">(), IsStrSame("John"));
    EXPECT_THAT(row->get("firstname"), IsStrSame("John"));

    EXPECT_THAT(row->get<"floaty">(), IsFloatSame(5.0f));
    EXPECT_THAT(row->get("floaty"), IsFloatSame(5.0f));
}

TEST(Select, RowCount)
{
    auto res = g_conn->execute<"SELECT * FROM __ct_sql_test_rows LIMIT 5;">();
    ASSERT_TRUE(res);
    EXPECT_EQ(res->get_row_count(), 5);
}

TEST(Update, AffectedRowCount)
{
    auto res_count = g_conn->execute("SELECT COUNT(*) FROM __ct_sql_test_rows WHERE id < 5;");
    ASSERT_TRUE(res_count);
    auto row = res_count->next();
    ASSERT_TRUE(row);

    const uint32_t count = row->get("COUNT(*)");

    auto res_inc = g_conn->execute(std::format("UPDATE __ct_sql_test_rows SET small = small+1 WHERE id < {};", count));
    ASSERT_TRUE(res_inc);
    EXPECT_EQ(res_inc->get_affected_row_count(), count);

    auto res_dec = g_conn->prepared<"UPDATE __ct_sql_test_rows SET small = small-1 WHERE id < ?;">(count);
    ASSERT_TRUE(res_dec);
    EXPECT_EQ(res_dec->get_affected_row_count(), count);
}

TEST(GetOptional, Misc)
{
    auto res = g_conn->prepared<"SELECT maybe_null FROM __ct_sql_test_rows LIMIT 2;">();
    ASSERT_TRUE(res);
    ASSERT_EQ(res->get_row_count(), 2);

    auto row1 = res->next();
    ASSERT_TRUE(row1);

    EXPECT_TRUE((row1->get_opt<"maybe_null", int>().has_value()));
    EXPECT_TRUE((row1->get_opt<int>("maybe_null").has_value()));
    EXPECT_TRUE((row1->get_opt<0, int>().has_value()));
    EXPECT_TRUE((row1->get_opt<int>(0).has_value()));

    auto row2 = res->next();
    ASSERT_TRUE(row2);

    EXPECT_FALSE((row2->get_opt<"maybe_null", int>().has_value()));
    EXPECT_FALSE((row2->get_opt<int>("maybe_null").has_value()));
    EXPECT_FALSE((row2->get_opt<0, int>().has_value()));
    EXPECT_FALSE((row2->get_opt<int>(0).has_value()));
}

TEST(ColumnTypes, StringParamater)
{
    std::string str = "John";
    auto res        = g_conn->prepared<"SELECT 1 FROM __ct_sql_test_rows WHERE firstname = ?;">(str);
    ASSERT_TRUE(res);
    ASSERT_TRUE(res->get_row_count() > 0);

    const char* c_arr = "John";
    res               = g_conn->prepared<"SELECT 1 FROM __ct_sql_test_rows WHERE firstname = ?;">(c_arr);
    ASSERT_TRUE(res);
    ASSERT_TRUE(res->get_row_count() > 0);

    std::string_view str_view = "John";
    res                       = g_conn->prepared<"SELECT 1 FROM __ct_sql_test_rows WHERE firstname = ?;">(str_view);
    ASSERT_TRUE(res);
    ASSERT_TRUE(res->get_row_count() > 0);

    res = g_conn->prepared<"SELECT 1 FROM __ct_sql_test_rows WHERE firstname = ?;">("John");
    ASSERT_TRUE(res);
    ASSERT_TRUE(res->get_row_count() > 0);
}

TEST(ColumnTypes, BlobSelect)
{
    auto res = g_conn->prepared<"SELECT blobby FROM __ct_sql_test_rows LIMIT 1;">();
    ASSERT_TRUE(res);
    ASSERT_EQ(res->get_row_count(), 1);

    auto row = res->next();
    ASSERT_TRUE(row);

    ASSERT_EQ(row->byte_length<"blobby">(), 16);

    {
        const auto byte_elements_are = ElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

        uint8_t c_arr[16];
        row->copy_to<"blobby">(c_arr);
        EXPECT_THAT(c_arr, byte_elements_are);

        std::array<uint8_t, 16> arr;
        row->copy_to<"blobby">(arr);
        EXPECT_THAT(arr, byte_elements_are);

        std::vector<uint8_t> vec;
        row->copy_to<"blobby">(vec);
        EXPECT_THAT(vec, byte_elements_are);
    }

    {
        const auto short_elements_are = ElementsAre(0x0100, 0x0302, 0x0504, 0x0706, 0x0908, 0x0B0A, 0x0D0C, 0x0F0E);

        uint16_t c_arr[8];
        row->copy_to<"blobby">(c_arr);
        EXPECT_THAT(c_arr, short_elements_are);

        std::array<uint16_t, 8> arr;
        row->copy_to<"blobby">(arr);
        EXPECT_THAT(arr, short_elements_are);

        std::vector<uint16_t> vec;
        row->copy_to<"blobby">(vec);
        EXPECT_THAT(vec, short_elements_are);
    }
}

TEST(Prepared, CachingControl)
{
    auto res = g_conn->prepared_uncached<"SELECT * FROM __ct_sql_test_rows LIMIT 5;">();
    ASSERT_TRUE(res);
    EXPECT_EQ(res->get_row_count(), 5);

    auto res2 = g_conn->prepared<"SELECT * FROM __ct_sql_test_rows LIMIT 5;", false>();
    ASSERT_TRUE(res2);
    EXPECT_EQ(res2->get_row_count(), 5);
}

TEST(InvalidQueries, BadQuery)
{
    auto res = g_conn->execute("not a valid query;");
    ASSERT_FALSE(res);
}

TEST(InvalidQueries, BadPreparedQueryRetry)
{
    for (size_t i = 0; i < 3; i++)
    {
        auto res = g_conn->prepared<"SELECT not_a_column FROM __ct_sql_test_rows WHERE firstname = ?;">(1);
        ASSERT_FALSE(res);
    }
}

TEST(InvalidQueries, NonExistentColumn)
{
    auto res = g_conn->execute<"SELECT not_a_column FROM __ct_sql_test_rows;">();
    ASSERT_FALSE(res);
}

TEST(InvalidQueries, MissingArgument)
{
    auto res = g_conn->prepared("SELECT * FROM __ct_sql_test_rows WHERE id > ?");
    ASSERT_FALSE(res);
}

TEST(Reconnect, ConnectionTimeoutPrepared)
{
    auto conn = get_db_connection("__ct_sql_test");

    // Verify it's connected
    ASSERT_TRUE(conn->ping());

    // Set a 1 second timeout for the connection
    ASSERT_TRUE(conn->prepared<"SET SESSION wait_timeout = 1;">());

    auto res = conn->prepared<"SELECT small FROM __ct_sql_test_rows;">();
    ASSERT_TRUE(res);

    // Wait for connection timeout
    std::this_thread::sleep_for(1.5s);

    // Verify it's been disconnected
    ASSERT_FALSE(conn->ping());

    res = conn->prepared<"SELECT small FROM __ct_sql_test_rows;">();
    ASSERT_TRUE(res);
}

TEST(Reconnect, ConnectionTimeoutExecute)
{
    auto conn = get_db_connection("__ct_sql_test");

    // Verify it's connected
    ASSERT_TRUE(conn->ping());

    // Set a 1 second timeout for the connection
    ASSERT_TRUE(conn->execute<"SET SESSION wait_timeout = 1;">());

    auto res = conn->execute<"SELECT small FROM __ct_sql_test_rows;">();
    ASSERT_TRUE(res);

    // Wait for connection timeout
    std::this_thread::sleep_for(1.5s);

    // Verify it's been disconnected
    ASSERT_FALSE(conn->ping());

    res = conn->execute<"SELECT small FROM __ct_sql_test_rows;">();
    ASSERT_TRUE(res);
}

TEST(StringLiteral, Concatenation)
{
    constexpr auto first = StringLiteral("SELECT tiny,");
    constexpr auto full  = first.append(" small FROM __ct_sql_test_rows LIMIT 1;");
    auto res             = g_conn->execute<full>();
    ASSERT_TRUE(res);
    auto row = res->next();
    ASSERT_TRUE(row);

    // Compile-time checks still work for the columns
    uint8_t tiny   = row->get<"tiny">();
    uint16_t small = row->get<"small">();
}

static void setup_database(MySqlConnection* conn)
{
    if (auto res = conn->execute("DROP DATABASE IF EXISTS __ct_sql_test;"); !res)
    {
        throw std::runtime_error(std::format("Error during query: {}", res.error));
    }

    if (auto res = conn->execute("CREATE DATABASE __ct_sql_test;"); !res)
    {
        throw std::runtime_error(std::format("Error during query: {}", res.error));
    }

    if (auto res = conn->use_database("__ct_sql_test"); !res)
    {
        throw std::runtime_error(std::format("Error during query: {}", res.error));
    }

    if (auto res = conn->execute("DROP TABLE IF EXISTS __ct_sql_test_rows;"); !res)
    {
        throw std::runtime_error(std::format("Error during query: {}", res.error));
    }

    // Create the table
    auto res = conn->execute(
        "CREATE TABLE `__ct_sql_test_rows` ("
        "  `id` int(10) NOT NULL,"
        "  `firstname` varchar(25) default null,"
        "  `lastname` varchar(50) default null,"
        "  `tiny` tinyint(3) not null default '0',"
        "  `small` smallint(5) unsigned not null default '0',"
        "  `big` bigint(20) unsigned not null default '0',"
        "  `floaty` float(7,3) not null default '0',"
        "  `doubley` double(8,5) not null default '0',"
        "  `blobby` blob(16) null,"
        "  `maybe_null` int(10) null default null,"
        "  PRIMARY KEY (`id`)"
        ");");

    if (!res)
    {
        throw std::runtime_error(std::format("Error during query: {}", res.error));
    }

    // Populate the table
    for (size_t i = 0; i < 10; i++)
    {
        if (auto res = conn->prepared("INSERT INTO __ct_sql_test_rows VALUES(?, \"John\", \"Doe\", 1, 2, 3, 5.0, 10.0, 0x000102030405060708090A0B0C0D0E0F, NULL);", i); !res)
        {
            throw std::runtime_error(std::format("Error during query: {}", res.error));
        }

        if (i % 2 == 0)
        {
            // Set every other row to have a non-null value in maybe_null
            if (auto res = conn->prepared("UPDATE __ct_sql_test_rows SET maybe_null = ? WHERE id = ?", i, i); !res)
            {
                throw std::runtime_error(std::format("Error during query: {}", res.error));
            }
        }
    }
}

static void teardown_database(MySqlConnection* conn)
{
    if (auto res = conn->execute("DROP DATABASE IF EXISTS __ct_sql_test;"); !res)
    {
        throw std::runtime_error(std::format("Error during query: {}", res.error));
    }
}

int main(int argc, char** argv)
{
    g_conn = get_db_connection();
    setup_database(g_conn.get());

    testing::InitGoogleTest(&argc, argv);
    auto res = RUN_ALL_TESTS();

    teardown_database(g_conn.get());

    return res;
}

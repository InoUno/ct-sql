

#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "ct-sql.h"

using namespace ct_sql;

#define BENCH_ROW_COUNT 500

static std::unique_ptr<MySqlConnection> get_db_connection()
{
    const auto user     = std::getenv("CT_SQL_USER");
    const auto password = std::getenv("CT_SQL_PASSWORD");
    const auto host     = std::getenv("CT_SQL_HOST");
    const auto port     = std::getenv("CT_SQL_PORT");
    const auto db       = std::getenv("CT_SQL_DATABASE");

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

struct BenchRow
{
    uint32_t id;
    std::string firstname;
    std::string lastname;
    uint8_t tiny;
    uint16_t small;
    uint64_t big;
    float floaty;
    double doubley;
};

class UsersFixture : public benchmark::Fixture
{
public:
    std::unique_ptr<MySqlConnection> conn;
    std::vector<BenchRow> rows;

    void SetUp(::benchmark::State& state)
    {
        if (!conn)
        {
            conn = get_db_connection();
            if (auto res = conn->execute("USE __ct_sql_bench;"); !res)
            {
                throw std::runtime_error(std::format("Error during query: {}", res.error));
            }
        }
        rows.reserve(BENCH_ROW_COUNT);
    }

    void TearDown(::benchmark::State& state)
    {
        rows.clear();
    }
};

// The various combinations of queries and column getters

#define PREPARED_TEMPLATE     conn->prepared<"SELECT id, firstname, lastname, tiny, small, big, floaty, doubley FROM __ct_sql_bench_rows">();
#define PREPARED_ARG          conn->prepared("SELECT id, firstname, lastname, tiny, small, big, floaty, doubley FROM __ct_sql_bench_rows");
#define EXECUTE_TEXT_TEMPLATE conn->execute<"SELECT id, firstname, lastname, tiny, small, big, floaty, doubley FROM __ct_sql_bench_rows">();
#define EXECUTE_TEXT_ARG      conn->execute("SELECT id, firstname, lastname, tiny, small, big, floaty, doubley FROM __ct_sql_bench_rows");

#define GETTER_ARG_INDEX          \
    BenchRow                      \
    {                             \
        .id        = row->get(0), \
        .firstname = row->get(1), \
        .lastname  = row->get(2), \
        .tiny      = row->get(3), \
        .small     = row->get(4), \
        .big       = row->get(5), \
        .floaty    = row->get(6), \
        .doubley   = row->get(7), \
    }

#define GETTER_TEMPLATE_INDEX       \
    BenchRow                        \
    {                               \
        .id        = row->get<0>(), \
        .firstname = row->get<1>(), \
        .lastname  = row->get<2>(), \
        .tiny      = row->get<3>(), \
        .small     = row->get<4>(), \
        .big       = row->get<5>(), \
        .floaty    = row->get<6>(), \
        .doubley   = row->get<7>(), \
    }

#define GETTER_TEMPLATE_COLNAME               \
    BenchRow                                  \
    {                                         \
        .id        = row->get<"id">(),        \
        .firstname = row->get<"firstname">(), \
        .lastname  = row->get<"lastname">(),  \
        .tiny      = row->get<"tiny">(),      \
        .small     = row->get<"small">(),     \
        .big       = row->get<"big">(),       \
        .floaty    = row->get<"floaty">(),    \
        .doubley   = row->get<"doubley">(),   \
    }

#define GETTER_ARG_COLNAME                  \
    BenchRow                                \
    {                                       \
        .id        = row->get("id"),        \
        .firstname = row->get("firstname"), \
        .lastname  = row->get("lastname"),  \
        .tiny      = row->get("tiny"),      \
        .small     = row->get("small"),     \
        .big       = row->get("big"),       \
        .floaty    = row->get("floaty"),    \
        .doubley   = row->get("doubley"),   \
    }

#define SETUP_CT_BENCHMARK(name, query_kind, getter_kind) \
    BENCHMARK_F(UsersFixture, name)                       \
    (benchmark::State & state)                            \
    {                                                     \
        auto conn  = this->conn.get();                    \
        auto& rows = this->rows;                          \
        for (auto _ : state)                              \
        {                                                 \
            auto res = query_kind;                        \
            while (auto row = res->next())                \
            {                                             \
                rows.emplace_back(getter_kind);           \
            }                                             \
        }                                                 \
    };

// Benchmark each permuatation of query and column getters
SETUP_CT_BENCHMARK(PreparedTemplate_IndexTemplate, PREPARED_TEMPLATE, GETTER_TEMPLATE_INDEX);
SETUP_CT_BENCHMARK(PreparedTemplate_ColnameTemplate, PREPARED_TEMPLATE, GETTER_TEMPLATE_COLNAME);
SETUP_CT_BENCHMARK(PreparedTemplate_IndexArg, PREPARED_TEMPLATE, GETTER_ARG_INDEX);
SETUP_CT_BENCHMARK(PreparedTemplate_ColnameArg, PREPARED_TEMPLATE, GETTER_ARG_COLNAME);

SETUP_CT_BENCHMARK(PreparedArg_IndexTemplate, PREPARED_ARG, GETTER_TEMPLATE_INDEX);
SETUP_CT_BENCHMARK(PreparedArg_ColnameTemplate, PREPARED_ARG, GETTER_TEMPLATE_COLNAME);
SETUP_CT_BENCHMARK(PreparedArg_IndexArg, PREPARED_ARG, GETTER_ARG_INDEX);
SETUP_CT_BENCHMARK(PreparedArg_ColnameArg, PREPARED_ARG, GETTER_ARG_COLNAME);

SETUP_CT_BENCHMARK(ExecuteTextTemplate_IndexTemplate, EXECUTE_TEXT_TEMPLATE, GETTER_TEMPLATE_INDEX);
SETUP_CT_BENCHMARK(ExecuteTextTemplate_ColnameTemplate, EXECUTE_TEXT_TEMPLATE, GETTER_TEMPLATE_COLNAME);
SETUP_CT_BENCHMARK(ExecuteTextTemplate_IndexArg, EXECUTE_TEXT_TEMPLATE, GETTER_ARG_INDEX);
SETUP_CT_BENCHMARK(ExecuteTextTemplate_ColnameArg, EXECUTE_TEXT_TEMPLATE, GETTER_ARG_COLNAME);

SETUP_CT_BENCHMARK(ExecuteTextArg_IndexTemplate, EXECUTE_TEXT_ARG, GETTER_TEMPLATE_INDEX);
SETUP_CT_BENCHMARK(ExecuteTextArg_ColnameTemplate, EXECUTE_TEXT_ARG, GETTER_TEMPLATE_COLNAME);
SETUP_CT_BENCHMARK(ExecuteTextArg_IndexArg, EXECUTE_TEXT_ARG, GETTER_ARG_INDEX);
SETUP_CT_BENCHMARK(ExecuteTextArg_ColnameArg, EXECUTE_TEXT_ARG, GETTER_ARG_COLNAME);

static void setup_database(MySqlConnection* conn)
{
    if (auto res = conn->execute("DROP DATABASE IF EXISTS __ct_sql_bench;"); !res)
    {
        throw std::runtime_error(std::format("Error during query: {}", res.error));
    }

    if (auto res = conn->execute("CREATE DATABASE __ct_sql_bench;"); !res)
    {
        throw std::runtime_error(std::format("Error during query: {}", res.error));
    }

    if (auto res = conn->execute("USE __ct_sql_bench;"); !res)
    {
        throw std::runtime_error(std::format("Error during query: {}", res.error));
    }

    if (auto res = conn->execute("DROP TABLE IF EXISTS `__ct_sql_bench_rows`;"); !res)
    {
        throw std::runtime_error(std::format("Error during query: {}", res.error));
    }

    // Create the table
    auto res = conn->execute(
        "CREATE TABLE `__ct_sql_bench_rows` ("
        "  `id` int(10) NOT NULL,"
        "  `firstname` varchar(25) default null,"
        "  `lastname` varchar(50) default null,"
        "  `tiny` tinyint(3) not null default '0',"
        "  `small` smallint(5) unsigned not null default '0',"
        "  `big` bigint(20) unsigned not null default '0',"
        "  `floaty` float(7,3) not null default '0',"
        "  `doubley` double(8,5) not null default '0',"
        "  PRIMARY KEY (`id`)"
        ") ENGINE=MyISAM DEFAULT CHARSET=utf8;");

    if (!res)
    {
        throw std::runtime_error(std::format("Error during query: {}", res.error));
    }

    // Populate the table
    for (size_t i = 0; i < BENCH_ROW_COUNT; i++)
    {
        if (auto res = conn->prepared<"INSERT INTO __ct_sql_bench_rows VALUES(?, \"John\", \"Doe\", 1, 2, 3, 5.0, 10.0);">(i); !res)
        {
            throw std::runtime_error(std::format("Error during query: {}", res.error));
        }
    }
}

static void teardown_database(MySqlConnection* conn)
{
    if (auto res = conn->execute("DROP DATABASE IF EXISTS __ct_sql_bench;"); !res)
    {
        std::cerr << std::format("Error during query: {}", res.error) << std::endl;
    }
}

int main(int argc, char** argv)
{
    benchmark::MaybeReenterWithoutASLR(argc, argv);
    char arg0_default[] = "benchmark";
    char* args_default  = reinterpret_cast<char*>(arg0_default);
    if (!argv)
    {
        argc = 1;
        argv = &args_default;
    }
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv))
    {
        return 1;
    }

    auto conn = get_db_connection();
    setup_database(conn.get());

    ::benchmark::RunSpecifiedBenchmarks();

    teardown_database(conn.get());

    ::benchmark::Shutdown();
    return 0;
}


#include "database/DatabaseConnectionString.h"
#include "lib/catch.hpp"
#include <soci.h>

using namespace viichain;

TEST_CASE("remove password from database connection string",
          "[db][dbconnectionstring]")
{
    SECTION("empty connection string remains empty")
    {
        REQUIRE(removePasswordFromConnectionString("") == "");
    }

    SECTION("password is removed if first")
    {
        REQUIRE(removePasswordFromConnectionString(
                    R"(postgresql://password=abc dbname=viichain)") ==
                R"(postgresql://password=******** dbname=viichain)");
    }

    SECTION("password is removed if second")
    {
        REQUIRE(removePasswordFromConnectionString(
                    R"(postgresql://dbname=viichain password=dbname)") ==
                R"(postgresql://dbname=viichain password=********)");
    }

    SECTION("database can be named password")
    {
        REQUIRE(removePasswordFromConnectionString(
                    R"(postgresql://dbname=password password=dbname)") ==
                R"(postgresql://dbname=password password=********)");
    }

    SECTION("quoted password is removed")
    {
        REQUIRE(
            removePasswordFromConnectionString(
                R"(postgresql://dbname=viichain password='quoted password')") ==
            R"(postgresql://dbname=viichain password=********)");
    }

    SECTION("quoted password with quote is removed")
    {
        REQUIRE(
            removePasswordFromConnectionString(
                R"(postgresql://dbname=viichain password='quoted \' password')") ==
            R"(postgresql://dbname=viichain password=********)");
    }

    SECTION("quoted password with backslash is removed")
    {
        REQUIRE(
            removePasswordFromConnectionString(
                R"(postgresql://dbname=viichain password='quoted \\ password')") ==
            R"(postgresql://dbname=viichain password=********)");
    }

    SECTION("quoted password with backslash and quote is removed")
    {
        REQUIRE(
            removePasswordFromConnectionString(
                R"(postgresql://dbname=viichain password='quoted \\ password')") ==
            R"(postgresql://dbname=viichain password=********)");
    }

    SECTION("parameters after password remain unchanged")
    {
        REQUIRE(
            removePasswordFromConnectionString(
                R"(postgresql://dbname=viichain password='quoted \\ password' performance='as fast as possible')") ==
            R"(postgresql://dbname=viichain password=******** performance='as fast as possible')");
    }

    SECTION("dbname can be quored")
    {
        REQUIRE(
            removePasswordFromConnectionString(
                R"(postgresql://dbname='viichain with spaces' password='quoted \\ password' performance='as fast as possible')") ==
            R"(postgresql://dbname='viichain with spaces' password=******** performance='as fast as possible')");
    }

    SECTION("spaces before equals are accepted")
    {
        REQUIRE(
            removePasswordFromConnectionString(
                R"(postgresql://dbname ='vii with spaces' password ='quoted \\ password' performance ='as fast as possible')") ==
            R"(postgresql://dbname ='vii with spaces' password =******** performance ='as fast as possible')");
    }

    SECTION("spaces after equals are accepted")
    {
        REQUIRE(
            removePasswordFromConnectionString(
                R"(postgresql://dbname= 'vii with spaces' password= 'quoted \\ password' performance= 'as fast as possible')") ==
            R"(postgresql://dbname= 'vii with spaces' password= ******** performance= 'as fast as possible')");
    }

    SECTION("spaces around equals are accepted")
    {
        REQUIRE(
            removePasswordFromConnectionString(
                R"(postgresql://dbname = 'vii with spaces' password = 'quoted \\ password' performance = 'as fast as possible')") ==
            R"(postgresql://dbname = 'vii with spaces' password = ******** performance = 'as fast as possible')");
    }

    SECTION(
        "invalid connection string without equals and value remains as it was")
    {
        REQUIRE(removePasswordFromConnectionString(
                    R"(postgresql://dbname password=asbc)") ==
                R"(postgresql://dbname password=asbc)");
    }

    SECTION("invalid connection string without value remains as it was")
    {
        REQUIRE(removePasswordFromConnectionString(
                    R"(postgresql://dbname= password=asbc)") ==
                R"(postgresql://dbname= password=asbc)");
    }

    SECTION("invalid connection string with unfinished quoted value")
    {
        REQUIRE(removePasswordFromConnectionString(
                    R"(postgresql://dbname='quoted value)") ==
                R"(postgresql://dbname='quoted value)");
    }

    SECTION("invalid connection string with quoted value with unfinished "
            "escape sequence")
    {
        REQUIRE(removePasswordFromConnectionString(
                    R"(postgresql://dbname='quoted value\ password=abc)") ==
                R"(postgresql://dbname='quoted value\ password=abc)");
    }

    SECTION("invalid connection string without backend name")
    {
        REQUIRE(removePasswordFromConnectionString(
                    R"(dbname=name password=abc)") ==
                R"(dbname=name password=abc)");
    }

    SECTION("ignore sqlite3://:memory:")
    {
        REQUIRE(removePasswordFromConnectionString(
                    R"(sqlite3://:memory:)") ==
                R"(sqlite3://:memory:)");
    }
}

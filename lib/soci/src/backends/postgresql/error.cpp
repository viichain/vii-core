
#define SOCI_POSTGRESQL_SOURCE
#include "soci-postgresql.h"
#include "error.h"
#include <cstring>
#include <cassert>

using namespace soci;
using namespace soci::details;

postgresql_soci_error::postgresql_soci_error(
    std::string const & msg, char const *sqlst)
    : soci_error(msg)
{
    assert(std::strlen(sqlst) == 5);
    std::memcpy(sqlstate_, sqlst, 5);
}

std::string postgresql_soci_error::sqlstate() const
{
    return std::string(sqlstate_, 5);
}

void
details::postgresql_result::check_for_errors(char const* errMsg) const
{
    static_cast<void>(check_for_data(errMsg));
}

bool
details::postgresql_result::check_for_data(char const* errMsg) const
{
    ExecStatusType const status = PQresultStatus(result_);
    switch (status)
    {
        case PGRES_EMPTY_QUERY:
        case PGRES_COMMAND_OK:
                        return false;

        case PGRES_TUPLES_OK:
            return true;

        default:
                                                break;
    }

    std::string msg(errMsg);
    const char* const pqError = PQresultErrorMessage(result_);
    if (pqError && *pqError)
    {
      msg += " ";
      msg += pqError;
    }

    const char* sqlstate = PQresultErrorField(result_, PG_DIAG_SQLSTATE);
    const char* const blank_sql_state = "     ";
    if (!sqlstate)
    {
        sqlstate = blank_sql_state;
    }

    throw postgresql_soci_error(msg, sqlstate);
}

#pragma once


#include <lib/soci/src/backends/sqlite3/soci-sqlite3.h>
#ifdef USE_POSTGRES
#include <lib/soci/src/backends/postgresql/soci-postgresql.h>
#endif

namespace viichain
{
template <typename T = void> class DatabaseTypeSpecificOperation
{
  public:
    virtual T doSqliteSpecificOperation(soci::sqlite3_session_backend* sq) = 0;
#ifdef USE_POSTGRES
    virtual T
    doPostgresSpecificOperation(soci::postgresql_session_backend* pg) = 0;
#endif
};
}


#define SOCI_SQLITE3_SOURCE
#include "soci-sqlite3.h"
#include <backend-loader.h>

#ifdef _MSC_VER
#pragma warning(disable:4355)
#endif

using namespace soci;
using namespace soci::details;

sqlite3_session_backend * sqlite3_backend_factory::make_session(
     connection_parameters const & parameters) const
{
     return new sqlite3_session_backend(parameters);
}

sqlite3_backend_factory const soci::sqlite3;

extern "C"
{

SOCI_SQLITE3_DECL backend_factory const * factory_sqlite3()
{
    return &soci::sqlite3;
}

SOCI_SQLITE3_DECL void register_factory_sqlite3()
{
    soci::dynamic_backends::register_backend("sqlite3", soci::sqlite3);
}

} // extern "C"


#define SOCI_POSTGRESQL_SOURCE
#include "soci-postgresql.h"
#include <backend-loader.h>
#include <libpq/libpq-fs.h> // libpq

#ifdef SOCI_POSTGRESQL_NOPARAMS
#ifndef SOCI_POSTGRESQL_NOBINDBYNAME
#define SOCI_POSTGRESQL_NOBINDBYNAME
#endif // SOCI_POSTGRESQL_NOBINDBYNAME
#endif // SOCI_POSTGRESQL_NOPARAMS

#ifdef _MSC_VER
#pragma warning(disable:4355)
#endif

using namespace soci;
using namespace soci::details;

postgresql_session_backend * postgresql_backend_factory::make_session(
     connection_parameters const & parameters) const
{
     return new postgresql_session_backend(parameters);
}

postgresql_backend_factory const soci::postgresql;

extern "C"
{

SOCI_POSTGRESQL_DECL backend_factory const * factory_postgresql()
{
    return &soci::postgresql;
}

SOCI_POSTGRESQL_DECL void register_factory_postgresql()
{
    soci::dynamic_backends::register_backend("postgresql", soci::postgresql);
}

} // extern "C"

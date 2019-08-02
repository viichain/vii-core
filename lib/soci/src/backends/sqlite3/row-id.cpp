
#include "soci-sqlite3.h"

#ifdef _MSC_VER
#pragma warning(disable:4355)
#endif

using namespace soci;
using namespace soci::details;

sqlite3_rowid_backend::sqlite3_rowid_backend(
    sqlite3_session_backend & /* session */)
{
    }

sqlite3_rowid_backend::~sqlite3_rowid_backend()
{
    }


#define SOCI_SOURCE
#include "rowid.h"
#include "session.h"

using namespace soci;
using namespace soci::details;

rowid::rowid(session & s)
{
    backEnd_ = s.make_rowid_backend();
}

rowid::~rowid()
{
    delete backEnd_;
}

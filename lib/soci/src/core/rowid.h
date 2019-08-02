
#ifndef SOCI_ROWID_H_INCLUDED
#define SOCI_ROWID_H_INCLUDED

#include "soci-config.h"

namespace soci
{

class session;

namespace details
{

class rowid_backend;

} // namespace details


class SOCI_DECL rowid
{
public:
    explicit rowid(session & s);
    ~rowid();

    details::rowid_backend * get_backend() { return backEnd_; }

private:
    details::rowid_backend *backEnd_;
};

} // namespace soci

#endif

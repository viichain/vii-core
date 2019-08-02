
#ifndef SOCI_TRANSACTION_H_INCLUDED
#define SOCI_TRANSACTION_H_INCLUDED

#include "session.h"
#include "soci-config.h"

namespace soci
{

class SOCI_DECL transaction
{
public:
    explicit transaction(session& sql);

    ~transaction();

    void commit();
    void rollback();

private:
    bool handled_;
    session& sql_;

        transaction(transaction const& other);
    transaction& operator=(transaction const& other);
};

} // namespace soci

#endif // SOCI_TRANSACTION_H_INCLUDED

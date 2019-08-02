
#define SOCI_SOURCE
#include "transaction.h"
#include "error.h"

using namespace soci;

transaction::transaction(session& sql)
    : handled_(false), sql_(sql)
{
    sql_.begin();
}

transaction::~transaction()
{
    if (handled_ == false)
    {
        try
        {
            rollback();
        }
        catch (...)
        {}
    }
}

void transaction::commit()
{
    if (handled_)
    {
        throw soci_error("The transaction object cannot be handled twice.");
    }

    sql_.commit();
    handled_ = true;
}

void transaction::rollback()
{
    if (handled_)
    {
        throw soci_error("The transaction object cannot be handled twice.");
    }

    sql_.rollback();
    handled_ = true;
}

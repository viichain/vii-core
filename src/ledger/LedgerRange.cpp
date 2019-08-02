
#include "ledger/LedgerRange.h"

#include <cassert>
#include <util/format.h>

namespace viichain
{

LedgerRange::LedgerRange(uint32_t first, uint32_t last)
    : mFirst{first}, mLast{last}
{
    assert(mFirst > 0);
    assert(mLast >= mFirst);
}

std::string
LedgerRange::toString() const
{
    return fmt::format("{}..{}", mFirst, mLast);
}

bool
operator==(LedgerRange const& x, LedgerRange const& y)
{
    if (x.mFirst != y.mFirst)
    {
        return false;
    }
    if (x.mLast != y.mLast)
    {
        return false;
    }
    return true;
}

bool
operator!=(LedgerRange const& x, LedgerRange const& y)
{
    return !(x == y);
}
}

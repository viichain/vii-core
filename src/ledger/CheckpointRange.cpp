
#include "ledger/CheckpointRange.h"
#include "history/HistoryManager.h"
#include "ledger/LedgerRange.h"

#include <cassert>
#include <util/format.h>

namespace viichain
{

CheckpointRange::CheckpointRange(uint32_t first, uint32_t last,
                                 uint32_t frequency)
    : mFirst{first}, mLast{last}, mFrequency{frequency}
{
    assert(mFirst > 0);
    assert(mLast >= mFirst);
    assert((mFirst + 1) % mFrequency == 0);
    assert((mLast + 1) % mFrequency == 0);
}

CheckpointRange::CheckpointRange(LedgerRange const& ledgerRange,
                                 HistoryManager const& historyManager)
    : mFirst{historyManager.checkpointContainingLedger(ledgerRange.mFirst)}
    , mLast{historyManager.checkpointContainingLedger(ledgerRange.mLast)}
    , mFrequency{historyManager.getCheckpointFrequency()}
{
    assert(mFirst > 0);
    assert(mLast >= mFirst);
    assert((mFirst + 1) % mFrequency == 0);
    assert((mLast + 1) % mFrequency == 0);
}

std::string
CheckpointRange::toString() const
{
    return fmt::format("{}..{}", mFirst + 1 - mFrequency, mLast);
}

bool
operator==(CheckpointRange const& x, CheckpointRange const& y)
{
    if (x.mFirst != y.mFirst)
    {
        return false;
    }
    if (x.mLast != y.mLast)
    {
        return false;
    }
    if (x.mFrequency != y.mFrequency)
    {
        return false;
    }
    return true;
}

bool
operator!=(CheckpointRange const& x, CheckpointRange const& y)
{
    return !(x == y);
}
}

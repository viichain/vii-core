
#include "ledger/SyncingLedgerChain.h"
#include "herder/LedgerCloseData.h"

namespace viichain
{

SyncingLedgerChain::SyncingLedgerChain() = default;
SyncingLedgerChain::SyncingLedgerChain(SyncingLedgerChain const&) = default;
SyncingLedgerChain::~SyncingLedgerChain() = default;

LedgerCloseData const&
SyncingLedgerChain::front() const
{
    return mChain.front();
}

LedgerCloseData const&
SyncingLedgerChain::back() const
{
    return mChain.back();
}

void
SyncingLedgerChain::pop()
{
    mChain.pop();
}

SyncingLedgerChainAddResult
SyncingLedgerChain::push(LedgerCloseData const& lcd)
{
    if (mChain.empty() ||
        mChain.back().getLedgerSeq() + 1 == lcd.getLedgerSeq())
    {
        mChain.emplace(lcd);
        return SyncingLedgerChainAddResult::CONTIGUOUS;
    }

    if (lcd.getLedgerSeq() <= mChain.back().getLedgerSeq())
    {
        return SyncingLedgerChainAddResult::TOO_OLD;
    }

    return SyncingLedgerChainAddResult::TOO_NEW;
}

size_t
SyncingLedgerChain::size() const
{
    return mChain.size();
}

bool
SyncingLedgerChain::empty() const
{
    return mChain.empty();
}
}

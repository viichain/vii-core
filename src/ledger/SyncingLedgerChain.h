#pragma once


#include <cstddef>
#include <list>
#include <queue>

namespace viichain
{

class LedgerCloseData;

enum class SyncingLedgerChainAddResult
{
    CONTIGUOUS,
    TOO_OLD,
    TOO_NEW
};

class SyncingLedgerChain final
{
  public:
    SyncingLedgerChain();
    SyncingLedgerChain(SyncingLedgerChain const&);
    ~SyncingLedgerChain();

    LedgerCloseData const& front() const;
    LedgerCloseData const& back() const;
    void pop();
    SyncingLedgerChainAddResult push(LedgerCloseData const& lcd);

    size_t size() const;
    bool empty() const;

  private:
    std::queue<LedgerCloseData, std::list<LedgerCloseData>> mChain;
};
}

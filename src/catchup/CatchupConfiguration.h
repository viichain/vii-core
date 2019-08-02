#pragma once


#include "ledger/LedgerRange.h"
#include <cstdint>
#include <string>

namespace viichain
{

class CatchupConfiguration
{
  public:
    enum class Mode
    {
        OFFLINE,
        ONLINE
    };
    static const uint32_t CURRENT = 0;

    CatchupConfiguration(uint32_t toLedger, uint32_t count, Mode mode);
    CatchupConfiguration(LedgerNumHashPair ledgerHashPair, uint32_t count,
                         Mode mode);

        CatchupConfiguration resolve(uint32_t remoteCheckpoint) const;

    uint32_t
    toLedger() const
    {
        return mLedgerHashPair.first;
    }

    uint32_t
    count() const
    {
        return mCount;
    }

    optional<Hash>
    hash() const
    {
        return mLedgerHashPair.second;
    }

    Mode
    mode() const
    {
        return mMode;
    }

  private:
    uint32_t mCount;
    LedgerNumHashPair mLedgerHashPair;
    Mode mMode;
};

uint32_t parseLedger(std::string const& str);
uint32_t parseLedgerCount(std::string const& str);
}

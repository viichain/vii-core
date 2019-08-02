#pragma once


#include <cstdint>
#include <string>

namespace viichain
{

class HistoryManager;
struct LedgerRange;

struct CheckpointRange final
{
    uint32_t const mFirst;
    uint32_t const mLast;
    uint32_t const mFrequency;

    CheckpointRange(uint32_t first, uint32_t last, uint32_t frequency);
    CheckpointRange(LedgerRange const& ledgerRange,
                    HistoryManager const& historyManager);
    friend bool operator==(CheckpointRange const& x, CheckpointRange const& y);
    friend bool operator!=(CheckpointRange const& x, CheckpointRange const& y);

    uint32_t
    count() const
    {
        return (mLast - mFirst) / mFrequency + 1;
    }

    std::string toString() const;
};
}

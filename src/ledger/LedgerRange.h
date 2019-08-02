#pragma once


#include "util/optional.h"
#include "xdr/vii-types.h"
#include <cstdint>

namespace viichain
{
using LedgerNumHashPair = std::pair<uint32_t, optional<Hash>>;

struct LedgerRange final
{
  public:
    uint32_t const mFirst;
    uint32_t const mLast;

    LedgerRange(uint32_t first, uint32_t last);
    std::string toString() const;

    friend bool operator==(LedgerRange const& x, LedgerRange const& y);
    friend bool operator!=(LedgerRange const& x, LedgerRange const& y);
};
}

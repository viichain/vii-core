
#include "ByteSliceHasher.h"
#include <sodium.h>

namespace viichain
{
namespace shortHash
{
static unsigned char sKey[crypto_shorthash_KEYBYTES];
void
initialize()
{
    crypto_shorthash_keygen(sKey);
}
uint64_t
computeHash(viichain::ByteSlice const& b)
{
    uint64_t res;
    static_assert(sizeof(res) == crypto_shorthash_BYTES, "unexpected size");
    crypto_shorthash(reinterpret_cast<unsigned char*>(&res),
                     reinterpret_cast<const unsigned char*>(b.data()), b.size(),
                     sKey);
    return res;
}
}
}
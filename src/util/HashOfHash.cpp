#include "HashOfHash.h"
#include "crypto/ByteSliceHasher.h"

namespace std
{

size_t
hash<viichain::uint256>::operator()(viichain::uint256 const& x) const noexcept
{
    size_t res =
        viichain::shortHash::computeHash(viichain::ByteSlice(x.data(), 8));

    return res;
}
}

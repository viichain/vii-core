#pragma once
#include <xdr/vii-types.h>

namespace std
{
template <> struct hash<viichain::uint256>
{
    size_t operator()(viichain::uint256 const& x) const noexcept;
};
}


#include "overlay/PeerSharedKeyId.h"

namespace viichain
{

bool
operator==(PeerSharedKeyId const& x, PeerSharedKeyId const& y)
{
    return (x.mECDHPublicKey == y.mECDHPublicKey) && (x.mRole == y.mRole);
}

bool
operator!=(PeerSharedKeyId const& x, PeerSharedKeyId const& y)
{
    return !(x == y);
}
}

namespace std
{

size_t
hash<viichain::PeerSharedKeyId>::
operator()(viichain::PeerSharedKeyId const& x) const noexcept
{
    return std::hash<viichain::Curve25519Public>{}(x.mECDHPublicKey) ^
           std::hash<int>{}(static_cast<int>(x.mRole));
}
}

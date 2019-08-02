#pragma once


#include "crypto/ECDH.h"
#include "overlay/Peer.h"

namespace viichain
{
struct PeerSharedKeyId
{
    Curve25519Public mECDHPublicKey;
    Peer::PeerRole mRole;

    friend bool operator==(PeerSharedKeyId const& x, PeerSharedKeyId const& y);
    friend bool operator!=(PeerSharedKeyId const& x, PeerSharedKeyId const& y);
};
}

namespace std
{
template <> struct hash<viichain::PeerSharedKeyId>
{
    size_t operator()(viichain::PeerSharedKeyId const& x) const noexcept;
};
}

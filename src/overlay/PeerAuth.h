#pragma once

#include "crypto/ECDH.h"
#include "overlay/Peer.h"
#include "overlay/PeerSharedKeyId.h"
#include "util/lrucache.hpp"
#include "xdr/vii-types.h"


namespace viichain
{
class PeerAuth
{
                                        
    Application& mApp;
    Curve25519Secret mECDHSecretKey;
    Curve25519Public mECDHPublicKey;
    AuthCert mCert;

    cache::lru_cache<PeerSharedKeyId, HmacSha256Key> mSharedKeyCache;

    HmacSha256Key getSharedKey(Curve25519Public const& remotePublic,
                               Peer::PeerRole role);

  public:
    PeerAuth(Application& app);

    AuthCert getAuthCert();
    bool verifyRemoteAuthCert(NodeID const& remoteNode, AuthCert const& cert);

    HmacSha256Key getSendingMacKey(Curve25519Public const& remotePublic,
                                   uint256 const& localNonce,
                                   uint256 const& remoteNonce,
                                   Peer::PeerRole role);
    HmacSha256Key getReceivingMacKey(Curve25519Public const& remotePublic,
                                     uint256 const& localNonce,
                                     uint256 const& remoteNonce,
                                     Peer::PeerRole role);
};
}

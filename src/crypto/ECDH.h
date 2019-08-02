#pragma once


#include "xdr/vii-types.h"
#include <functional>

namespace viichain
{


Curve25519Secret EcdhRandomSecret();

Curve25519Public EcdhDerivePublic(Curve25519Secret const& sec);


HmacSha256Key EcdhDeriveSharedKey(Curve25519Secret const& localSecret,
                                  Curve25519Public const& localPublic,
                                  Curve25519Public const& remotePublic,
                                  bool localFirst);
}

namespace std
{
template <> struct hash<viichain::Curve25519Public>
{
    size_t operator()(viichain::Curve25519Public const& x) const noexcept;
};
}

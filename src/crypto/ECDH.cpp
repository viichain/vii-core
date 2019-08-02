
#include "crypto/ECDH.h"
#include "crypto/SHA.h"
#include "util/HashOfHash.h"
#include <functional>
#include <sodium.h>

#ifdef MSAN_ENABLED
#include <sanitizer/msan_interface.h>
#endif

namespace viichain
{

Curve25519Secret
EcdhRandomSecret()
{
    Curve25519Secret out;
    randombytes_buf(out.key.data(), out.key.size());
#ifdef MSAN_ENABLED
    __msan_unpoison(out.key.data(), out.key.size());
#endif
    return out;
}

Curve25519Public
EcdhDerivePublic(Curve25519Secret const& sec)
{
    Curve25519Public out;
    if (crypto_scalarmult_base(out.key.data(), sec.key.data()) != 0)
    {
        throw std::runtime_error("Could not derive key (mult_base)");
    }
    return out;
}

HmacSha256Key
EcdhDeriveSharedKey(Curve25519Secret const& localSecret,
                    Curve25519Public const& localPublic,
                    Curve25519Public const& remotePublic, bool localFirst)
{
    auto const& publicA = localFirst ? localPublic : remotePublic;
    auto const& publicB = localFirst ? remotePublic : localPublic;

    unsigned char q[crypto_scalarmult_BYTES];
    if (crypto_scalarmult(q, localSecret.key.data(), remotePublic.key.data()) !=
        0)
    {
        throw std::runtime_error("Could not derive shared key (mult)");
    }
#ifdef MSAN_ENABLED
    __msan_unpoison(q, crypto_scalarmult_BYTES);
#endif
    std::vector<uint8_t> buf;
    buf.reserve(crypto_scalarmult_BYTES + publicA.key.size() +
                publicB.key.size());
    buf.insert(buf.end(), q, q + crypto_scalarmult_BYTES);
    buf.insert(buf.end(), publicA.key.begin(), publicA.key.end());
    buf.insert(buf.end(), publicB.key.begin(), publicB.key.end());
    return hkdfExtract(buf);
}
}

namespace std
{
size_t
hash<viichain::Curve25519Public>::
operator()(viichain::Curve25519Public const& k) const noexcept
{
    return std::hash<viichain::uint256>()(k.key);
}
}

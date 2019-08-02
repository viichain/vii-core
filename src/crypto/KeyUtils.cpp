
#include "KeyUtils.h"

#include "crypto/StrKey.h"

namespace viichain
{

size_t
KeyUtils::getKeyVersionSize(strKey::StrKeyVersionByte keyVersion)
{
    switch (keyVersion)
    {
    case strKey::STRKEY_PUBKEY_ED25519:
    case strKey::STRKEY_SEED_ED25519:
        return crypto_sign_PUBLICKEYBYTES;
    case strKey::STRKEY_PRE_AUTH_TX:
    case strKey::STRKEY_HASH_X:
        return 32U;
    default:
        throw std::invalid_argument("invalid key version: " +
                                    std::to_string(keyVersion));
    }
}
}

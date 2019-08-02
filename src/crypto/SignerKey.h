#pragma once


#include "crypto/KeyUtils.h"

namespace viichain
{

struct SignerKey;

template <> struct KeyFunctions<SignerKey>
{
    struct getKeyTypeEnum
    {
        using type = SignerKeyType;
    };

    static std::string getKeyTypeName();
    static bool getKeyVersionIsSupported(strKey::StrKeyVersionByte keyVersion);
    static SignerKeyType toKeyType(strKey::StrKeyVersionByte keyVersion);
    static strKey::StrKeyVersionByte toKeyVersion(SignerKeyType keyType);
    static uint256& getKeyValue(SignerKey& key);
    static uint256 const& getKeyValue(SignerKey const& key);
};
}

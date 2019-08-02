#pragma once


#include "crypto/KeyUtils.h"
#include "util/XDROperators.h"
#include "xdr/vii-types.h"

#include <array>
#include <functional>
#include <ostream>

namespace viichain
{

class ByteSlice;
struct SecretValue;
struct SignerKey;

class SecretKey
{
    using uint512 = xdr::opaque_array<64>;
    PublicKeyType mKeyType;
    uint512 mSecretKey;
    PublicKey mPublicKey;

    struct Seed
    {
        PublicKeyType mKeyType;
        uint256 mSeed;
        ~Seed();
    };

        Seed getSeed() const;

  public:
    SecretKey();
    ~SecretKey();

        PublicKey const& getPublicKey() const;

        SecretValue getStrKeySeed() const;

        std::string getStrKeyPublic() const;

        bool isZero() const;

        Signature sign(ByteSlice const& bin) const;

        static SecretKey random();

#ifdef BUILD_TESTS
                    static SecretKey pseudoRandomForTesting();

                static SecretKey pseudoRandomForTestingFromSeed(unsigned int seed);
#endif

        static SecretKey fromStrKeySeed(std::string const& strKeySeed);
    static SecretKey
    fromStrKeySeed(std::string&& strKeySeed)
    {
        SecretKey ret = fromStrKeySeed(strKeySeed);
        for (std::size_t i = 0; i < strKeySeed.size(); ++i)
            strKeySeed[i] = 0;
        return ret;
    }

        static SecretKey fromSeed(ByteSlice const& seed);

    bool
    operator==(SecretKey const& rh) const
    {
        return (mKeyType == rh.mKeyType) && (mSecretKey == rh.mSecretKey);
    }
};

template <> struct KeyFunctions<PublicKey>
{
    struct getKeyTypeEnum
    {
        using type = PublicKeyType;
    };

    static std::string getKeyTypeName();
    static bool getKeyVersionIsSupported(strKey::StrKeyVersionByte keyVersion);
    static PublicKeyType toKeyType(strKey::StrKeyVersionByte keyVersion);
    static strKey::StrKeyVersionByte toKeyVersion(PublicKeyType keyType);
    static uint256& getKeyValue(PublicKey& key);
    static uint256 const& getKeyValue(PublicKey const& key);
};

namespace PubKeyUtils
{
bool verifySig(PublicKey const& key, Signature const& signature,
               ByteSlice const& bin);

void clearVerifySigCache();
void flushVerifySigCacheCounts(uint64_t& hits, uint64_t& misses);

PublicKey random();
}

namespace StrKeyUtils
{
void logKey(std::ostream& s, std::string const& key);
}

namespace HashUtils
{
Hash random();
}
}

namespace std
{
template <> struct hash<viichain::PublicKey>
{
    size_t operator()(viichain::PublicKey const& x) const noexcept;
};
}

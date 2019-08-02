#pragma once


#include "crypto/ByteSliceHasher.h"
#include "xdr/vii-ledger.h"
#include <functional>

namespace std
{
template <> class hash<viichain::LedgerKey>
{
  public:
    size_t
    operator()(viichain::LedgerKey const& lk) const
    {
        size_t res;
        switch (lk.type())
        {
        case viichain::ACCOUNT:
            res = viichain::shortHash::computeHash(
                viichain::ByteSlice(lk.account().accountID.ed25519().data(), 8));
            break;
        case viichain::TRUSTLINE:
        {
            auto& tl = lk.trustLine();
            res = viichain::shortHash::computeHash(
                viichain::ByteSlice(tl.accountID.ed25519().data(), 8));
            switch (lk.trustLine().asset.type())
            {
            case viichain::ASSET_TYPE_NATIVE:
                break;
            case viichain::ASSET_TYPE_CREDIT_ALPHANUM4:
            {
                auto& tl4 = tl.asset.alphaNum4();
                res ^= viichain::shortHash::computeHash(
                    viichain::ByteSlice(tl4.issuer.ed25519().data(), 8));
                res ^= tl4.assetCode[0];
                break;
            }
            case viichain::ASSET_TYPE_CREDIT_ALPHANUM12:
            {
                auto& tl12 = tl.asset.alphaNum12();
                res ^= viichain::shortHash::computeHash(
                    viichain::ByteSlice(tl12.issuer.ed25519().data(), 8));
                res ^= tl12.assetCode[0];
                break;
            }
            default:
                abort();
            }
            break;
        }
        case viichain::DATA:
            res = viichain::shortHash::computeHash(
                viichain::ByteSlice(lk.data().accountID.ed25519().data(), 8));
            res ^= viichain::shortHash::computeHash(viichain::ByteSlice(
                lk.data().dataName.data(), lk.data().dataName.size()));
            break;
        case viichain::OFFER:
            res = viichain::shortHash::computeHash(viichain::ByteSlice(
                &lk.offer().offerID, sizeof(lk.offer().offerID)));
            break;
        default:
            abort();
        }
        return res;
    }
};
}

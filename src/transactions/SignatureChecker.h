#pragma once


#include "xdr/vii-ledger-entries.h"
#include "xdr/vii-transaction.h"
#include "xdr/vii-types.h"

#include <map>
#include <set>
#include <stdint.h>
#include <vector>

namespace viichain
{

using UsedOneTimeSignerKeys = std::map<AccountID, std::set<SignerKey>>;

class SignatureChecker
{
  public:
    explicit SignatureChecker(
        uint32_t protocolVersion, Hash const& contentsHash,
        xdr::xvector<DecoratedSignature, 20> const& signatures);

    bool checkSignature(AccountID const& accountID,
                        std::vector<Signer> const& signersV,
                        int32_t neededWeight);
    bool checkAllSignaturesUsed() const;

    const UsedOneTimeSignerKeys& usedOneTimeSignerKeys() const;

  private:
    uint32_t mProtocolVersion;
    Hash const& mContentsHash;
    xdr::xvector<DecoratedSignature, 20> const& mSignatures;

    std::vector<bool> mUsedSignatures;
    UsedOneTimeSignerKeys mUsedOneTimeSignerKeys;
};
};

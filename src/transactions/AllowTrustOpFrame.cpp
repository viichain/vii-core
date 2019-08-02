
#include "transactions/AllowTrustOpFrame.h"
#include "database/Database.h"
#include "ledger/LedgerManager.h"
#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnEntry.h"
#include "ledger/LedgerTxnHeader.h"
#include "ledger/TrustLineWrapper.h"
#include "main/Application.h"
#include "transactions/TransactionUtils.h"

namespace viichain
{
AllowTrustOpFrame::AllowTrustOpFrame(Operation const& op, OperationResult& res,
                                     TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx)
    , mAllowTrust(mOperation.body.allowTrustOp())
{
}

ThresholdLevel
AllowTrustOpFrame::getThresholdLevel() const
{
    return ThresholdLevel::LOW;
}

bool
AllowTrustOpFrame::doApply(AbstractLedgerTxn& ltx)
{
    if (ltx.loadHeader().current().ledgerVersion > 2)
    {
        if (mAllowTrust.trustor == getSourceID())
        {
                        innerResult().code(ALLOW_TRUST_SELF_NOT_ALLOWED);
            return false;
        }
    }

    {
        LedgerTxn ltxSource(ltx); // ltxSource will be rolled back
        auto header = ltxSource.loadHeader();
        auto sourceAccountEntry = loadSourceAccount(ltxSource, header);
        auto const& sourceAccount = sourceAccountEntry.current().data.account();
        if (!(sourceAccount.flags & AUTH_REQUIRED_FLAG))
        { // this account doesn't require authorization to
                        innerResult().code(ALLOW_TRUST_TRUST_NOT_REQUIRED);
            return false;
        }

        if (!(sourceAccount.flags & AUTH_REVOCABLE_FLAG) &&
            !mAllowTrust.authorize)
        {
            innerResult().code(ALLOW_TRUST_CANT_REVOKE);
            return false;
        }
    }

        if (mAllowTrust.trustor == getSourceID())
    {
        innerResult().code(ALLOW_TRUST_SUCCESS);
        return true;
    }

    Asset ci;
    ci.type(mAllowTrust.asset.type());
    if (mAllowTrust.asset.type() == ASSET_TYPE_CREDIT_ALPHANUM4)
    {
        ci.alphaNum4().assetCode = mAllowTrust.asset.assetCode4();
        ci.alphaNum4().issuer = getSourceID();
    }
    else if (mAllowTrust.asset.type() == ASSET_TYPE_CREDIT_ALPHANUM12)
    {
        ci.alphaNum12().assetCode = mAllowTrust.asset.assetCode12();
        ci.alphaNum12().issuer = getSourceID();
    }

    LedgerKey key(TRUSTLINE);
    key.trustLine().accountID = mAllowTrust.trustor;
    key.trustLine().asset = ci;

    bool didRevokeAuth = false;
    {
        auto trust = ltx.load(key);
        if (!trust)
        {
            innerResult().code(ALLOW_TRUST_NO_TRUST_LINE);
            return false;
        }
        didRevokeAuth = isAuthorized(trust) && !mAllowTrust.authorize;
    }

    auto header = ltx.loadHeader();
    if (header.current().ledgerVersion >= 10 && didRevokeAuth)
    {
                        auto offers = ltx.loadOffersByAccountAndAsset(mAllowTrust.trustor, ci);
        for (auto& offer : offers)
        {
            auto const& oe = offer.current().data.offer();
            if (!(oe.sellerID == mAllowTrust.trustor))
            {
                throw std::runtime_error("Offer not owned by expected account");
            }
            else if (!(oe.buying == ci || oe.selling == ci))
            {
                throw std::runtime_error(
                    "Offer not buying or selling expected asset");
            }

            releaseLiabilities(ltx, header, offer);
            auto trustAcc = viichain::loadAccount(ltx, mAllowTrust.trustor);
            addNumEntries(header, trustAcc, -1);
            offer.erase();
        }
    }

    auto trustLineEntry = ltx.load(key);
    setAuthorized(trustLineEntry, mAllowTrust.authorize);

    innerResult().code(ALLOW_TRUST_SUCCESS);
    return true;
}

bool
AllowTrustOpFrame::doCheckValid(uint32_t ledgerVersion)
{
    if (mAllowTrust.asset.type() == ASSET_TYPE_NATIVE)
    {
        innerResult().code(ALLOW_TRUST_MALFORMED);
        return false;
    }
    Asset ci;
    ci.type(mAllowTrust.asset.type());
    if (mAllowTrust.asset.type() == ASSET_TYPE_CREDIT_ALPHANUM4)
    {
        ci.alphaNum4().assetCode = mAllowTrust.asset.assetCode4();
        ci.alphaNum4().issuer = getSourceID();
    }
    else if (mAllowTrust.asset.type() == ASSET_TYPE_CREDIT_ALPHANUM12)
    {
        ci.alphaNum12().assetCode = mAllowTrust.asset.assetCode12();
        ci.alphaNum12().issuer = getSourceID();
    }

    if (!isAssetValid(ci))
    {
        innerResult().code(ALLOW_TRUST_MALFORMED);
        return false;
    }

    return true;
}
}

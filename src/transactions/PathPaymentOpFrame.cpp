
#include "util/asio.h"
#include "transactions/PathPaymentOpFrame.h"
#include "OfferExchange.h"
#include "database/Database.h"
#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnEntry.h"
#include "ledger/LedgerTxnHeader.h"
#include "ledger/TrustLineWrapper.h"
#include "transactions/TransactionUtils.h"
#include "util/Logging.h"
#include "util/XDROperators.h"
#include <algorithm>

#include "main/Application.h"

namespace viichain
{

using namespace std;

PathPaymentOpFrame::PathPaymentOpFrame(Operation const& op,
                                       OperationResult& res,
                                       TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx)
    , mPathPayment(mOperation.body.pathPaymentOp())
{
}

bool
PathPaymentOpFrame::doApply(AbstractLedgerTxn& ltx)
{
    innerResult().code(PATH_PAYMENT_SUCCESS);

        int64_t curBReceived = mPathPayment.destAmount;
    Asset curB = mPathPayment.destAsset;

    
        std::vector<Asset> fullPath;
    fullPath.emplace_back(mPathPayment.sendAsset);
    fullPath.insert(fullPath.end(), mPathPayment.path.begin(),
                    mPathPayment.path.end());

    bool bypassIssuerCheck = false;

                    bypassIssuerCheck = (curB.type() != ASSET_TYPE_NATIVE) &&
                        (fullPath.size() == 1) &&
                        (mPathPayment.sendAsset == mPathPayment.destAsset) &&
                        (getIssuer(curB) == mPathPayment.destination);

    bool doesSourceAccountExist = true;
    if (ltx.loadHeader().current().ledgerVersion < 8)
    {
        doesSourceAccountExist =
            (bool)viichain::loadAccountWithoutRecord(ltx, getSourceID());
    }

    if (!bypassIssuerCheck)
    {
        if (!viichain::loadAccountWithoutRecord(ltx, mPathPayment.destination))
        {
            innerResult().code(PATH_PAYMENT_NO_DESTINATION);
            return false;
        }
    }

        if (curB.type() == ASSET_TYPE_NATIVE)
    {
        auto destination = viichain::loadAccount(ltx, mPathPayment.destination);
        if (!addBalance(ltx.loadHeader(), destination, curBReceived))
        {
            if (ltx.loadHeader().current().ledgerVersion >= 11)
            {
                innerResult().code(PATH_PAYMENT_LINE_FULL);
            }
            else
            {
                innerResult().code(PATH_PAYMENT_MALFORMED);
            }
            return false;
        }
    }
    else
    {
        if (!bypassIssuerCheck)
        {
            auto issuer =
                viichain::loadAccountWithoutRecord(ltx, getIssuer(curB));
            if (!issuer)
            {
                innerResult().code(PATH_PAYMENT_NO_ISSUER);
                innerResult().noIssuer() = curB;
                return false;
            }
        }

        auto destLine =
            viichain::loadTrustLine(ltx, mPathPayment.destination, curB);
        if (!destLine)
        {
            innerResult().code(PATH_PAYMENT_NO_TRUST);
            return false;
        }

        if (!destLine.isAuthorized())
        {
            innerResult().code(PATH_PAYMENT_NOT_AUTHORIZED);
            return false;
        }

        if (!destLine.addBalance(ltx.loadHeader(), curBReceived))
        {
            innerResult().code(PATH_PAYMENT_LINE_FULL);
            return false;
        }
    }

    innerResult().success().last =
        SimplePaymentResult(mPathPayment.destination, curB, curBReceived);

        for (int i = (int)fullPath.size() - 1; i >= 0; i--)
    {
        int64_t curASent, actualCurBReceived;
        Asset const& curA = fullPath[i];

        if (curA == curB)
        {
            continue;
        }

        if (curA.type() != ASSET_TYPE_NATIVE)
        {
            if (!viichain::loadAccountWithoutRecord(ltx, getIssuer(curA)))
            {
                innerResult().code(PATH_PAYMENT_NO_ISSUER);
                innerResult().noIssuer() = curA;
                return false;
            }
        }

        int64_t maxOffersToCross = INT64_MAX;
        if (ltx.loadHeader().current().ledgerVersion >=
            FIRST_PROTOCOL_SUPPORTING_OPERATION_LIMITS)
        {
            size_t offersCrossed = innerResult().success().offers.size();
                                                            maxOffersToCross = MAX_OFFERS_TO_CROSS - offersCrossed;
        }

                std::vector<ClaimOfferAtom> offerTrail;
        ConvertResult r = convertWithOffers(
            ltx, curA, INT64_MAX, curASent, curB, curBReceived,
            actualCurBReceived, true,
            [this](LedgerTxnEntry const& o) {
                auto const& offer = o.current().data.offer();
                if (offer.sellerID == getSourceID())
                {
                                        innerResult().code(PATH_PAYMENT_OFFER_CROSS_SELF);
                    return OfferFilterResult::eStop;
                }
                return OfferFilterResult::eKeep;
            },
            offerTrail, maxOffersToCross);

        assert(curASent >= 0);

        switch (r)
        {
        case ConvertResult::eFilterStop:
            return false;
        case ConvertResult::eOK:
            if (curBReceived == actualCurBReceived)
            {
                break;
            }
                case ConvertResult::ePartial:
            innerResult().code(PATH_PAYMENT_TOO_FEW_OFFERS);
            return false;
        case ConvertResult::eCrossedTooMany:
            mResult.code(opEXCEEDED_WORK_LIMIT);
            return false;
        }
        assert(curBReceived == actualCurBReceived);
        curBReceived = curASent; // next round, we need to send enough
        curB = curA;

                        auto& offers = innerResult().success().offers;
        offers.insert(offers.begin(), offerTrail.begin(), offerTrail.end());
    }

            int64_t curBSent = curBReceived;
    if (curBSent > mPathPayment.sendMax)
    { // make sure not over the max
        innerResult().code(PATH_PAYMENT_OVER_SENDMAX);
        return false;
    }

    if (curB.type() == ASSET_TYPE_NATIVE)
    {
        auto header = ltx.loadHeader();
        LedgerTxnEntry sourceAccount;
        if (header.current().ledgerVersion > 7)
        {
            sourceAccount = viichain::loadAccount(ltx, getSourceID());
            if (!sourceAccount)
            {
                innerResult().code(PATH_PAYMENT_MALFORMED);
                return false;
            }
        }
        else
        {
            sourceAccount = loadSourceAccount(ltx, header);
        }

        if (curBSent > getAvailableBalance(header, sourceAccount))
        { // they don't have enough to send
            innerResult().code(PATH_PAYMENT_UNDERFUNDED);
            return false;
        }

        if (!doesSourceAccountExist)
        {
            throw std::runtime_error("modifying account that does not exist");
        }

        auto ok = addBalance(header, sourceAccount, -curBSent);
        assert(ok);
    }
    else
    {
        if (!bypassIssuerCheck)
        {
            auto issuer =
                viichain::loadAccountWithoutRecord(ltx, getIssuer(curB));
            if (!issuer)
            {
                innerResult().code(PATH_PAYMENT_NO_ISSUER);
                innerResult().noIssuer() = curB;
                return false;
            }
        }

        auto sourceLine = loadTrustLine(ltx, getSourceID(), curB);
        if (!sourceLine)
        {
            innerResult().code(PATH_PAYMENT_SRC_NO_TRUST);
            return false;
        }

        if (!sourceLine.isAuthorized())
        {
            innerResult().code(PATH_PAYMENT_SRC_NOT_AUTHORIZED);
            return false;
        }

        if (!sourceLine.addBalance(ltx.loadHeader(), -curBSent))
        {
            innerResult().code(PATH_PAYMENT_UNDERFUNDED);
            return false;
        }
    }

    return true;
}

bool
PathPaymentOpFrame::doCheckValid(uint32_t ledgerVersion)
{
    if (mPathPayment.destAmount <= 0 || mPathPayment.sendMax <= 0)
    {
        innerResult().code(PATH_PAYMENT_MALFORMED);
        return false;
    }
    if (!isAssetValid(mPathPayment.sendAsset) ||
        !isAssetValid(mPathPayment.destAsset))
    {
        innerResult().code(PATH_PAYMENT_MALFORMED);
        return false;
    }
    auto const& p = mPathPayment.path;
    if (!std::all_of(p.begin(), p.end(), isAssetValid))
    {
        innerResult().code(PATH_PAYMENT_MALFORMED);
        return false;
    }
    return true;
}

void
PathPaymentOpFrame::insertLedgerKeysToPrefetch(
    std::unordered_set<LedgerKey>& keys) const
{
    keys.emplace(accountKey(mPathPayment.destination));

    auto processAsset = [&](Asset const& asset) {
        if (asset.type() != ASSET_TYPE_NATIVE)
        {
            auto issuer = getIssuer(asset);
            keys.emplace(accountKey(issuer));
        }
    };

    processAsset(mPathPayment.sendAsset);
    processAsset(mPathPayment.destAsset);
    std::for_each(mPathPayment.path.begin(), mPathPayment.path.end(),
                  processAsset);

    if (mPathPayment.destAsset.type() != ASSET_TYPE_NATIVE)
    {
        keys.emplace(
            trustlineKey(mPathPayment.destination, mPathPayment.destAsset));
    }
    if (mPathPayment.sendAsset.type() != ASSET_TYPE_NATIVE)
    {
        keys.emplace(trustlineKey(getSourceID(), mPathPayment.sendAsset));
    }
}
}

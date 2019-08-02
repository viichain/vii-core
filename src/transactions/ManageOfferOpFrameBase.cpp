
#include "transactions/ManageOfferOpFrameBase.h"
#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnEntry.h"
#include "ledger/LedgerTxnHeader.h"
#include "ledger/TrustLineWrapper.h"
#include "transactions/OfferExchange.h"
#include "transactions/TransactionUtils.h"

namespace viichain
{

ManageOfferOpFrameBase::ManageOfferOpFrameBase(
    Operation const& op, OperationResult& res, TransactionFrame& parentTx,
    Asset const& sheep, Asset const& wheat, int64_t offerID, Price const& price,
    bool setPassiveOnCreate)
    : OperationFrame(op, res, parentTx)
    , mSheep(sheep)
    , mWheat(wheat)
    , mOfferID(offerID)
    , mPrice(price)
    , mSetPassiveOnCreate(setPassiveOnCreate)
{
}

bool
ManageOfferOpFrameBase::checkOfferValid(AbstractLedgerTxn& ltxOuter)
{
    LedgerTxn ltx(ltxOuter); // ltx will always be rolled back

    if (isDeleteOffer())
    {
                return true;
    }

    if (mSheep.type() != ASSET_TYPE_NATIVE)
    {
        auto sheepLineA = loadTrustLine(ltx, getSourceID(), mSheep);
        auto issuer = viichain::loadAccount(ltx, getIssuer(mSheep));
        if (!issuer)
        {
            setResultSellNoIssuer();
            return false;
        }
        if (!sheepLineA)
        { // we don't have what we are trying to sell
            setResultSellNoTrust();
            return false;
        }
        if (sheepLineA.getBalance() == 0)
        {
            setResultUnderfunded();
            return false;
        }
        if (!sheepLineA.isAuthorized())
        {
                        setResultSellNotAuthorized();
            return false;
        }
    }

    if (mWheat.type() != ASSET_TYPE_NATIVE)
    {
        auto wheatLineA = loadTrustLine(ltx, getSourceID(), mWheat);
        auto issuer = viichain::loadAccount(ltx, getIssuer(mWheat));
        if (!issuer)
        {
            setResultBuyNoIssuer();
            return false;
        }
        if (!wheatLineA)
        { // we can't hold what we are trying to buy
            setResultBuyNoTrust();
            return false;
        }
        if (!wheatLineA.isAuthorized())
        { // we are not authorized to hold what we
                        setResultBuyNotAuthorized();
            return false;
        }
    }

    return true;
}

bool
ManageOfferOpFrameBase::computeOfferExchangeParameters(
    AbstractLedgerTxn& ltxOuter, bool creatingNewOffer, int64_t& maxSheepSend,
    int64_t& maxWheatReceive)
{
    LedgerTxn ltx(ltxOuter); // ltx will always be rolled back

    auto header = ltx.loadHeader();
    auto sourceAccount = loadSourceAccount(ltx, header);

    auto ledgerVersion = header.current().ledgerVersion;

    if (creatingNewOffer &&
        (ledgerVersion >= 10 ||
         (mSheep.type() == ASSET_TYPE_NATIVE && ledgerVersion > 8)))
    {
                                switch (addNumEntries(header, sourceAccount, 1))
        {
        case AddSubentryResult::SUCCESS:
            break;
        case AddSubentryResult::LOW_RESERVE:
            setResultLowReserve();
            return false;
        case AddSubentryResult::TOO_MANY_SUBENTRIES:
            mResult.code(opTOO_MANY_SUBENTRIES);
            return false;
        default:
            throw std::runtime_error("Unexpected result from addNumEntries");
        }
    }

    auto sheepLineA = loadTrustLineIfNotNative(ltx, getSourceID(), mSheep);
    auto wheatLineA = loadTrustLineIfNotNative(ltx, getSourceID(), mWheat);

    maxWheatReceive = canBuyAtMost(header, sourceAccount, mWheat, wheatLineA);
    maxSheepSend = canSellAtMost(header, sourceAccount, mSheep, sheepLineA);
    if (ledgerVersion >= 10)
    {
                                                                                                                                int64_t availableLimit =
            (mWheat.type() == ASSET_TYPE_NATIVE)
                ? getMaxAmountReceive(header, sourceAccount)
                : wheatLineA.getMaxAmountReceive(header);
        if (availableLimit < getOfferBuyingLiabilities())
        {
            setResultLineFull();
            return false;
        }

                                                                                                                                int64_t availableBalance =
            (mSheep.type() == ASSET_TYPE_NATIVE)
                ? getAvailableBalance(header, sourceAccount)
                : sheepLineA.getAvailableBalance(header);
        if (availableBalance < getOfferSellingLiabilities())
        {
            setResultUnderfunded();
            return false;
        }

        applyOperationSpecificLimits(maxSheepSend, 0, maxWheatReceive, 0);
    }
    else
    {
        getExchangeParametersBeforeV10(maxSheepSend, maxWheatReceive);
    }

    return true;
}

bool
ManageOfferOpFrameBase::doApply(AbstractLedgerTxn& ltxOuter)
{
    LedgerTxn ltx(ltxOuter);
    if (!checkOfferValid(ltx))
    {
        return false;
    }

    bool creatingNewOffer = false;
    bool passive = false;
    int64_t amount = 0;
    uint32_t flags = 0;

    if (mOfferID)
    { // modifying an old offer
        auto sellSheepOffer = viichain::loadOffer(ltx, getSourceID(), mOfferID);
        if (!sellSheepOffer)
        {
            setResultNotFound();
            return false;
        }

                                                auto header = ltx.loadHeader();
        if (header.current().ledgerVersion >= 10)
        {
            releaseLiabilities(ltx, header, sellSheepOffer);
        }

                flags = sellSheepOffer.current().data.offer().flags;
        passive = flags & PASSIVE_FLAG;

                                        sellSheepOffer.erase();
    }
    else
    { // creating a new Offer
        creatingNewOffer = true;
        passive = mSetPassiveOnCreate;
        flags = passive ? PASSIVE_FLAG : 0;
    }

    setResultSuccess();

    if (!isDeleteOffer())
    {
        int64_t maxSheepSend = 0;
        int64_t maxWheatReceive = 0;
        if (!computeOfferExchangeParameters(ltx, creatingNewOffer, maxSheepSend,
                                            maxWheatReceive))
        {
            return false;
        }

                if (maxWheatReceive == 0)
        {
            setResultLineFull();
            return false;
        }

        int64_t maxOffersToCross = INT64_MAX;
        if (ltx.loadHeader().current().ledgerVersion >=
            FIRST_PROTOCOL_SUPPORTING_OPERATION_LIMITS)
        {
            maxOffersToCross = MAX_OFFERS_TO_CROSS;
        }

        int64_t sheepSent, wheatReceived;
        std::vector<ClaimOfferAtom> offerTrail;
        Price maxWheatPrice(mPrice.d, mPrice.n);
        ConvertResult r = convertWithOffers(
            ltx, mSheep, maxSheepSend, sheepSent, mWheat, maxWheatReceive,
            wheatReceived, false,
            [this, passive, &maxWheatPrice](LedgerTxnEntry const& entry) {
                auto const& o = entry.current().data.offer();
                assert(o.offerID != mOfferID);
                if ((passive && (o.price >= maxWheatPrice)) ||
                    (o.price > maxWheatPrice))
                {
                    return OfferFilterResult::eStop;
                }
                if (o.sellerID == getSourceID())
                {
                                        setResultCrossSelf();
                    return OfferFilterResult::eStop;
                }
                return OfferFilterResult::eKeep;
            },
            offerTrail, maxOffersToCross);
        assert(sheepSent >= 0);

        bool sheepStays;
        switch (r)
        {
        case ConvertResult::eOK:
            sheepStays = false;
            break;
        case ConvertResult::ePartial:
            sheepStays = true;
            break;
        case ConvertResult::eFilterStop:
            if (!isResultSuccess())
            {
                return false;
            }
            sheepStays = true;
            break;
        case ConvertResult::eCrossedTooMany:
            mResult.code(opEXCEEDED_WORK_LIMIT);
            return false;
        default:
            abort();
        }

                for (auto const& oatom : offerTrail)
        {
            getSuccessResult().offersClaimed.push_back(oatom);
        }

        auto header = ltx.loadHeader();
        if (wheatReceived > 0)
        {
            if (mWheat.type() == ASSET_TYPE_NATIVE)
            {
                auto sourceAccount = loadSourceAccount(ltx, header);
                if (!addBalance(header, sourceAccount, wheatReceived))
                {
                                        throw std::runtime_error("offer claimed over limit");
                }
            }
            else
            {
                auto wheatLineA = loadTrustLine(ltx, getSourceID(), mWheat);
                if (!wheatLineA.addBalance(header, wheatReceived))
                {
                                        throw std::runtime_error("offer claimed over limit");
                }
            }

            if (mSheep.type() == ASSET_TYPE_NATIVE)
            {
                auto sourceAccount = loadSourceAccount(ltx, header);
                if (!addBalance(header, sourceAccount, -sheepSent))
                {
                                        throw std::runtime_error("offer sold more than balance");
                }
            }
            else
            {
                auto sheepLineA = loadTrustLine(ltx, getSourceID(), mSheep);
                if (!sheepLineA.addBalance(header, -sheepSent))
                {
                                        throw std::runtime_error("offer sold more than balance");
                }
            }
        }

        if (header.current().ledgerVersion >= 10)
        {
            if (sheepStays)
            {
                auto sourceAccount =
                    viichain::loadAccountWithoutRecord(ltx, getSourceID());
                auto sheepLineA = loadTrustLineWithoutRecordIfNotNative(
                    ltx, getSourceID(), mSheep);
                auto wheatLineA = loadTrustLineWithoutRecordIfNotNative(
                    ltx, getSourceID(), mWheat);

                int64_t sheepSendLimit =
                    canSellAtMost(header, sourceAccount, mSheep, sheepLineA);
                int64_t wheatReceiveLimit =
                    canBuyAtMost(header, sourceAccount, mWheat, wheatLineA);
                applyOperationSpecificLimits(sheepSendLimit, sheepSent,
                                             wheatReceiveLimit, wheatReceived);
                amount = adjustOffer(mPrice, sheepSendLimit, wheatReceiveLimit);
            }
            else
            {
                amount = 0;
            }
        }
        else
        {
            amount = maxSheepSend - sheepSent;
        }
    }

    auto header = ltx.loadHeader();
    if (amount > 0)
    {
        auto newOffer = buildOffer(amount, flags);
        if (creatingNewOffer)
        {
                                    auto sourceAccount = loadSourceAccount(ltx, header);
            switch (addNumEntries(header, sourceAccount, 1))
            {
            case AddSubentryResult::SUCCESS:
                break;
            case AddSubentryResult::LOW_RESERVE:
                setResultLowReserve();
                return false;
            case AddSubentryResult::TOO_MANY_SUBENTRIES:
                mResult.code(opTOO_MANY_SUBENTRIES);
                return false;
            default:
                throw std::runtime_error(
                    "Unexpected result from addNumEntries");
            }

            newOffer.data.offer().offerID = generateID(header);
            getSuccessResult().offer.effect(MANAGE_OFFER_CREATED);
        }
        else
        {
            getSuccessResult().offer.effect(MANAGE_OFFER_UPDATED);
        }

        auto sellSheepOffer = ltx.create(newOffer);
        getSuccessResult().offer.offer() =
            sellSheepOffer.current().data.offer();

        if (header.current().ledgerVersion >= 10)
        {
            acquireLiabilities(ltx, header, sellSheepOffer);
        }
    }
    else
    {
        getSuccessResult().offer.effect(MANAGE_OFFER_DELETED);

        if (!creatingNewOffer)
        {
            auto sourceAccount = loadSourceAccount(ltx, header);
            addNumEntries(header, sourceAccount, -1);
        }
    }

    ltx.commit();
    return true;
}

LedgerEntry
ManageOfferOpFrameBase::buildOffer(int64_t amount, uint32_t flags) const
{
    LedgerEntry le;
    le.data.type(OFFER);
    OfferEntry& o = le.data.offer();

    o.sellerID = getSourceID();
    o.amount = amount;
    o.price = mPrice;
    o.offerID = mOfferID;
    o.selling = mSheep;
    o.buying = mWheat;
    o.flags = flags;
    return le;
}

bool
ManageOfferOpFrameBase::doCheckValid(uint32_t ledgerVersion)
{
    if (!isAssetValid(mSheep) || !isAssetValid(mWheat))
    {
        setResultMalformed();
        return false;
    }
    if (compareAsset(mSheep, mWheat))
    {
        setResultMalformed();
        return false;
    }

    if (!isAmountValid() || mPrice.d <= 0 || mPrice.n <= 0)
    {
        setResultMalformed();
        return false;
    }

    if (mOfferID == 0 && isDeleteOffer())
    {
        if (ledgerVersion >= 11)
        {
            setResultMalformed();
            return false;
        }
        else if (ledgerVersion >= 3)
        {
            setResultNotFound();
            return false;
        }
            }

    return true;
}

void
ManageOfferOpFrameBase::insertLedgerKeysToPrefetch(
    std::unordered_set<LedgerKey>& keys) const
{
    if (isDeleteOffer())
    {
        return;
    }

        if (mOfferID)
    {
        keys.emplace(offerKey(getSourceID(), mOfferID));
    }

    auto addIssuerAndTrustline = [&](Asset const& asset) {
        if (asset.type() != ASSET_TYPE_NATIVE)
        {
            auto issuer = getIssuer(asset);
            keys.emplace(accountKey(issuer));
            keys.emplace(trustlineKey(this->getSourceID(), asset));
        }
    };

    addIssuerAndTrustline(mSheep);
    addIssuerAndTrustline(mWheat);
}
}

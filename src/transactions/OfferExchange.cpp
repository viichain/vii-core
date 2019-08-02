
#include "OfferExchange.h"
#include "database/Database.h"
#include "ledger/LedgerManager.h"
#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnEntry.h"
#include "ledger/LedgerTxnHeader.h"
#include "ledger/TrustLineWrapper.h"
#include "lib/util/uint128_t.h"
#include "transactions/TransactionUtils.h"
#include "util/Logging.h"

namespace viichain
{
int64_t
canSellAtMostBasedOnSheep(LedgerTxnHeader const& header, Asset const& sheep,
                          ConstTrustLineWrapper const& sheepLine,
                          Price const& wheatPrice)
{
    if (sheep.type() == ASSET_TYPE_NATIVE)
    {
        return INT64_MAX;
    }

        auto sellerMaxSheep = sheepLine ? sheepLine.getMaxAmountReceive(header) : 0;

    auto wheatAmount = int64_t{};
    if (!bigDivide(wheatAmount, sellerMaxSheep, wheatPrice.d, wheatPrice.n,
                   ROUND_DOWN))
    {
        wheatAmount = INT64_MAX;
    }

    return wheatAmount;
}

int64_t
canSellAtMost(LedgerTxnHeader const& header, LedgerTxnEntry const& account,
              Asset const& asset, TrustLineWrapper const& trustLine)
{
    if (asset.type() == ASSET_TYPE_NATIVE)
    {
                return std::max({getAvailableBalance(header, account), int64_t(0)});
    }

    if (trustLine && trustLine.isAuthorized())
    {
        return std::max({trustLine.getAvailableBalance(header), int64_t(0)});
    }

    return 0;
}

int64_t
canSellAtMost(LedgerTxnHeader const& header, ConstLedgerTxnEntry const& account,
              Asset const& asset, ConstTrustLineWrapper const& trustLine)
{
    if (asset.type() == ASSET_TYPE_NATIVE)
    {
                return std::max({getAvailableBalance(header, account), int64_t(0)});
    }

    if (trustLine && trustLine.isAuthorized())
    {
        return std::max({trustLine.getAvailableBalance(header), int64_t(0)});
    }

    return 0;
}

int64_t
canBuyAtMost(LedgerTxnHeader const& header, LedgerTxnEntry const& account,
             Asset const& asset, TrustLineWrapper const& trustLine)
{
    if (asset.type() == ASSET_TYPE_NATIVE)
    {
        return std::max({getMaxAmountReceive(header, account), int64_t(0)});
    }
    else
    {
        return trustLine ? std::max({trustLine.getMaxAmountReceive(header),
                                     int64_t(0)})
                         : 0;
    }
}

int64_t
canBuyAtMost(LedgerTxnHeader const& header, ConstLedgerTxnEntry const& account,
             Asset const& asset, ConstTrustLineWrapper const& trustLine)
{
    if (asset.type() == ASSET_TYPE_NATIVE)
    {
        return std::max({getMaxAmountReceive(header, account), int64_t(0)});
    }
    else
    {
        return trustLine ? std::max({trustLine.getMaxAmountReceive(header),
                                     int64_t(0)})
                         : 0;
    }
}

ExchangeResult
exchangeV2(int64_t wheatReceived, Price price, int64_t maxWheatReceive,
           int64_t maxSheepSend)
{
    auto result = ExchangeResult{};
    result.reduced = wheatReceived > maxWheatReceive;
    wheatReceived = std::min(wheatReceived, maxWheatReceive);

        if (!bigDivide(result.numSheepSend, wheatReceived, price.n, price.d,
                   ROUND_DOWN))
    {
        result.numSheepSend = INT64_MAX;
    }

    result.reduced = result.reduced || (result.numSheepSend > maxSheepSend);
    result.numSheepSend = std::min(result.numSheepSend, maxSheepSend);
        result.numWheatReceived =
        bigDivide(result.numSheepSend, price.d, price.n, ROUND_DOWN);

    return result;
}

ExchangeResult
exchangeV3(int64_t wheatReceived, Price price, int64_t maxWheatReceive,
           int64_t maxSheepSend)
{
    auto result = ExchangeResult{};
    result.reduced = wheatReceived > maxWheatReceive;
    result.numWheatReceived = std::min(wheatReceived, maxWheatReceive);

            if (!bigDivide(result.numSheepSend, result.numWheatReceived, price.n,
                   price.d, ROUND_UP))
    {
        result.reduced = true;
        result.numSheepSend = INT64_MAX;
    }

    result.reduced = result.reduced || (result.numSheepSend > maxSheepSend);
    result.numSheepSend = std::min(result.numSheepSend, maxSheepSend);

    auto newWheatReceived = int64_t{};
    if (!bigDivide(newWheatReceived, result.numSheepSend, price.d, price.n,
                   ROUND_DOWN))
    {
        newWheatReceived = INT64_MAX;
    }
    result.numWheatReceived =
        std::min(result.numWheatReceived, newWheatReceived);

    return result;
}

bool
checkPriceErrorBound(Price price, int64_t wheatReceive, int64_t sheepSend,
                     bool canFavorWheat)
{
                                        
        int64_t errN = (int64_t)100 * (int64_t)price.n;
    int64_t errD = (int64_t)100 * (int64_t)price.d;

    uint128_t lhs = bigMultiply(errN, wheatReceive);
    uint128_t rhs = bigMultiply(errD, sheepSend);

    if (canFavorWheat && rhs > lhs)
    {
        return true;
    }

    uint128_t absDiff = (lhs > rhs) ? (lhs - rhs) : (rhs - lhs);
    uint128_t cap = bigMultiply(price.n, wheatReceive);
    return (absDiff <= cap);
}

static uint128_t
calculateOfferValue(int32_t priceN, int32_t priceD, int64_t maxSend,
                    int64_t maxReceive)
{
    uint128_t sendValue = bigMultiply(maxSend, priceN);
    uint128_t receiveValue = bigMultiply(maxReceive, priceD);
    return std::min({sendValue, receiveValue});
}

ExchangeResultV10
exchangeV10(Price price, int64_t maxWheatSend, int64_t maxWheatReceive,
            int64_t maxSheepSend, int64_t maxSheepReceive, bool isPathPayment)
{
    auto beforeThresholds = exchangeV10WithoutPriceErrorThresholds(
        price, maxWheatSend, maxWheatReceive, maxSheepSend, maxSheepReceive,
        isPathPayment);
    return applyPriceErrorThresholds(
        price, beforeThresholds.numWheatReceived, beforeThresholds.numSheepSend,
        beforeThresholds.wheatStays, isPathPayment);
}

ExchangeResultV10
exchangeV10WithoutPriceErrorThresholds(Price price, int64_t maxWheatSend,
                                       int64_t maxWheatReceive,
                                       int64_t maxSheepSend,
                                       int64_t maxSheepReceive,
                                       bool isPathPayment)
{
    uint128_t wheatValue =
        calculateOfferValue(price.n, price.d, maxWheatSend, maxSheepReceive);
    uint128_t sheepValue =
        calculateOfferValue(price.d, price.n, maxSheepSend, maxWheatReceive);
    bool wheatStays = (wheatValue > sheepValue);

    int64_t wheatReceive;
    int64_t sheepSend;
    if (wheatStays)
    {
        if (price.n > price.d || isPathPayment) // Wheat is more valuable
        {
            wheatReceive = bigDivide(sheepValue, price.n, ROUND_DOWN);
            sheepSend = bigDivide(wheatReceive, price.n, price.d, ROUND_UP);
        }
        else // Sheep is more valuable
        {
            sheepSend = bigDivide(sheepValue, price.d, ROUND_DOWN);
            wheatReceive = bigDivide(sheepSend, price.d, price.n, ROUND_DOWN);
        }
    }
    else
    {
        if (price.n > price.d) // Wheat is more valuable
        {
            wheatReceive = bigDivide(wheatValue, price.n, ROUND_DOWN);
            sheepSend = bigDivide(wheatReceive, price.n, price.d, ROUND_DOWN);
        }
        else // Sheep is more valuable
        {
            sheepSend = bigDivide(wheatValue, price.d, ROUND_DOWN);
            wheatReceive = bigDivide(sheepSend, price.d, price.n, ROUND_UP);
        }
    }

        if (wheatReceive < 0 ||
        wheatReceive > std::min({maxWheatReceive, maxWheatSend}))
    {
        throw std::runtime_error("wheatReceive out of bounds");
    }
    if (sheepSend < 0 || sheepSend > std::min({maxSheepReceive, maxSheepSend}))
    {
        throw std::runtime_error("sheepSend out of bounds");
    }

    ExchangeResultV10 res;
    res.numWheatReceived = wheatReceive;
    res.numSheepSend = sheepSend;
    res.wheatStays = wheatStays;
    return res;
}

ExchangeResultV10
applyPriceErrorThresholds(Price price, int64_t wheatReceive, int64_t sheepSend,
                          bool wheatStays, bool isPathPayment)
{
    if (wheatReceive > 0 && sheepSend > 0)
    {
        uint128_t wheatReceiveValue = bigMultiply(wheatReceive, price.n);
        uint128_t sheepSendValue = bigMultiply(sheepSend, price.d);

                                if (wheatStays && sheepSendValue < wheatReceiveValue)
        {
            throw std::runtime_error("favored sheep when wheat stays");
        }
        if (!wheatStays && sheepSendValue > wheatReceiveValue)
        {
            throw std::runtime_error("favored wheat when sheep stays");
        }

        if (!isPathPayment)
        {
                                    if (!checkPriceErrorBound(price, wheatReceive, sheepSend, false))
            {
                sheepSend = 0;
                wheatReceive = 0;
            }
        }
        else
        {
                                                                                                            if (!checkPriceErrorBound(price, wheatReceive, sheepSend, true))
            {
                throw std::runtime_error("exceeded price error bound");
            }
        }
    }
    else
    {
                                wheatReceive = 0;
        sheepSend = 0;
    }

    ExchangeResultV10 res;
    res.numWheatReceived = wheatReceive;
    res.numSheepSend = sheepSend;
    res.wheatStays = wheatStays;
    return res;
}

void
adjustOffer(LedgerTxnHeader const& header, LedgerTxnEntry& offer,
            LedgerTxnEntry const& account, Asset const& wheat,
            TrustLineWrapper const& wheatLine, Asset const& sheep,
            TrustLineWrapper const& sheepLine)
{
    OfferEntry& oe = offer.current().data.offer();
    int64_t maxWheatSend =
        std::min({oe.amount, canSellAtMost(header, account, wheat, wheatLine)});
    int64_t maxSheepReceive = canBuyAtMost(header, account, sheep, sheepLine);
    oe.amount = adjustOffer(oe.price, maxWheatSend, maxSheepReceive);
}

int64_t
adjustOffer(Price const& price, int64_t maxWheatSend, int64_t maxSheepReceive)
{
    auto res = exchangeV10(price, maxWheatSend, INT64_MAX, INT64_MAX,
                           maxSheepReceive, false);
    return res.numWheatReceived;
}

static ExchangeResultType
performExchange(LedgerTxnHeader const& header,
                LedgerTxnEntry const& sellingWheatOffer,
                ConstLedgerTxnEntry const& accountB,
                ConstTrustLineWrapper const& wheatLineAccountB,
                ConstTrustLineWrapper const& sheepLineAccountB,
                int64_t maxWheatReceived, int64_t& numWheatReceived,
                int64_t maxSheepSend, int64_t& numSheepSend, int64_t& newAmount)
{
    auto const& offer = sellingWheatOffer.current().data.offer();
    Asset const& sheep = offer.buying;
    Asset const& wheat = offer.selling;
    Price const& price = offer.price;

    numWheatReceived = std::min(
        {canSellAtMostBasedOnSheep(header, sheep, sheepLineAccountB, price),
         canSellAtMost(header, accountB, wheat, wheatLineAccountB),
         offer.amount});
    assert(numWheatReceived >= 0);

    newAmount = numWheatReceived;
    auto exchangeResult = header.current().ledgerVersion < 3
                              ? exchangeV2(numWheatReceived, price,
                                           maxWheatReceived, maxSheepSend)
                              : exchangeV3(numWheatReceived, price,
                                           maxWheatReceived, maxSheepSend);

    numWheatReceived = exchangeResult.numWheatReceived;
    numSheepSend = exchangeResult.numSheepSend;

    bool offerTaken = false;
    switch (exchangeResult.type())
    {
    case ExchangeResultType::REDUCED_TO_ZERO:
        return ExchangeResultType::REDUCED_TO_ZERO;
    case ExchangeResultType::BOGUS:
                numWheatReceived = 0;
        numSheepSend = 0;
        offerTaken = true;
        break;
    default:
        break;
    }

    offerTaken = offerTaken || offer.amount <= numWheatReceived;
    newAmount = offerTaken ? 0 : (newAmount - numWheatReceived);
    return ExchangeResultType::NORMAL;
}

static CrossOfferResult
crossOffer(AbstractLedgerTxn& ltx, LedgerTxnEntry& sellingWheatOffer,
           int64_t maxWheatReceived, int64_t& numWheatReceived,
           int64_t maxSheepSend, int64_t& numSheepSend,
           std::vector<ClaimOfferAtom>& offerTrail)
{
    assert(maxWheatReceived > 0);
    assert(maxSheepSend > 0);

    auto& offer = sellingWheatOffer.current().data.offer();
            Asset sheep = offer.buying;
    Asset wheat = offer.selling;
    AccountID accountBID = offer.sellerID;
    int64_t offerID = offer.offerID;

    int64_t newAmount = offer.amount;
    {
        auto accountB = viichain::loadAccountWithoutRecord(ltx, accountBID);
        if (!accountB)
        {
            throw std::runtime_error(
                "invalid database state: offer must have matching account");
        }

        auto sheepLineAccountB =
            loadTrustLineWithoutRecordIfNotNative(ltx, accountBID, sheep);
        auto wheatLineAccountB =
            loadTrustLineWithoutRecordIfNotNative(ltx, accountBID, wheat);

        auto exchangeResult = performExchange(
            ltx.loadHeader(), sellingWheatOffer, accountB, wheatLineAccountB,
            sheepLineAccountB, maxWheatReceived, numWheatReceived, maxSheepSend,
            numSheepSend, newAmount);
        if (exchangeResult == ExchangeResultType::REDUCED_TO_ZERO)
        {
            return CrossOfferResult::eOfferCantConvert;
        }
    }

        if (newAmount == 0)
    { // entire offer is taken
        sellingWheatOffer.erase();
        auto accountB = viichain::loadAccount(ltx, accountBID);
        addNumEntries(ltx.loadHeader(), accountB, -1);
    }
    else
    {
        offer.amount = newAmount;
    }

        if (numSheepSend != 0)
    {
                                        LedgerTxn ltxInner(ltx);
        auto header = ltxInner.loadHeader();
        if (sheep.type() == ASSET_TYPE_NATIVE)
        {
            auto accountB = viichain::loadAccount(ltxInner, accountBID);
            if (!addBalance(header, accountB, numSheepSend))
            {
                return CrossOfferResult::eOfferCantConvert;
            }
        }
        else
        {
            auto sheepLineAccountB =
                viichain::loadTrustLine(ltxInner, accountBID, sheep);
            if (!sheepLineAccountB.addBalance(header, numSheepSend))
            {
                return CrossOfferResult::eOfferCantConvert;
            }
        }
        ltxInner.commit();
    }

    if (numWheatReceived != 0)
    {
                                        LedgerTxn ltxInner(ltx);
        auto header = ltxInner.loadHeader();
        if (wheat.type() == ASSET_TYPE_NATIVE)
        {
            auto accountB = viichain::loadAccount(ltxInner, accountBID);
            if (!addBalance(header, accountB, -numWheatReceived))
            {
                return CrossOfferResult::eOfferCantConvert;
            }
        }
        else
        {
            auto wheatLineAccountB =
                viichain::loadTrustLine(ltxInner, accountBID, wheat);
            if (!wheatLineAccountB.addBalance(header, -numWheatReceived))
            {
                return CrossOfferResult::eOfferCantConvert;
            }
        }
        ltxInner.commit();
    }

    offerTrail.push_back(ClaimOfferAtom(accountBID, offerID, wheat,
                                        numWheatReceived, sheep, numSheepSend));
    return (newAmount == 0) ? CrossOfferResult::eOfferTaken
                            : CrossOfferResult::eOfferPartial;
}

static CrossOfferResult
crossOfferV10(AbstractLedgerTxn& ltx, LedgerTxnEntry& sellingWheatOffer,
              int64_t maxWheatReceived, int64_t& numWheatReceived,
              int64_t maxSheepSend, int64_t& numSheepSend, bool& wheatStays,
              bool isPathPayment, std::vector<ClaimOfferAtom>& offerTrail)
{
    assert(maxWheatReceived > 0);
    assert(maxSheepSend > 0);
    auto header = ltx.loadHeader();

    auto& offer = sellingWheatOffer.current().data.offer();
    Asset sheep = offer.buying;
    Asset wheat = offer.selling;
    AccountID accountBID = offer.sellerID;
    int64_t offerID = offer.offerID;

    if (!viichain::loadAccountWithoutRecord(ltx, accountBID))
    {
        throw std::runtime_error(
            "invalid database state: offer must have matching account");
    }

        releaseLiabilities(ltx, header, sellingWheatOffer);

            LedgerTxnEntry accountB;
    if (wheat.type() == ASSET_TYPE_NATIVE || sheep.type() == ASSET_TYPE_NATIVE)
    {
        accountB = viichain::loadAccount(ltx, accountBID);
    }
    auto sheepLineAccountB = loadTrustLineIfNotNative(ltx, accountBID, sheep);
    auto wheatLineAccountB = loadTrustLineIfNotNative(ltx, accountBID, wheat);

            adjustOffer(header, sellingWheatOffer, accountB, wheat, wheatLineAccountB,
                sheep, sheepLineAccountB);

    int64_t maxWheatSend =
        canSellAtMost(header, accountB, wheat, wheatLineAccountB);
    maxWheatSend = std::min({offer.amount, maxWheatSend});
    int64_t maxSheepReceive =
        canBuyAtMost(header, accountB, sheep, sheepLineAccountB);
    auto exchangeResult =
        exchangeV10(offer.price, maxWheatSend, maxWheatReceived, maxSheepSend,
                    maxSheepReceive, isPathPayment);

    numWheatReceived = exchangeResult.numWheatReceived;
    numSheepSend = exchangeResult.numSheepSend;
    wheatStays = exchangeResult.wheatStays;

        if (numSheepSend != 0)
    {
        if (sheep.type() == ASSET_TYPE_NATIVE)
        {
            if (!addBalance(header, accountB, numSheepSend))
            {
                throw std::runtime_error("overflowed sheep balance");
            }
        }
        else
        {
            if (!sheepLineAccountB.addBalance(header, numSheepSend))
            {
                throw std::runtime_error("overflowed sheep balance");
            }
        }
    }

    if (numWheatReceived != 0)
    {
        if (wheat.type() == ASSET_TYPE_NATIVE)
        {
            if (!addBalance(header, accountB, -numWheatReceived))
            {
                throw std::runtime_error("overflowed wheat balance");
            }
        }
        else
        {
            if (!wheatLineAccountB.addBalance(header, -numWheatReceived))
            {
                throw std::runtime_error("overflowed wheat balance");
            }
        }
    }

    if (wheatStays)
    {
        offer.amount -= numWheatReceived;
        adjustOffer(header, sellingWheatOffer, accountB, wheat,
                    wheatLineAccountB, sheep, sheepLineAccountB);
    }
    else
    {
        offer.amount = 0;
    }

    auto res = (offer.amount == 0) ? CrossOfferResult::eOfferTaken
                                   : CrossOfferResult::eOfferPartial;
    {
        LedgerTxn ltxInner(ltx);
        header = ltxInner.loadHeader();
        sellingWheatOffer = loadOffer(ltxInner, accountBID, offerID);
        if (res == CrossOfferResult::eOfferTaken)
        {
            sellingWheatOffer.erase();
            accountB = viichain::loadAccount(ltxInner, accountBID);
            addNumEntries(header, accountB, -1);
        }
        else
        {
            acquireLiabilities(ltxInner, header, sellingWheatOffer);
        }
        ltxInner.commit();
    }

                    offerTrail.push_back(ClaimOfferAtom(accountBID, offerID, wheat,
                                        numWheatReceived, sheep, numSheepSend));
    return res;
}

ConvertResult
convertWithOffers(
    AbstractLedgerTxn& ltxOuter, Asset const& sheep, int64_t maxSheepSend,
    int64_t& sheepSend, Asset const& wheat, int64_t maxWheatReceive,
    int64_t& wheatReceived, bool isPathPayment,
    std::function<OfferFilterResult(LedgerTxnEntry const&)> filter,
    std::vector<ClaimOfferAtom>& offerTrail, int64_t maxOffersToCross)
{
            assert(offerTrail.empty());

    sheepSend = 0;
    wheatReceived = 0;

    bool needMore = (maxWheatReceive > 0 && maxSheepSend > 0);
    while (needMore)
    {
        LedgerTxn ltx(ltxOuter);
        auto wheatOffer = ltx.loadBestOffer(sheep, wheat);
        if (!wheatOffer)
        {
            break;
        }
        if (filter && filter(wheatOffer) == OfferFilterResult::eStop)
        {
            return ConvertResult::eFilterStop;
        }

                if (offerTrail.size() >= static_cast<uint64_t>(maxOffersToCross))
        {
            return ConvertResult::eCrossedTooMany;
        }

        int64_t numWheatReceived;
        int64_t numSheepSend;
        CrossOfferResult cor;
        if (ltx.loadHeader().current().ledgerVersion >= 10)
        {
            bool wheatStays;
            cor = crossOfferV10(ltx, wheatOffer, maxWheatReceive,
                                numWheatReceived, maxSheepSend, numSheepSend,
                                wheatStays, isPathPayment, offerTrail);
            needMore = !wheatStays;
        }
        else
        {
            cor = crossOffer(ltx, wheatOffer, maxWheatReceive, numWheatReceived,
                             maxSheepSend, numSheepSend, offerTrail);
            needMore = true;
        }

        assert(numSheepSend >= 0);
        assert(numSheepSend <= maxSheepSend);
        assert(numWheatReceived >= 0);
        assert(numWheatReceived <= maxWheatReceive);

        if (cor == CrossOfferResult::eOfferCantConvert)
        {
            return ConvertResult::ePartial;
        }
        ltx.commit();

        sheepSend += numSheepSend;
        maxSheepSend -= numSheepSend;

        wheatReceived += numWheatReceived;
        maxWheatReceive -= numWheatReceived;

        needMore = needMore && (maxWheatReceive > 0 && maxSheepSend > 0);
        if (!needMore)
        {
            return ConvertResult::eOK;
        }
        else if (cor == CrossOfferResult::eOfferPartial)
        {
            return ConvertResult::ePartial;
        }
    }
    if ((ltxOuter.loadHeader().current().ledgerVersion < 10) || !needMore)
    {
        return ConvertResult::eOK;
    }
    else
    {
        return ConvertResult::ePartial;
    }
}
}

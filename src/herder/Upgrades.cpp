
#include "herder/Upgrades.h"
#include "database/Database.h"
#include "database/DatabaseUtils.h"
#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnEntry.h"
#include "ledger/LedgerTxnHeader.h"
#include "ledger/TrustLineWrapper.h"
#include "main/Config.h"
#include "transactions/OfferExchange.h"
#include "transactions/TransactionUtils.h"
#include "util/Decoder.h"
#include "util/Logging.h"
#include "util/Timer.h"
#include "util/types.h"
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <lib/util/format.h>
#include <xdrpp/marshal.h>

namespace cereal
{
template <class Archive>
void
save(Archive& ar, viichain::Upgrades::UpgradeParameters const& p)
{
    ar(make_nvp("time", viichain::VirtualClock::to_time_t(p.mUpgradeTime)));
    ar(make_nvp("version", p.mProtocolVersion));
    ar(make_nvp("fee", p.mBaseFee));
    ar(make_nvp("maxtxsize", p.mMaxTxSize));
    ar(make_nvp("reserve", p.mBaseReserve));
}

template <class Archive>
void
load(Archive& ar, viichain::Upgrades::UpgradeParameters& o)
{
    time_t t;
    ar(make_nvp("time", t));
    o.mUpgradeTime = viichain::VirtualClock::from_time_t(t);
    ar(make_nvp("version", o.mProtocolVersion));
    ar(make_nvp("fee", o.mBaseFee));
    ar(make_nvp("maxtxsize", o.mMaxTxSize));
    ar(make_nvp("reserve", o.mBaseReserve));
}
} // namespace cereal

namespace viichain
{
std::string
Upgrades::UpgradeParameters::toJson() const
{
    std::ostringstream out;
    {
        cereal::JSONOutputArchive ar(out);
        cereal::save(ar, *this);
    }
    return out.str();
}

void
Upgrades::UpgradeParameters::fromJson(std::string const& s)
{
    std::istringstream in(s);
    {
        cereal::JSONInputArchive ar(in);
        cereal::load(ar, *this);
    }
}

Upgrades::Upgrades(UpgradeParameters const& params) : mParams(params)
{
}

void
Upgrades::setParameters(UpgradeParameters const& params, Config const& cfg)
{
    if (params.mProtocolVersion &&
        *params.mProtocolVersion > cfg.LEDGER_PROTOCOL_VERSION)
    {
        throw std::invalid_argument(fmt::format(
            "Protocol version error: supported is up to {}, passed is {}",
            cfg.LEDGER_PROTOCOL_VERSION, *params.mProtocolVersion));
    }
    mParams = params;
}

Upgrades::UpgradeParameters const&
Upgrades::getParameters() const
{
    return mParams;
}

std::vector<LedgerUpgrade>
Upgrades::createUpgradesFor(LedgerHeader const& header) const
{
    auto result = std::vector<LedgerUpgrade>{};
    if (!timeForUpgrade(header.scpValue.closeTime))
    {
        return result;
    }

    if (mParams.mProtocolVersion &&
        (header.ledgerVersion != *mParams.mProtocolVersion))
    {
        result.emplace_back(LEDGER_UPGRADE_VERSION);
        result.back().newLedgerVersion() = *mParams.mProtocolVersion;
    }
    if (mParams.mBaseFee && (header.baseFee != *mParams.mBaseFee))
    {
        result.emplace_back(LEDGER_UPGRADE_BASE_FEE);
        result.back().newBaseFee() = *mParams.mBaseFee;
    }
    if (mParams.mMaxTxSize && (header.maxTxSetSize != *mParams.mMaxTxSize))
    {
        result.emplace_back(LEDGER_UPGRADE_MAX_TX_SET_SIZE);
        result.back().newMaxTxSetSize() = *mParams.mMaxTxSize;
    }
    if (mParams.mBaseReserve && (header.baseReserve != *mParams.mBaseReserve))
    {
        result.emplace_back(LEDGER_UPGRADE_BASE_RESERVE);
        result.back().newBaseReserve() = *mParams.mBaseReserve;
    }

    return result;
}

void
Upgrades::applyTo(LedgerUpgrade const& upgrade, AbstractLedgerTxn& ltx)
{
    switch (upgrade.type())
    {
    case LEDGER_UPGRADE_VERSION:
        applyVersionUpgrade(ltx, upgrade.newLedgerVersion());
        break;
    case LEDGER_UPGRADE_BASE_FEE:
        ltx.loadHeader().current().baseFee = upgrade.newBaseFee();
        break;
    case LEDGER_UPGRADE_MAX_TX_SET_SIZE:
        ltx.loadHeader().current().maxTxSetSize = upgrade.newMaxTxSetSize();
        break;
    case LEDGER_UPGRADE_BASE_RESERVE:
        applyReserveUpgrade(ltx, upgrade.newBaseReserve());
        break;
    default:
    {
        auto s = fmt::format("Unknown upgrade type: {0}", upgrade.type());
        throw std::runtime_error(s);
    }
    }
}

std::string
Upgrades::toString(LedgerUpgrade const& upgrade)
{
    switch (upgrade.type())
    {
    case LEDGER_UPGRADE_VERSION:
        return fmt::format("protocolversion={0}", upgrade.newLedgerVersion());
    case LEDGER_UPGRADE_BASE_FEE:
        return fmt::format("basefee={0}", upgrade.newBaseFee());
    case LEDGER_UPGRADE_MAX_TX_SET_SIZE:
        return fmt::format("maxtxsetsize={0}", upgrade.newMaxTxSetSize());
    case LEDGER_UPGRADE_BASE_RESERVE:
        return fmt::format("basereserve={0}", upgrade.newBaseReserve());
    default:
        return "<unsupported>";
    }
}

std::string
Upgrades::toString() const
{
    fmt::MemoryWriter r;

    auto appendInfo = [&](std::string const& s, optional<uint32> const& o) {
        if (o)
        {
            if (!r.size())
            {
                r << "upgradetime="
                  << VirtualClock::pointToISOString(mParams.mUpgradeTime);
            }
            r << ", " << s << "=" << *o;
        }
    };
    appendInfo("protocolversion", mParams.mProtocolVersion);
    appendInfo("basefee", mParams.mBaseFee);
    appendInfo("basereserve", mParams.mBaseReserve);
    appendInfo("maxtxsize", mParams.mMaxTxSize);

    return r.str();
}

Upgrades::UpgradeParameters
Upgrades::removeUpgrades(std::vector<UpgradeType>::const_iterator beginUpdates,
                         std::vector<UpgradeType>::const_iterator endUpdates,
                         bool& updated)
{
    updated = false;
    UpgradeParameters res = mParams;

    auto resetParam = [&](optional<uint32>& o, uint32 v) {
        if (o && *o == v)
        {
            o.reset();
            updated = true;
        }
    };

    for (auto it = beginUpdates; it != endUpdates; it++)
    {
        auto& u = *it;
        LedgerUpgrade lu;
        try
        {
            xdr::xdr_from_opaque(u, lu);
        }
        catch (xdr::xdr_runtime_error&)
        {
            continue;
        }
        switch (lu.type())
        {
        case LEDGER_UPGRADE_VERSION:
            resetParam(res.mProtocolVersion, lu.newLedgerVersion());
            break;
        case LEDGER_UPGRADE_BASE_FEE:
            resetParam(res.mBaseFee, lu.newBaseFee());
            break;
        case LEDGER_UPGRADE_MAX_TX_SET_SIZE:
            resetParam(res.mMaxTxSize, lu.newMaxTxSetSize());
            break;
        case LEDGER_UPGRADE_BASE_RESERVE:
            resetParam(res.mBaseReserve, lu.newBaseReserve());
            break;
        default:
                        break;
        }
    }
    return res;
}

Upgrades::UpgradeValidity
Upgrades::isValidForApply(UpgradeType const& opaqueUpgrade,
                          LedgerUpgrade& upgrade, LedgerHeader const& header,
                          uint32_t maxLedgerVersion)
{
    try
    {
        xdr::xdr_from_opaque(opaqueUpgrade, upgrade);
    }
    catch (xdr::xdr_runtime_error&)
    {
        return UpgradeValidity::XDR_INVALID;
    }

    bool res = true;
    switch (upgrade.type())
    {
    case LEDGER_UPGRADE_VERSION:
    {
        uint32 newVersion = upgrade.newLedgerVersion();
                res = res && (newVersion <= maxLedgerVersion);
                res = res && (newVersion > header.ledgerVersion);
    }
    break;
    case LEDGER_UPGRADE_BASE_FEE:
        res = res && (upgrade.newBaseFee() != 0);
        break;
    case LEDGER_UPGRADE_MAX_TX_SET_SIZE:
        res = res && (upgrade.newMaxTxSetSize() != 0);
        break;
    case LEDGER_UPGRADE_BASE_RESERVE:
        res = res && (upgrade.newBaseReserve() != 0);
        break;
    default:
        res = false;
    }

    return res ? UpgradeValidity::VALID : UpgradeValidity::INVALID;
}

bool
Upgrades::isValidForNomination(LedgerUpgrade const& upgrade,
                               LedgerHeader const& header) const
{
    if (!timeForUpgrade(header.scpValue.closeTime))
    {
        return false;
    }

    switch (upgrade.type())
    {
    case LEDGER_UPGRADE_VERSION:
        return mParams.mProtocolVersion &&
               (upgrade.newLedgerVersion() == *mParams.mProtocolVersion);
    case LEDGER_UPGRADE_BASE_FEE:
        return mParams.mBaseFee && (upgrade.newBaseFee() == *mParams.mBaseFee);
    case LEDGER_UPGRADE_MAX_TX_SET_SIZE:
        return mParams.mMaxTxSize &&
               (upgrade.newMaxTxSetSize() == *mParams.mMaxTxSize);
    case LEDGER_UPGRADE_BASE_RESERVE:
        return mParams.mBaseReserve &&
               (upgrade.newBaseReserve() == *mParams.mBaseReserve);
    default:
        return false;
    }
}

bool
Upgrades::isValid(UpgradeType const& upgrade, LedgerUpgradeType& upgradeType,
                  bool nomination, Config const& cfg,
                  LedgerHeader const& header) const
{
    LedgerUpgrade lupgrade;
    bool res =
        isValidForApply(upgrade, lupgrade, header,
                        cfg.LEDGER_PROTOCOL_VERSION) == UpgradeValidity::VALID;

    if (nomination)
    {
        res = res && isValidForNomination(lupgrade, header);
    }

    if (res)
    {
        upgradeType = lupgrade.type();
    }
    return res;
}

bool
Upgrades::timeForUpgrade(uint64_t time) const
{
    return mParams.mUpgradeTime <= VirtualClock::from_time_t(time);
}

void
Upgrades::dropAll(Database& db)
{
    db.getSession() << "DROP TABLE IF EXISTS upgradehistory";
    db.getSession() << "CREATE TABLE upgradehistory ("
                       "ledgerseq    INT NOT NULL CHECK (ledgerseq >= 0), "
                       "upgradeindex INT NOT NULL, "
                       "upgrade      TEXT NOT NULL, "
                       "changes      TEXT NOT NULL, "
                       "PRIMARY KEY (ledgerseq, upgradeindex)"
                       ")";
    db.getSession()
        << "CREATE INDEX upgradehistbyseq ON upgradehistory (ledgerseq);";
}

void
Upgrades::storeUpgradeHistory(Database& db, uint32_t ledgerSeq,
                              LedgerUpgrade const& upgrade,
                              LedgerEntryChanges const& changes, int index)
{
    xdr::opaque_vec<> upgradeContent(xdr::xdr_to_opaque(upgrade));
    std::string upgradeContent64 = decoder::encode_b64(upgradeContent);

    xdr::opaque_vec<> upgradeChanges(xdr::xdr_to_opaque(changes));
    std::string upgradeChanges64 = decoder::encode_b64(upgradeChanges);

    auto prep = db.getPreparedStatement(
        "INSERT INTO upgradehistory "
        "(ledgerseq, upgradeindex,  upgrade,  changes) VALUES "
        "(:seq,      :upgradeindex, :upgrade, :changes)");

    auto& st = prep.statement();
    st.exchange(soci::use(ledgerSeq));
    st.exchange(soci::use(index));
    st.exchange(soci::use(upgradeContent64));
    st.exchange(soci::use(upgradeChanges64));
    st.define_and_bind();
    {
        auto timer = db.getInsertTimer("upgradehistory");
        st.execute(true);
    }

    if (st.get_affected_rows() != 1)
    {
        throw std::runtime_error("Could not update data in SQL");
    }
}

void
Upgrades::deleteOldEntries(Database& db, uint32_t ledgerSeq, uint32_t count)
{
    DatabaseUtils::deleteOldEntriesHelper(db.getSession(), ledgerSeq, count,
                                          "upgradehistory", "ledgerseq");
}

static void
addLiabilities(std::map<Asset, std::unique_ptr<int64_t>>& liabilities,
               AccountID const& accountID, Asset const& asset, int64_t delta)
{
    auto iter =
        liabilities.insert(std::make_pair(asset, std::make_unique<int64_t>(0)))
            .first;
    if (asset.type() != ASSET_TYPE_NATIVE && accountID == getIssuer(asset))
    {
        return;
    }
    if (iter->second)
    {
        if (!viichain::addBalance(*iter->second, delta))
        {
            iter->second.reset();
        }
    }
}

static int64_t
getAvailableBalanceExcludingLiabilities(AccountID const& accountID,
                                        Asset const& asset,
                                        int64_t balanceAboveReserve,
                                        AbstractLedgerTxn& ltx)
{
    if (asset.type() == ASSET_TYPE_NATIVE)
    {
        return balanceAboveReserve;
    }

    if (accountID == getIssuer(asset))
    {
        return INT64_MAX;
    }
    else
    {
        auto trust = viichain::loadTrustLineWithoutRecord(ltx, accountID, asset);
        if (trust && trust.isAuthorized())
        {
            return trust.getBalance();
        }
        else
        {
            return 0;
        }
    }
}

static int64_t
getAvailableLimitExcludingLiabilities(AccountID const& accountID,
                                      Asset const& asset, int64_t balance,
                                      AbstractLedgerTxn& ltx)
{
    if (asset.type() == ASSET_TYPE_NATIVE)
    {
        return INT64_MAX - balance;
    }

    if (accountID == getIssuer(asset))
    {
        return INT64_MAX;
    }
    else
    {
        LedgerKey key(TRUSTLINE);
        key.trustLine().accountID = accountID;
        key.trustLine().asset = asset;
        auto trust = ltx.loadWithoutRecord(key);
        if (trust && isAuthorized(trust))
        {
            auto const& tl = trust.current().data.trustLine();
            return tl.limit - tl.balance;
        }
        else
        {
            return 0;
        }
    }
}

static bool
shouldDeleteOffer(Asset const& asset, int64_t effectiveBalance,
                  std::map<Asset, std::unique_ptr<int64_t>> const& liabilities,
                  std::function<int64_t(Asset const&, int64_t)> getCap)
{
    auto iter = liabilities.find(asset);
    if (iter == liabilities.end())
    {
        throw std::runtime_error("liabilities were not calculated");
    }
            return iter->second ? *iter->second > getCap(asset, effectiveBalance)
                        : true;
}

enum class UpdateOfferResult
{
    Unchanged,
    Adjusted,
    AdjustedToZero,
    Erased
};

static UpdateOfferResult
updateOffer(
    LedgerTxnEntry& offerEntry, int64_t balance, int64_t balanceAboveReserve,
    std::map<Asset, Liabilities>& liabilities,
    std::map<Asset, std::unique_ptr<int64_t>> const& initialBuyingLiabilities,
    std::map<Asset, std::unique_ptr<int64_t>> const& initialSellingLiabilities,
    AbstractLedgerTxn& ltx, LedgerTxnHeader const& header)
{
    using namespace std::placeholders;
    auto& offer = offerEntry.current().data.offer();

    auto availableBalanceBind =
        std::bind(getAvailableBalanceExcludingLiabilities, offer.sellerID, _1,
                  _2, std::ref(ltx));
    auto availableLimitBind = std::bind(getAvailableLimitExcludingLiabilities,
                                        offer.sellerID, _1, _2, std::ref(ltx));

    bool erase =
        shouldDeleteOffer(offer.selling, balanceAboveReserve,
                          initialSellingLiabilities, availableBalanceBind);
    erase = erase ||
            shouldDeleteOffer(offer.buying, balance, initialBuyingLiabilities,
                              availableLimitBind);
    UpdateOfferResult res =
        erase ? UpdateOfferResult::Erased : UpdateOfferResult::Unchanged;

                                if (!erase && adjustOffer(offer.price, offer.amount, INT64_MAX) == 0)
    {
        erase = true;
        res = UpdateOfferResult::AdjustedToZero;
    }

    if (erase)
    {
        offerEntry.erase();
    }
    else
    {
                                auto adjAmount = adjustOffer(offer.price, offer.amount, INT64_MAX);
        if (adjAmount != offer.amount)
        {
            offer.amount = adjAmount;
            res = UpdateOfferResult::Adjusted;
        }

        if (offer.buying.type() == ASSET_TYPE_NATIVE ||
            !(offer.sellerID == getIssuer(offer.buying)))
        {
            if (!viichain::addBalance(
                    liabilities[offer.buying].buying,
                    getOfferBuyingLiabilities(header, offerEntry)))
            {
                throw std::runtime_error("could not add buying "
                                         "liabilities");
            }
        }
        if (offer.selling.type() == ASSET_TYPE_NATIVE ||
            !(offer.sellerID == getIssuer(offer.selling)))
        {
            if (!viichain::addBalance(
                    liabilities[offer.selling].selling,
                    getOfferSellingLiabilities(header, offerEntry)))
            {
                throw std::runtime_error("could not add selling "
                                         "liabilities");
            }
        }
    }
    return res;
}

static void
prepareLiabilities(AbstractLedgerTxn& ltx, LedgerTxnHeader const& header)
{
    CLOG(INFO, "Ledger") << "Starting prepareLiabilities";

    auto offersByAccount = ltx.loadAllOffers();

    uint64_t nChangedAccounts = 0;
    uint64_t nChangedTrustLines = 0;
    std::map<UpdateOfferResult, uint64_t> nUpdatedOffers;
    for (auto& accountOffers : offersByAccount)
    {
                                                std::map<Asset, std::unique_ptr<int64_t>> initialBuyingLiabilities;
        std::map<Asset, std::unique_ptr<int64_t>> initialSellingLiabilities;
        for (auto const& offerEntry : accountOffers.second)
        {
            auto const& offer = offerEntry.current().data.offer();
            addLiabilities(initialBuyingLiabilities, offer.sellerID,
                           offer.buying,
                           getOfferBuyingLiabilities(header, offerEntry));
            addLiabilities(initialSellingLiabilities, offer.sellerID,
                           offer.selling,
                           getOfferSellingLiabilities(header, offerEntry));
        }

        auto accountEntry = viichain::loadAccount(ltx, accountOffers.first);
        if (!accountEntry)
        {
            throw std::runtime_error("account does not exist");
        }
        auto const& acc = accountEntry.current().data.account();
        AccountEntry const accountBefore = acc;

                        int64_t balance = acc.balance;
        int64_t minBalance = getMinBalance(header, acc.numSubEntries);
        int64_t balanceAboveReserve = balance - minBalance;

        std::map<Asset, Liabilities> liabilities;
        for (auto& offerEntry : accountOffers.second)
        {
            auto offerID = offerEntry.current().data.offer().offerID;
            auto res = updateOffer(offerEntry, balance, balanceAboveReserve,
                                   liabilities, initialBuyingLiabilities,
                                   initialSellingLiabilities, ltx, header);
            if (res == UpdateOfferResult::AdjustedToZero ||
                res == UpdateOfferResult::Erased)
            {
                viichain::addNumEntries(header, accountEntry, -1);
            }

            ++nUpdatedOffers[res];
            if (res != UpdateOfferResult::Unchanged)
            {
                std::string message;
                switch (res)
                {
                case UpdateOfferResult::Adjusted:
                    message = " was adjusted";
                    break;
                case UpdateOfferResult::AdjustedToZero:
                    message = " was adjusted to zero";
                    break;
                case UpdateOfferResult::Erased:
                    message = " was erased";
                    break;
                default:
                    throw std::runtime_error("Unknown UpdateOfferResult");
                }
                CLOG(DEBUG, "Ledger")
                    << "Offer with offerID=" << offerID << message;
            }
        }

        for (auto const& assetLiabilities : liabilities)
        {
            Asset const& asset = assetLiabilities.first;
            Liabilities const& liab = assetLiabilities.second;
            if (asset.type() == ASSET_TYPE_NATIVE)
            {
                int64_t deltaSelling =
                    liab.selling - getSellingLiabilities(header, accountEntry);
                int64_t deltaBuying =
                    liab.buying - getBuyingLiabilities(header, accountEntry);
                if (!addSellingLiabilities(header, accountEntry, deltaSelling))
                {
                    throw std::runtime_error("invalid selling liabilities "
                                             "during upgrade");
                }
                if (!addBuyingLiabilities(header, accountEntry, deltaBuying))
                {
                    throw std::runtime_error("invalid buying liabilities "
                                             "during upgrade");
                }
            }
            else
            {
                auto trustEntry =
                    viichain::loadTrustLine(ltx, accountOffers.first, asset);
                int64_t deltaSelling =
                    liab.selling - trustEntry.getSellingLiabilities(header);
                int64_t deltaBuying =
                    liab.buying - trustEntry.getBuyingLiabilities(header);
                if (deltaSelling != 0 || deltaBuying != 0)
                {
                    ++nChangedTrustLines;
                }

                if (!trustEntry.addSellingLiabilities(header, deltaSelling))
                {
                    throw std::runtime_error("invalid selling liabilities "
                                             "during upgrade");
                }
                if (!trustEntry.addBuyingLiabilities(header, deltaBuying))
                {
                    throw std::runtime_error("invalid buying liabilities "
                                             "during upgrade");
                }
            }
        }

        if (!(acc == accountBefore))
        {
            ++nChangedAccounts;
        }
    }

    CLOG(INFO, "Ledger") << "prepareLiabilities completed with "
                         << nChangedAccounts << " accounts modified, "
                         << nChangedTrustLines << " trustlines modified, "
                         << nUpdatedOffers[UpdateOfferResult::Adjusted]
                         << " offers adjusted, "
                         << nUpdatedOffers[UpdateOfferResult::AdjustedToZero]
                         << " offers adjusted to zero, and "
                         << nUpdatedOffers[UpdateOfferResult::Erased]
                         << " offers erased";
}

void
Upgrades::applyVersionUpgrade(AbstractLedgerTxn& ltx, uint32_t newVersion)
{
    auto header = ltx.loadHeader();
    uint32_t prevVersion = header.current().ledgerVersion;

    header.current().ledgerVersion = newVersion;
    if (header.current().ledgerVersion >= 10 && prevVersion < 10)
    {
        prepareLiabilities(ltx, header);
    }
}

void
Upgrades::applyReserveUpgrade(AbstractLedgerTxn& ltx, uint32_t newReserve)
{
    auto header = ltx.loadHeader();
    bool didReserveIncrease = newReserve > header.current().baseReserve;

    header.current().baseReserve = newReserve;
    if (header.current().ledgerVersion >= 10 && didReserveIncrease)
    {
        prepareLiabilities(ltx, header);
    }
}
}

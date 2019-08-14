
#include "herder/HerderImpl.h"
#include "crypto/Hex.h"
#include "crypto/KeyUtils.h"
#include "crypto/SHA.h"
#include "herder/HerderPersistence.h"
#include "herder/HerderUtils.h"
#include "herder/LedgerCloseData.h"
#include "herder/QuorumIntersectionChecker.h"
#include "herder/TxSetFrame.h"
#include "ledger/LedgerManager.h"
#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnEntry.h"
#include "ledger/LedgerTxnHeader.h"
#include "lib/json/json.h"
#include "main/Application.h"
#include "main/Config.h"
#include "main/ErrorMessages.h"
#include "main/PersistentState.h"
#include "overlay/OverlayManager.h"
#include "scp/LocalNode.h"
#include "scp/Slot.h"
#include "transactions/TransactionUtils.h"
#include "util/Logging.h"
#include "util/StatusManager.h"
#include "util/Timer.h"

#include "medida/counter.h"
#include "medida/meter.h"
#include "medida/metrics_registry.h"
#include "util/Decoder.h"
#include "util/XDRStream.h"
#include "xdrpp/marshal.h"

#include <ctime>
#include <lib/util/format.h>

using namespace std;

namespace viichain
{

constexpr auto const TRANSACTION_QUEUE_SIZE = 4;
constexpr auto const TRANSACTION_QUEUE_BAN_SIZE = 10;

std::unique_ptr<Herder>
Herder::create(Application& app)
{
    return std::make_unique<HerderImpl>(app);
}

HerderImpl::SCPMetrics::SCPMetrics(Application& app)
    : mLostSync(app.getMetrics().NewMeter({"scp", "sync", "lost"}, "sync"))
    , mEnvelopeEmit(
          app.getMetrics().NewMeter({"scp", "envelope", "emit"}, "envelope"))
    , mEnvelopeReceive(
          app.getMetrics().NewMeter({"scp", "envelope", "receive"}, "envelope"))
    , mCumulativeStatements(app.getMetrics().NewCounter(
          {"scp", "memory", "cumulative-statements"}))
    , mEnvelopeValidSig(app.getMetrics().NewMeter(
          {"scp", "envelope", "validsig"}, "envelope"))
    , mEnvelopeInvalidSig(app.getMetrics().NewMeter(
          {"scp", "envelope", "invalidsig"}, "envelope"))
{
}

HerderImpl::HerderImpl(Application& app)
    : mTransactionQueue(app, TRANSACTION_QUEUE_SIZE, TRANSACTION_QUEUE_BAN_SIZE)
    , mPendingEnvelopes(app, *this)
    , mHerderSCPDriver(app, *this, mUpgrades, mPendingEnvelopes)
    , mLastSlotSaved(0)
    , mTrackingTimer(app)
    , mLastExternalize(app.getClock().now())
    , mTriggerTimer(app)
    , mRebroadcastTimer(app)
    , mApp(app)
    , mLedgerManager(app.getLedgerManager())
    , mSCPMetrics(app)
{
    Hash hash = getSCP().getLocalNode()->getQuorumSetHash();
    mPendingEnvelopes.addSCPQuorumSet(hash,
                                      getSCP().getLocalNode()->getQuorumSet());
}

HerderImpl::~HerderImpl()
{
}

Herder::State
HerderImpl::getState() const
{
    return mHerderSCPDriver.getState();
}

SCP&
HerderImpl::getSCP()
{
    return mHerderSCPDriver.getSCP();
}

void
HerderImpl::syncMetrics()
{
    mSCPMetrics.mCumulativeStatements.set_count(
        getSCP().getCumulativeStatemtCount());
}

std::string
HerderImpl::getStateHuman() const
{
    static const char* stateStrings[HERDER_NUM_STATE] = {
        "HERDER_SYNCING_STATE", "HERDER_TRACKING_STATE"};
    return std::string(stateStrings[getState()]);
}

void
HerderImpl::bootstrap()
{
    CLOG(INFO, "Herder") << "Force joining SCP with local state";
    assert(getSCP().isValidator());
    assert(mApp.getConfig().FORCE_SCP);

    mLedgerManager.bootstrap();
    mHerderSCPDriver.bootstrap();

    ledgerClosed();
}

void
HerderImpl::valueExternalized(uint64 slotIndex, VIIValue const& value)
{
    const int DUMP_SCP_TIMEOUT_SECONDS = 20;

        getHerderSCPDriver().recordSCPExecutionMetrics(slotIndex);

        auto now = mApp.getClock().now();
    auto gap =
        std::chrono::duration_cast<std::chrono::seconds>(now - mLastExternalize)
            .count();
    if (gap > DUMP_SCP_TIMEOUT_SECONDS)
    {
        auto slotInfo = getJsonQuorumInfo(getSCP().getLocalNodeID(), false,
                                          false, slotIndex);
        Json::FastWriter fw;
        CLOG(WARNING, "Herder")
            << fmt::format("Ledger took {} seconds, SCP information:{}", gap,
                           fw.write(slotInfo));
    }
    mLastExternalize = now;

        trackingHeartBeat();

    bool validated = getSCP().isSlotFullyValidated(slotIndex);

    if (Logging::logDebug("Herder"))
        CLOG(DEBUG, "Herder") << fmt::format(
            "HerderSCPDriver::valueExternalized index: {} txSet: {}", slotIndex,
            hexAbbrev(value.txSetHash));

    if (getSCP().isValidator() && !validated)
    {
        CLOG(WARNING, "Herder")
            << fmt::format("Ledger {} ({}) closed and could NOT be fully "
                           "validated by validator",
                           slotIndex, hexAbbrev(value.txSetHash));
    }

    TxSetFramePtr externalizedSet = mPendingEnvelopes.getTxSet(value.txSetHash);

                mTriggerTimer.cancel();

        mApp.getHerderPersistence().saveSCPHistory(
        static_cast<uint32>(slotIndex),
        getSCP().getExternalizingState(slotIndex),
        mPendingEnvelopes.getCurrentlyTrackedQuorum());

        {
        bool updated;
        auto newUpgrades = mUpgrades.removeUpgrades(
            value.upgrades.begin(), value.upgrades.end(), updated);
        if (updated)
        {
            setUpgrades(newUpgrades);
        }
    }

                LedgerCloseData ledgerData(mHerderSCPDriver.lastConsensusLedgerIndex(),
                               externalizedSet, value);
    mLedgerManager.valueExternalized(ledgerData);

        updateTransactionQueue(externalizedSet->mTransactions);

        if (slotIndex > MAX_SLOTS_TO_REMEMBER)
    {
        getSCP().purgeSlots(slotIndex - MAX_SLOTS_TO_REMEMBER);
    }

    ledgerClosed();

        checkAndMaybeReanalyzeQuorumMap();

            trackingHeartBeat();
}

void
HerderImpl::rebroadcast()
{
    auto const& lcl = mLedgerManager.getLastClosedLedgerHeader().header;
    for (auto const& e : getSCP().getLatestMessagesSend(lcl.ledgerSeq + 1))
    {
        broadcast(e);
    }

    if (!mHerderSCPDriver.trackingSCP())
    {
        getMoreSCPState();
    }

    startRebroadcastTimer();
}

void
HerderImpl::broadcast(SCPEnvelope const& e)
{
    if (!mApp.getConfig().MANUAL_CLOSE)
    {
        VIIMessage m;
        m.type(SCP_MESSAGE);
        m.envelope() = e;

        CLOG(DEBUG, "Herder") << "broadcast "
                              << " s:" << e.statement.pledges.type()
                              << " i:" << e.statement.slotIndex;

        mSCPMetrics.mEnvelopeEmit.Mark();
        mApp.getOverlayManager().broadcastMessage(m, true);
    }
}

void
HerderImpl::startRebroadcastTimer()
{
    mRebroadcastTimer.expires_from_now(std::chrono::seconds(2));

    mRebroadcastTimer.async_wait(std::bind(&HerderImpl::rebroadcast, this),
                                 &VirtualTimer::onFailureNoop);
}

void
HerderImpl::emitEnvelope(SCPEnvelope const& envelope)
{
    uint64 slotIndex = envelope.statement.slotIndex;

    if (Logging::logDebug("Herder"))
        CLOG(DEBUG, "Herder")
            << "emitEnvelope"
            << " s:" << envelope.statement.pledges.type() << " i:" << slotIndex
            << " a:" << mApp.getStateHuman();

    persistSCPState(slotIndex);

    broadcast(envelope);

        startRebroadcastTimer();
}

TransactionQueue::AddResult
HerderImpl::recvTransaction(TransactionFramePtr tx)
{
    auto result = mTransactionQueue.tryAdd(tx);
    if (result == TransactionQueue::AddResult::ADD_STATUS_PENDING)
    {
        if (Logging::logTrace("Herder"))
            CLOG(TRACE, "Herder")
                << "recv transaction " << hexAbbrev(tx->getFullHash())
                << " for " << KeyUtils::toShortString(tx->getSourceID());
    }
    return result;
}

bool
HerderImpl::checkCloseTime(SCPEnvelope const& envelope, bool enforceRecent)
{
    using std::placeholders::_1;
    auto const& st = envelope.statement;

    uint64_t ctCutoff = 0;

    if (enforceRecent)
    {
        ctCutoff = VirtualClock::to_time_t(mApp.getClock().now());
        if (ctCutoff >= mApp.getConfig().MAXIMUM_LEDGER_CLOSETIME_DRIFT)
        {
            ctCutoff -= mApp.getConfig().MAXIMUM_LEDGER_CLOSETIME_DRIFT;
        }
    }

    auto envLedgerIndex = envelope.statement.slotIndex;
    auto& scpD = getHerderSCPDriver();

    auto const& lcl = mLedgerManager.getLastClosedLedgerHeader().header;
    auto lastCloseIndex = lcl.ledgerSeq;
    auto lastCloseTime = lcl.scpValue.closeTime;

                    auto adjustLastCloseTime = [&](HerderSCPDriver::ConsensusData const* cd) {
        if (cd && envLedgerIndex >= cd->mConsensusIndex &&
            cd->mConsensusIndex > lastCloseIndex)
        {
            lastCloseIndex = static_cast<uint32>(cd->mConsensusIndex);
            lastCloseTime = cd->mConsensusValue.closeTime;
        }
    };
    adjustLastCloseTime(mHerderSCPDriver.trackingSCP());
    adjustLastCloseTime(mHerderSCPDriver.lastTrackingSCP());

    VIIValue sv;
            auto checkCTHelper = [&](std::vector<Value> const& values) {
        return std::any_of(values.begin(), values.end(), [&](Value const& e) {
            auto r = scpD.toVIIValue(e, sv);
                        r = r && sv.closeTime >= ctCutoff;
            if (r)
            {
                                                r = (lastCloseIndex == envLedgerIndex &&
                     lastCloseTime == sv.closeTime);
                                r = r || (lastCloseIndex > envLedgerIndex &&
                          lastCloseTime > sv.closeTime);
                                                r = r || scpD.checkCloseTime(envLedgerIndex, lastCloseTime, sv);
            }
            return r;
        });
    };

    bool b;

    switch (st.pledges.type())
    {
    case SCP_ST_NOMINATE:
        b = checkCTHelper(st.pledges.nominate().accepted) ||
            checkCTHelper(st.pledges.nominate().votes);
        break;
    case SCP_ST_PREPARE:
    {
        auto& prep = st.pledges.prepare();
        b = checkCTHelper({prep.ballot.value});
        if (!b && prep.prepared)
        {
            b = checkCTHelper({prep.prepared->value});
        }
        if (!b && prep.preparedPrime)
        {
            b = checkCTHelper({prep.preparedPrime->value});
        }
    }
    break;
    case SCP_ST_CONFIRM:
        b = checkCTHelper({st.pledges.confirm().ballot.value});
        break;
    case SCP_ST_EXTERNALIZE:
        b = checkCTHelper({st.pledges.externalize().commit.value});
        break;
    default:
        abort();
    }

    if (!b && Logging::logTrace("Herder"))
    {
        CLOG(TRACE, "Herder")
            << "Invalid close time processing " << getSCP().envToStr(st);
    }
    return b;
}

Herder::EnvelopeStatus
HerderImpl::recvSCPEnvelope(SCPEnvelope const& envelope)
{
    if (mApp.getConfig().MANUAL_CLOSE)
    {
        return Herder::ENVELOPE_STATUS_DISCARDED;
    }

    if (!verifyEnvelope(envelope))
    {
        CLOG(DEBUG, "Herder") << "Received bad envelope, discarding";
        return Herder::ENVELOPE_STATUS_DISCARDED;
    }

    if (Logging::logDebug("Herder"))
        CLOG(DEBUG, "Herder")
            << "recvSCPEnvelope"
            << " from: "
            << mApp.getConfig().toShortString(envelope.statement.nodeID)
            << " s:" << envelope.statement.pledges.type()
            << " i:" << envelope.statement.slotIndex
            << " a:" << mApp.getStateHuman();

    mSCPMetrics.mEnvelopeReceive.Mark();

    uint32_t minLedgerSeq = getCurrentLedgerSeq();
    if (minLedgerSeq > MAX_SLOTS_TO_REMEMBER)
    {
        minLedgerSeq -= MAX_SLOTS_TO_REMEMBER;
    }

    uint32_t maxLedgerSeq = std::numeric_limits<uint32>::max();

    if (!checkCloseTime(envelope, false))
    {
                                CLOG(DEBUG, "Herder")
            << "skipping invalid close time (incompatible with current state)";
        return Herder::ENVELOPE_STATUS_DISCARDED;
    }

    if (mHerderSCPDriver.trackingSCP())
    {
                
                                        maxLedgerSeq = mHerderSCPDriver.nextConsensusLedgerIndex() +
                       LEDGER_VALIDITY_BRACKET;
    }
    else if (!checkCloseTime(envelope,
                             minLedgerSeq <= LedgerManager::GENESIS_LEDGER_SEQ))
    {
                                CLOG(DEBUG, "Herder") << "recvSCPEnvelope: skipping invalid close time "
                                 "(check MAXIMUM_LEDGER_CLOSETIME_DRIFT)";
        return Herder::ENVELOPE_STATUS_DISCARDED;
    }

        if (envelope.statement.slotIndex > maxLedgerSeq ||
        envelope.statement.slotIndex < minLedgerSeq)
    {
        CLOG(DEBUG, "Herder") << "Ignoring SCPEnvelope outside of range: "
                              << envelope.statement.slotIndex << "( "
                              << minLedgerSeq << "," << maxLedgerSeq << ")";
        return Herder::ENVELOPE_STATUS_DISCARDED;
    }

    if (envelope.statement.nodeID == getSCP().getLocalNode()->getNodeID())
    {
        CLOG(DEBUG, "Herder") << "recvSCPEnvelope: skipping own message";
        return Herder::ENVELOPE_STATUS_SKIPPED_SELF;
    }

    auto status = mPendingEnvelopes.recvSCPEnvelope(envelope);
    if (status == Herder::ENVELOPE_STATUS_READY)
    {
        processSCPQueue();
    }
    return status;
}

Herder::EnvelopeStatus
HerderImpl::recvSCPEnvelope(SCPEnvelope const& envelope,
                            const SCPQuorumSet& qset, TxSetFrame txset)
{
    mPendingEnvelopes.addTxSet(txset.getContentsHash(),
                               envelope.statement.slotIndex,
                               std::make_shared<TxSetFrame>(txset));
    mPendingEnvelopes.addSCPQuorumSet(sha256(xdr::xdr_to_opaque(qset)), qset);
    return recvSCPEnvelope(envelope);
}

void
HerderImpl::sendSCPStateToPeer(uint32 ledgerSeq, Peer::pointer peer)
{
    if (getSCP().empty())
    {
        return;
    }

    if (getSCP().getLowSlotIndex() > std::numeric_limits<uint32_t>::max() ||
        getSCP().getHighSlotIndex() >= std::numeric_limits<uint32_t>::max())
    {
        return;
    }

    auto minSeq =
        std::max(ledgerSeq, static_cast<uint32_t>(getSCP().getLowSlotIndex()));
    auto maxSeq = static_cast<uint32_t>(getSCP().getHighSlotIndex());

    for (uint32_t seq = minSeq; seq <= maxSeq; seq++)
    {
        auto const& envelopes = getSCP().getCurrentState(seq);

        if (envelopes.size() != 0)
        {
            CLOG(DEBUG, "Herder")
                << "Send state " << envelopes.size() << " for ledger " << seq;

            for (auto const& e : envelopes)
            {
                VIIMessage m;
                m.type(SCP_MESSAGE);
                m.envelope() = e;
                peer->sendMessage(m);
            }
        }
    }
}

void
HerderImpl::processSCPQueue()
{
    if (mHerderSCPDriver.trackingSCP())
    {
                if (mHerderSCPDriver.nextConsensusLedgerIndex() > MAX_SLOTS_TO_REMEMBER)
        {
            mPendingEnvelopes.eraseBelow(
                mHerderSCPDriver.nextConsensusLedgerIndex() -
                MAX_SLOTS_TO_REMEMBER);
        }

        processSCPQueueUpToIndex(mHerderSCPDriver.nextConsensusLedgerIndex());
    }
    else
    {
                                for (auto& slot : mPendingEnvelopes.readySlots())
        {
            processSCPQueueUpToIndex(slot);
            if (mHerderSCPDriver.trackingSCP())
            {
                                                break;
            }
        }
    }
}

void
HerderImpl::processSCPQueueUpToIndex(uint64 slotIndex)
{
    while (true)
    {
        SCPEnvelope env;
        if (mPendingEnvelopes.pop(slotIndex, env))
        {
            auto r = getSCP().receiveEnvelope(env);
            if (r == SCP::EnvelopeState::VALID)
            {
                mPendingEnvelopes.envelopeProcessed(env);
            }
        }
        else
        {
            return;
        }
    }
}

#ifdef BUILD_TESTS
PendingEnvelopes&
HerderImpl::getPendingEnvelopes()
{
    return mPendingEnvelopes;
}
#endif

void
HerderImpl::ledgerClosed()
{
    mTriggerTimer.cancel();

    CLOG(TRACE, "Herder") << "HerderImpl::ledgerClosed";

    auto lastIndex = mHerderSCPDriver.lastConsensusLedgerIndex();

    mPendingEnvelopes.slotClosed(lastIndex);

    mApp.getOverlayManager().ledgerClosed(lastIndex);

    uint64_t nextIndex = mHerderSCPDriver.nextConsensusLedgerIndex();

        processSCPQueueUpToIndex(nextIndex);

            if (nextIndex != mHerderSCPDriver.nextConsensusLedgerIndex())
    {
        return;
    }

    if (!mLedgerManager.isSynced())
    {
        CLOG(DEBUG, "Herder")
            << "Not presently synced, not triggering ledger-close.";
        return;
    }

    auto seconds = mApp.getConfig().getExpectedLedgerCloseTime();

            auto lastBallotStart = mApp.getClock().now() - seconds;
    auto lastStart = mHerderSCPDriver.getPrepareStart(lastIndex);
    if (lastStart)
    {
        lastBallotStart = *lastStart;
    }
                mTriggerTimer.expires_at(lastBallotStart + seconds);

    if (!mApp.getConfig().MANUAL_CLOSE)
        mTriggerTimer.async_wait(std::bind(&HerderImpl::triggerNextLedger, this,
                                           static_cast<uint32_t>(nextIndex)),
                                 &VirtualTimer::onFailureNoop);
}

bool
HerderImpl::recvSCPQuorumSet(Hash const& hash, const SCPQuorumSet& qset)
{
    return mPendingEnvelopes.recvSCPQuorumSet(hash, qset);
}

bool
HerderImpl::recvTxSet(Hash const& hash, const TxSetFrame& t)
{
    auto txset = std::make_shared<TxSetFrame>(t);
    return mPendingEnvelopes.recvTxSet(hash, txset);
}

void
HerderImpl::peerDoesntHave(MessageType type, uint256 const& itemID,
                           Peer::pointer peer)
{
    mPendingEnvelopes.peerDoesntHave(type, itemID, peer);
}

TxSetFramePtr
HerderImpl::getTxSet(Hash const& hash)
{
    return mPendingEnvelopes.getTxSet(hash);
}

SCPQuorumSetPtr
HerderImpl::getQSet(Hash const& qSetHash)
{
    return mHerderSCPDriver.getQSet(qSetHash);
}

uint32_t
HerderImpl::getCurrentLedgerSeq() const
{
    uint32_t res = mLedgerManager.getLastClosedLedgerNum();

    if (mHerderSCPDriver.trackingSCP() &&
        res < mHerderSCPDriver.trackingSCP()->mConsensusIndex)
    {
        res = static_cast<uint32_t>(
            mHerderSCPDriver.trackingSCP()->mConsensusIndex);
    }
    if (mHerderSCPDriver.lastTrackingSCP() &&
        res < mHerderSCPDriver.lastTrackingSCP()->mConsensusIndex)
    {
        res = static_cast<uint32_t>(
            mHerderSCPDriver.lastTrackingSCP()->mConsensusIndex);
    }
    return res;
}

SequenceNumber
HerderImpl::getMaxSeqInPendingTxs(AccountID const& acc)
{
    return mTransactionQueue.getAccountTransactionQueueInfo(acc).mMaxSeq;
}

void
HerderImpl::triggerNextLedger(uint32_t ledgerSeqToTrigger)
{
    if (!mHerderSCPDriver.trackingSCP() || !mLedgerManager.isSynced())
    {
        CLOG(DEBUG, "Herder") << "triggerNextLedger: skipping (out of sync) : "
                              << mApp.getStateHuman();
        return;
    }

            auto const& lcl = mLedgerManager.getLastClosedLedgerHeader();
    auto proposedSet = mTransactionQueue.toTxSet(lcl.hash);
    auto removed = proposedSet->trimInvalid(mApp);
    mTransactionQueue.remove(removed);

    proposedSet->surgePricingFilter(mApp);

    if (!proposedSet->checkValid(mApp))
    {
        throw std::runtime_error("wanting to emit an invalid txSet");
    }

    auto txSetHash = proposedSet->getContentsHash();

            uint32_t slotIndex = lcl.header.ledgerSeq + 1;

                mPendingEnvelopes.addTxSet(txSetHash, slotIndex, proposedSet);

            if (ledgerSeqToTrigger != slotIndex)
    {
        return;
    }

                uint64_t nextCloseTime = VirtualClock::to_time_t(mApp.getClock().now());
    if (nextCloseTime <= lcl.header.scpValue.closeTime)
    {
        nextCloseTime = lcl.header.scpValue.closeTime + 1;
    }

    VIIValue newProposedValue(txSetHash, nextCloseTime, emptyUpgradeSteps,
                                  VII_VALUE_BASIC);

        auto upgrades = mUpgrades.createUpgradesFor(lcl.header);
    for (auto const& upgrade : upgrades)
    {
        Value v(xdr::xdr_to_opaque(upgrade));
        if (v.size() >= UpgradeType::max_size())
        {
            CLOG(ERROR, "Herder")
                << "HerderImpl::triggerNextLedger"
                << " exceeded size for upgrade step (got " << v.size()
                << " ) for upgrade type " << std::to_string(upgrade.type());
            CLOG(ERROR, "Herder") << REPORT_INTERNAL_BUG;
        }
        else
        {
            newProposedValue.upgrades.emplace_back(v.begin(), v.end());
        }
    }

    getHerderSCPDriver().recordSCPEvent(slotIndex, true);

        if (!getSCP().isValidator())
    {
        CLOG(DEBUG, "Herder")
            << "Non-validating node, skipping nomination (SCP).";
        return;
    }

    if (lcl.header.ledgerVersion >= 11)
    {
                signVIIValue(mApp.getConfig().NODE_SEED, newProposedValue);
    }
    mHerderSCPDriver.nominate(slotIndex, newProposedValue, proposedSet,
                              lcl.header.scpValue);
}

void
HerderImpl::setUpgrades(Upgrades::UpgradeParameters const& upgrades)
{
    mUpgrades.setParameters(upgrades, mApp.getConfig());
    persistUpgrades();

    auto desc = mUpgrades.toString();

    if (!desc.empty())
    {
        auto message = fmt::format("Armed with network upgrades: {}", desc);
        auto prev = mApp.getStatusManager().getStatusMessage(
            StatusCategory::REQUIRES_UPGRADES);
        if (prev != message)
        {
            CLOG(INFO, "Herder") << message;
            mApp.getStatusManager().setStatusMessage(
                StatusCategory::REQUIRES_UPGRADES, message);
        }
    }
    else
    {
        CLOG(INFO, "Herder") << "Network upgrades cleared";
        mApp.getStatusManager().removeStatusMessage(
            StatusCategory::REQUIRES_UPGRADES);
    }
}

std::string
HerderImpl::getUpgradesJson()
{
    return mUpgrades.getParameters().toJson();
}

bool
HerderImpl::resolveNodeID(std::string const& s, PublicKey& retKey)
{
    bool r = mApp.getConfig().resolveNodeID(s, retKey);
    if (!r)
    {
        if (s.size() > 1 && s[0] == '@')
        {
            std::string arg = s.substr(1);
                                    uint32 seq = getCurrentLedgerSeq();
            if (seq > 2)
            {
                seq--;
            }
            auto const& envelopes = getSCP().getCurrentState(seq);
            for (auto const& e : envelopes)
            {
                std::string curK = KeyUtils::toStrKey(e.statement.nodeID);
                if (curK.compare(0, arg.size(), arg) == 0)
                {
                    retKey = e.statement.nodeID;
                    r = true;
                    break;
                }
            }
        }
    }
    return r;
}

Json::Value
HerderImpl::getJsonInfo(size_t limit, bool fullKeys)
{
    Json::Value ret;
    ret["you"] =
        mApp.getConfig().toStrKey(mApp.getConfig().NODE_SEED.getPublicKey());

    ret["scp"] = getSCP().getJsonInfo(limit, fullKeys);
    ret["queue"] = mPendingEnvelopes.getJsonInfo(limit);
    return ret;
}

Json::Value
HerderImpl::getJsonTransitiveQuorumIntersectionInfo(bool fullKeys) const
{
    Json::Value ret;
    ret["intersection"] =
        mLastQuorumMapIntersectionState.enjoysQuorunIntersection();
    ret["node_count"] =
        static_cast<Json::UInt64>(mLastQuorumMapIntersectionState.mNumNodes);
    ret["last_check_ledger"] = static_cast<Json::UInt64>(
        mLastQuorumMapIntersectionState.mLastCheckLedger);
    if (!mLastQuorumMapIntersectionState.enjoysQuorunIntersection())
    {
        ret["last_good_ledger"] = static_cast<Json::UInt64>(
            mLastQuorumMapIntersectionState.mLastGoodLedger);
        Json::Value split, a, b;
        auto const& pair = mLastQuorumMapIntersectionState.mPotentialSplit;
        for (auto const& k : pair.first)
        {
            auto s = (fullKeys ? mApp.getConfig().toStrKey(k)
                               : mApp.getConfig().toShortString(k));
            a.append(s);
        }
        for (auto const& k : pair.second)
        {
            auto s = (fullKeys ? mApp.getConfig().toStrKey(k)
                               : mApp.getConfig().toShortString(k));
            b.append(s);
        }
        split.append(a);
        split.append(b);
        ret["potential_split"] = split;
    }
    return ret;
}

Json::Value
HerderImpl::getJsonQuorumInfo(NodeID const& id, bool summary, bool fullKeys,
                              uint64 index)
{
    Json::Value ret;
    ret["node"] = (fullKeys ? mApp.getConfig().toStrKey(id)
                            : mApp.getConfig().toShortString(id));
    ret["qset"] = getSCP().getJsonQuorumInfo(id, summary, fullKeys, index);
    bool isSelf = id == mApp.getConfig().NODE_SEED.getPublicKey();
    if (isSelf && mLastQuorumMapIntersectionState.hasAnyResults())
    {
        ret["transitive"] = getJsonTransitiveQuorumIntersectionInfo(fullKeys);
    }
    return ret;
}

Json::Value
HerderImpl::getJsonTransitiveQuorumInfo(NodeID const& rootID, bool summary,
                                        bool fullKeys)
{
    Json::Value ret;
    bool isSelf = rootID == mApp.getConfig().NODE_SEED.getPublicKey();
    if (isSelf && mLastQuorumMapIntersectionState.hasAnyResults())
    {
        ret = getJsonTransitiveQuorumIntersectionInfo(fullKeys);
    }

    Json::Value& nodes = ret["nodes"];

    auto& q = mPendingEnvelopes.getCurrentlyTrackedQuorum();

    auto rootLatest = getSCP().getLatestMessage(rootID);
    std::map<Value, int> knownValues;

        std::unordered_set<NodeID> visited;
    std::vector<NodeID> next;
    next.push_back(rootID);
    visited.emplace(rootID);
    int distance = 0;
    int valGenID = 0;
    while (!next.empty())
    {
        std::vector<NodeID> frontier(std::move(next));
        next.clear();
        std::sort(frontier.begin(), frontier.end());
        for (auto const& id : frontier)
        {
            Json::Value cur;
            valGenID++;
            cur["node"] = fullKeys ? mApp.getConfig().toStrKey(id)
                                   : mApp.getConfig().toShortString(id);
            if (!summary)
            {
                cur["distance"] = distance;
            }
            auto it = q.find(id);
            std::string status;
            if (it != q.end())
            {
                auto qSet = it->second;
                if (qSet)
                {
                    if (!summary)
                    {
                        cur["qset"] =
                            getSCP().getLocalNode()->toJson(*qSet, fullKeys);
                    }
                    LocalNode::forAllNodes(*qSet, [&](NodeID const& n) {
                        auto b = visited.emplace(n);
                        if (b.second)
                        {
                            next.emplace_back(n);
                        }
                    });
                }
                auto latest = getSCP().getLatestMessage(id);
                if (latest)
                {
                    auto vals = Slot::getStatementValues(latest->statement);
                                                            int trackingValID = -1;
                    Value const* trackingValue = nullptr;
                    for (auto const& v : vals)
                    {
                        auto p =
                            knownValues.insert(std::make_pair(v, valGenID));
                        if (p.first->second > trackingValID)
                        {
                            trackingValID = p.first->second;
                            trackingValue = &v;
                        }
                    }

                    cur["heard"] =
                        static_cast<Json::UInt64>(latest->statement.slotIndex);
                    if (!summary)
                    {
                        cur["value"] = trackingValue
                                           ? mHerderSCPDriver.getValueString(
                                                 *trackingValue)
                                           : "";
                        cur["value_id"] = trackingValID;
                    }
                                        if (rootLatest)
                    {
                        if (latest->statement.slotIndex <
                            rootLatest->statement.slotIndex)
                        {
                            status = "behind";
                        }
                        else if (latest->statement.slotIndex >
                                 rootLatest->statement.slotIndex)
                        {
                            status = "ahead";
                        }
                        else
                        {
                            status = "tracking";
                        }
                    }
                }
                else
                {
                    status = "missing";
                }
            }
            else
            {
                status = "unknown";
            }
            cur["status"] = status;
            nodes.append(cur);
        }
        distance++;
    }
    return ret;
}

QuorumTracker::QuorumMap const&
HerderImpl::getCurrentlyTrackedQuorum() const
{
    return mPendingEnvelopes.getCurrentlyTrackedQuorum();
}

static Hash
getQmapHash(QuorumTracker::QuorumMap const& qmap)
{
    std::unique_ptr<SHA256> hasher = SHA256::create();
    for (auto const& pair : qmap)
    {
        hasher->add(xdr::xdr_to_opaque(pair.first));
        if (pair.second)
        {
            hasher->add(xdr::xdr_to_opaque(*pair.second));
        }
        else
        {
            hasher->add("\0");
        }
    }
    return hasher->finish();
}

void
HerderImpl::checkAndMaybeReanalyzeQuorumMap()
{
    if (!mApp.getConfig().QUORUM_INTERSECTION_CHECKER)
    {
        return;
    }
    if (!mLastQuorumMapIntersectionState.mRecalculating)
    {
        QuorumTracker::QuorumMap const& qmap = getCurrentlyTrackedQuorum();
        Hash curr = getQmapHash(qmap);
        if (mLastQuorumMapIntersectionState.mLastCheckQuorumMapHash != curr)
        {
            CLOG(INFO, "Herder")
                << "Transitive closure of quorum has changed, re-analyzing.";
            mLastQuorumMapIntersectionState.mRecalculating = true;
            auto& cfg = mApp.getConfig();
            auto ledger = getCurrentLedgerSeq();
            auto nNodes = qmap.size();
            auto qic = QuorumIntersectionChecker::create(qmap, cfg);
            auto& hState = mLastQuorumMapIntersectionState;
            auto& app = mApp;
            auto worker = [curr, ledger, nNodes, qic, &app, &hState] {
                bool ok = qic->networkEnjoysQuorumIntersection();
                auto split = qic->getPotentialSplit();
                app.postOnMainThread(
                    [ok, curr, ledger, nNodes, split, &hState] {
                        hState.mRecalculating = false;
                        hState.mNumNodes = nNodes;
                        hState.mLastCheckLedger = ledger;
                        hState.mLastCheckQuorumMapHash = curr;
                        hState.mPotentialSplit = split;
                        if (ok)
                        {
                            hState.mLastGoodLedger = ledger;
                        }
                    },
                    "QuorumIntersectionChecker");
            };
            mApp.postOnBackgroundThread(worker, "QuorumIntersectionChecker");
        }
    }
}

void
HerderImpl::persistSCPState(uint64 slot)
{
    if (slot < mLastSlotSaved)
    {
        return;
    }

    mLastSlotSaved = slot;

        xdr::xvector<SCPEnvelope> latestEnvs;
    std::map<Hash, TxSetFramePtr> txSets;
    std::map<Hash, SCPQuorumSetPtr> quorumSets;

    for (auto const& e : getSCP().getLatestMessagesSend(slot))
    {
        latestEnvs.emplace_back(e);

                for (auto const& h : getTxSetHashes(e))
        {
            auto txSet = mPendingEnvelopes.getTxSet(h);
            if (txSet)
            {
                txSets.insert(std::make_pair(h, txSet));
            }
        }
        Hash qsHash = Slot::getCompanionQuorumSetHashFromStatement(e.statement);
        SCPQuorumSetPtr qSet = mPendingEnvelopes.getQSet(qsHash);
        if (qSet)
        {
            quorumSets.insert(std::make_pair(qsHash, qSet));
        }
    }

    xdr::xvector<TransactionSet> latestTxSets;
    for (auto it : txSets)
    {
        latestTxSets.emplace_back();
        it.second->toXDR(latestTxSets.back());
    }

    xdr::xvector<SCPQuorumSet> latestQSets;
    for (auto it : quorumSets)
    {
        latestQSets.emplace_back(*it.second);
    }

    auto latestSCPData =
        xdr::xdr_to_opaque(latestEnvs, latestTxSets, latestQSets);
    std::string scpState;
    scpState = decoder::encode_b64(latestSCPData);

    mApp.getPersistentState().setSCPStateForSlot(slot, scpState);
}

void
HerderImpl::restoreSCPState()
{
        auto const& lcl = mLedgerManager.getLastClosedLedgerHeader();
    if (!mApp.getConfig().FORCE_SCP &&
        lcl.header.ledgerSeq == LedgerManager::GENESIS_LEDGER_SEQ)
    {
                        return;
    }

    mHerderSCPDriver.restoreSCPState(lcl.header.ledgerSeq, lcl.header.scpValue);

    trackingHeartBeat();

        auto latest64 = mApp.getPersistentState().getSCPStateAllSlots();
    for (auto const& state : latest64)
    {
        std::vector<uint8_t> buffer;
        decoder::decode_b64(state, buffer);

        xdr::xvector<SCPEnvelope> latestEnvs;
        xdr::xvector<TransactionSet> latestTxSets;
        xdr::xvector<SCPQuorumSet> latestQSets;

        try
        {
            xdr::xdr_from_opaque(buffer, latestEnvs, latestTxSets, latestQSets);

            for (auto const& txset : latestTxSets)
            {
                TxSetFramePtr cur =
                    make_shared<TxSetFrame>(mApp.getNetworkID(), txset);
                Hash h = cur->getContentsHash();
                mPendingEnvelopes.addTxSet(h, 0, cur);
            }
            for (auto const& qset : latestQSets)
            {
                Hash hash = sha256(xdr::xdr_to_opaque(qset));
                mPendingEnvelopes.addSCPQuorumSet(hash, qset);
            }
            for (auto const& e : latestEnvs)
            {
                getSCP().setStateFromEnvelope(e.statement.slotIndex, e);
                mLastSlotSaved =
                    std::max<uint64>(mLastSlotSaved, e.statement.slotIndex);
            }
        }
        catch (std::exception& e)
        {
                                                CLOG(INFO, "Herder") << "Error while restoring old scp messages, "
                                    "proceeding without them : "
                                 << e.what();
        }
        mPendingEnvelopes.rebuildQuorumTrackerState();
    }

    startRebroadcastTimer();
}

void
HerderImpl::persistUpgrades()
{
    auto s = mUpgrades.getParameters().toJson();
    mApp.getPersistentState().setState(PersistentState::kLedgerUpgrades, s);
}

void
HerderImpl::restoreUpgrades()
{
    std::string s =
        mApp.getPersistentState().getState(PersistentState::kLedgerUpgrades);
    if (!s.empty())
    {
        Upgrades::UpgradeParameters p;
        p.fromJson(s);
        try
        {
                        setUpgrades(p);
        }
        catch (std::exception e)
        {
            CLOG(INFO, "Herder") << "Error restoring upgrades '" << e.what()
                                 << "' with upgrades '" << s << "'";
        }
    }
}

void
HerderImpl::restoreState()
{
    restoreSCPState();
    restoreUpgrades();
}

void
HerderImpl::trackingHeartBeat()
{
    if (mApp.getConfig().MANUAL_CLOSE)
    {
        return;
    }

    assert(mHerderSCPDriver.trackingSCP());
    mTrackingTimer.expires_from_now(
        std::chrono::seconds(CONSENSUS_STUCK_TIMEOUT_SECONDS));
    mTrackingTimer.async_wait(std::bind(&HerderImpl::herderOutOfSync, this),
                              &VirtualTimer::onFailureNoop);
}

void
HerderImpl::updateTransactionQueue(
    std::vector<TransactionFramePtr> const& applied)
{
        mTransactionQueue.remove(applied);
    mTransactionQueue.shift();

            {
        auto toBroadcast = mTransactionQueue.toTxSet({});
        for (auto tx : toBroadcast->sortForApply())
        {
            auto msg = tx->toVIIMessage();
            mApp.getOverlayManager().broadcastMessage(msg);
        }
    }
}

void
HerderImpl::herderOutOfSync()
{
    CLOG(WARNING, "Herder") << "Lost track of consensus";

    auto s = getJsonInfo(20).toStyledString();
    CLOG(WARNING, "Herder") << "Out of sync context: " << s;

    mSCPMetrics.mLostSync.Mark();
    mHerderSCPDriver.lostSync();

    getMoreSCPState();

    processSCPQueue();
}

void
HerderImpl::getMoreSCPState()
{
    int const NB_PEERS_TO_ASK = 2;
    auto low = mApp.getLedgerManager().getLastClosedLedgerNum() + 1;
    if (low > Herder::MAX_SLOTS_TO_REMEMBER)
    {
        low -= Herder::MAX_SLOTS_TO_REMEMBER;
    }
    else
    {
        low = 1;
    }

        auto r = mApp.getOverlayManager().getRandomAuthenticatedPeers();
    for (int i = 0; i < NB_PEERS_TO_ASK && i < r.size(); i++)
    {
        r[i]->sendGetScpState(low);
    }
}

bool
HerderImpl::verifyEnvelope(SCPEnvelope const& envelope)
{
    auto b = PubKeyUtils::verifySig(
        envelope.statement.nodeID, envelope.signature,
        xdr::xdr_to_opaque(mApp.getNetworkID(), ENVELOPE_TYPE_SCP,
                           envelope.statement));
    if (b)
    {
        mSCPMetrics.mEnvelopeValidSig.Mark();
    }
    else
    {
        mSCPMetrics.mEnvelopeInvalidSig.Mark();
    }

    return b;
}
void
HerderImpl::signEnvelope(SecretKey const& s, SCPEnvelope& envelope)
{
    envelope.signature = s.sign(xdr::xdr_to_opaque(
        mApp.getNetworkID(), ENVELOPE_TYPE_SCP, envelope.statement));
}
bool
HerderImpl::verifyVIIValueSignature(VIIValue const& sv)
{
    auto b = PubKeyUtils::verifySig(
        sv.ext.lcValueSignature().nodeID, sv.ext.lcValueSignature().signature,
        xdr::xdr_to_opaque(mApp.getNetworkID(), ENVELOPE_TYPE_SCPVALUE,
                           sv.txSetHash, sv.closeTime));
    return b;
}

void
HerderImpl::signVIIValue(SecretKey const& s, VIIValue& sv)
{
    sv.ext.v(VII_VALUE_SIGNED);
    sv.ext.lcValueSignature().nodeID = s.getPublicKey();
    sv.ext.lcValueSignature().signature =
        s.sign(xdr::xdr_to_opaque(mApp.getNetworkID(), ENVELOPE_TYPE_SCPVALUE,
                                  sv.txSetHash, sv.closeTime));
}
}

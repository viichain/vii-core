
#include "herder/HerderSCPDriver.h"
#include "crypto/Hex.h"
#include "crypto/SHA.h"
#include "crypto/SecretKey.h"
#include "herder/HerderImpl.h"
#include "herder/LedgerCloseData.h"
#include "herder/PendingEnvelopes.h"
#include "ledger/LedgerManager.h"
#include "main/Application.h"
#include "main/ErrorMessages.h"
#include "scp/SCP.h"
#include "scp/Slot.h"
#include "util/Logging.h"
#include "xdr/vii-SCP.h"
#include "xdr/vii-ledger-entries.h"
#include <medida/metrics_registry.h>
#include <util/format.h>
#include <xdrpp/marshal.h>

namespace viichain
{

HerderSCPDriver::SCPMetrics::SCPMetrics(Application& app)
    : mEnvelopeSign(
          app.getMetrics().NewMeter({"scp", "envelope", "sign"}, "envelope"))
    , mValueValid(app.getMetrics().NewMeter({"scp", "value", "valid"}, "value"))
    , mValueInvalid(
          app.getMetrics().NewMeter({"scp", "value", "invalid"}, "value"))
    , mCombinedCandidates(app.getMetrics().NewMeter(
          {"scp", "nomination", "combinecandidates"}, "value"))
    , mNominateToPrepare(
          app.getMetrics().NewTimer({"scp", "timing", "nominated"}))
    , mPrepareToExternalize(
          app.getMetrics().NewTimer({"scp", "timing", "externalized"}))
{
}

HerderSCPDriver::HerderSCPDriver(Application& app, HerderImpl& herder,
                                 Upgrades const& upgrades,
                                 PendingEnvelopes& pendingEnvelopes)
    : mApp{app}
    , mHerder{herder}
    , mLedgerManager{mApp.getLedgerManager()}
    , mUpgrades{upgrades}
    , mPendingEnvelopes{pendingEnvelopes}
    , mSCP{*this, mApp.getConfig().NODE_SEED.getPublicKey(),
           mApp.getConfig().NODE_IS_VALIDATOR, mApp.getConfig().QUORUM_SET}
    , mSCPMetrics{mApp}
    , mNominateTimeout{mApp.getMetrics().NewHistogram(
          {"scp", "timeout", "nominate"})}
    , mPrepareTimeout{
          mApp.getMetrics().NewHistogram({"scp", "timeout", "prepare"})}
{
}

HerderSCPDriver::~HerderSCPDriver()
{
}

void
HerderSCPDriver::stateChanged()
{
    mApp.syncOwnMetrics();
}

void
HerderSCPDriver::bootstrap()
{
    stateChanged();
    clearSCPExecutionEvents();
}

void
HerderSCPDriver::lostSync()
{
    stateChanged();

        mLastTrackingSCP.reset(mTrackingSCP.release());
}

Herder::State
HerderSCPDriver::getState() const
{
            return mTrackingSCP && mLastTrackingSCP ? Herder::HERDER_TRACKING_STATE
                                            : Herder::HERDER_SYNCING_STATE;
}

void
HerderSCPDriver::restoreSCPState(uint64_t index, VIIValue const& value)
{
    mTrackingSCP = std::make_unique<ConsensusData>(index, value);
}


void
HerderSCPDriver::signEnvelope(SCPEnvelope& envelope)
{
    mSCPMetrics.mEnvelopeSign.Mark();
    mHerder.signEnvelope(mApp.getConfig().NODE_SEED, envelope);
}

void
HerderSCPDriver::emitEnvelope(SCPEnvelope const& envelope)
{
    mHerder.emitEnvelope(envelope);
}


bool
HerderSCPDriver::isSlotCompatibleWithCurrentState(uint64_t slotIndex) const
{
    bool res = false;
    if (mLedgerManager.isSynced())
    {
        auto const& lcl = mLedgerManager.getLastClosedLedgerHeader();
        res = (slotIndex == (lcl.header.ledgerSeq + 1));
    }

    return res;
}

bool
HerderSCPDriver::checkCloseTime(uint64_t slotIndex, uint64_t lastCloseTime,
                                VIIValue const& b) const
{
        if (b.closeTime <= lastCloseTime)
    {
        CLOG(TRACE, "Herder")
            << "Close time too old for slot " << slotIndex << ", got "
            << b.closeTime << " vs " << lastCloseTime;
        return false;
    }

        uint64_t timeNow = mApp.timeNow();
    if (b.closeTime > timeNow + Herder::MAX_TIME_SLIP_SECONDS.count())
    {
        CLOG(TRACE, "Herder")
            << "Close time too far in future for slot " << slotIndex << ", got "
            << b.closeTime << " vs " << timeNow;
        return false;
    }
    return true;
}

SCPDriver::ValidationLevel
HerderSCPDriver::validateValueHelper(uint64_t slotIndex, VIIValue const& b,
                                     bool nomination) const
{
    uint64_t lastCloseTime;

    if (b.ext.v() == VII_VALUE_SIGNED)
    {
        if (nomination)
        {
            if (!mHerder.verifyVIIValueSignature(b))
            {
                return SCPDriver::kInvalidValue;
            }
        }
        else
        {
                        return SCPDriver::kInvalidValue;
        }
    }

    bool compat = isSlotCompatibleWithCurrentState(slotIndex);

    auto const& lcl = mLedgerManager.getLastClosedLedgerHeader().header;

        lastCloseTime = lcl.scpValue.closeTime;

    if (compat)
    {
        if (!checkCloseTime(slotIndex, lastCloseTime, b))
        {
            return SCPDriver::kInvalidValue;
        }
    }
    else
    {
        if (slotIndex == lcl.ledgerSeq)
        {
                        if (b.closeTime != lastCloseTime)
            {
                CLOG(TRACE, "Herder")
                    << "Got a bad close time for ledger " << slotIndex
                    << ", got " << b.closeTime << " vs " << lastCloseTime;
                return SCPDriver::kInvalidValue;
            }
        }
        else if (slotIndex < lcl.ledgerSeq)
        {
                        if (b.closeTime >= lastCloseTime)
            {
                CLOG(TRACE, "Herder")
                    << "Got a bad close time for ledger " << slotIndex
                    << ", got " << b.closeTime << " vs " << lastCloseTime;
                return SCPDriver::kInvalidValue;
            }
        }
        else if (!checkCloseTime(slotIndex, lastCloseTime, b))
        {
                        return SCPDriver::kInvalidValue;
        }

        if (!mTrackingSCP)
        {
                                    if (Logging::logTrace("Herder"))
            {
                CLOG(TRACE, "Herder")
                    << "MaybeValidValue (not tracking) for slot " << slotIndex;
            }
            return SCPDriver::kMaybeValidValue;
        }

                if (nextConsensusLedgerIndex() > slotIndex)
        {
                                    if (Logging::logTrace("Herder"))
            {
                CLOG(TRACE, "Herder")
                    << "MaybeValidValue (already moved on) for slot "
                    << slotIndex << ", at " << nextConsensusLedgerIndex();
            }
            return SCPDriver::kMaybeValidValue;
        }
        if (nextConsensusLedgerIndex() < slotIndex)
        {
                                    CLOG(ERROR, "Herder")
                << "HerderSCPDriver::validateValue"
                << " i: " << slotIndex
                << " processing a future message while tracking "
                << "(tracking: " << mTrackingSCP->mConsensusIndex << ", last: "
                << (mLastTrackingSCP ? mLastTrackingSCP->mConsensusIndex : 0)
                << " ) ";
            return SCPDriver::kInvalidValue;
        }

                lastCloseTime = mTrackingSCP->mConsensusValue.closeTime;
        if (!checkCloseTime(slotIndex, lastCloseTime, b))
        {
            return SCPDriver::kInvalidValue;
        }

                if (Logging::logTrace("Herder"))
        {
            CLOG(TRACE, "Herder")
                << "Can't validate locally, value may be valid for slot "
                << slotIndex;
        }
        return SCPDriver::kMaybeValidValue;
    }

    Hash const& txSetHash = b.txSetHash;

    
    if ((!nomination || lcl.ledgerVersion < 11) &&
        b.ext.v() != VII_VALUE_BASIC)
    {
                        CLOG(TRACE, "Herder")
            << "HerderSCPDriver::validateValue"
            << " i: " << slotIndex << " invalid value type - expected BASIC";
        return SCPDriver::kInvalidValue;
    }
    if (nomination &&
        (lcl.ledgerVersion >= 11 && b.ext.v() != VII_VALUE_SIGNED))
    {
                CLOG(TRACE, "Herder")
            << "HerderSCPDriver::validateValue"
            << " i: " << slotIndex << " invalid value type - expected SIGNED";
        return SCPDriver::kInvalidValue;
    }

    TxSetFramePtr txSet = mPendingEnvelopes.getTxSet(txSetHash);

    SCPDriver::ValidationLevel res;

    if (!txSet)
    {
        CLOG(ERROR, "Herder") << "HerderSCPDriver::validateValue"
                              << " i: " << slotIndex << " txSet not found?";

        res = SCPDriver::kInvalidValue;
    }
    else if (!txSet->checkValid(mApp))
    {
        if (Logging::logDebug("Herder"))
            CLOG(DEBUG, "Herder") << "HerderSCPDriver::validateValue"
                                  << " i: " << slotIndex << " Invalid txSet:"
                                  << " " << hexAbbrev(txSet->getContentsHash());
        res = SCPDriver::kInvalidValue;
    }
    else
    {
        if (Logging::logDebug("Herder"))
            CLOG(DEBUG, "Herder")
                << "HerderSCPDriver::validateValue"
                << " i: " << slotIndex
                << " txSet: " << hexAbbrev(txSet->getContentsHash()) << " OK";
        res = SCPDriver::kFullyValidatedValue;
    }
    return res;
}

SCPDriver::ValidationLevel
HerderSCPDriver::validateValue(uint64_t slotIndex, Value const& value,
                               bool nomination)
{
    VIIValue b;
    try
    {
        xdr::xdr_from_opaque(value, b);
    }
    catch (...)
    {
        mSCPMetrics.mValueInvalid.Mark();
        return SCPDriver::kInvalidValue;
    }

    SCPDriver::ValidationLevel res =
        validateValueHelper(slotIndex, b, nomination);
    if (res != SCPDriver::kInvalidValue)
    {
        auto const& lcl = mLedgerManager.getLastClosedLedgerHeader();

        LedgerUpgradeType lastUpgradeType = LEDGER_UPGRADE_VERSION;
                for (size_t i = 0;
             i < b.upgrades.size() && res != SCPDriver::kInvalidValue; i++)
        {
            LedgerUpgradeType thisUpgradeType;
            if (!mUpgrades.isValid(b.upgrades[i], thisUpgradeType, nomination,
                                   mApp.getConfig(), lcl.header))
            {
                CLOG(TRACE, "Herder")
                    << "HerderSCPDriver::validateValue invalid step at index "
                    << i;
                res = SCPDriver::kInvalidValue;
            }
            else if (i != 0 && (lastUpgradeType >= thisUpgradeType))
            {
                CLOG(TRACE, "Herder")
                    << "HerderSCPDriver::validateValue out of "
                       "order upgrade step at index "
                    << i;
                res = SCPDriver::kInvalidValue;
            }

            lastUpgradeType = thisUpgradeType;
        }
    }

    if (res)
    {
        mSCPMetrics.mValueValid.Mark();
    }
    else
    {
        mSCPMetrics.mValueInvalid.Mark();
    }
    return res;
}

Value
HerderSCPDriver::extractValidValue(uint64_t slotIndex, Value const& value)
{
    VIIValue b;
    try
    {
        xdr::xdr_from_opaque(value, b);
    }
    catch (...)
    {
        return Value();
    }
    Value res;
    if (validateValueHelper(slotIndex, b, true) ==
        SCPDriver::kFullyValidatedValue)
    {
        auto const& lcl = mLedgerManager.getLastClosedLedgerHeader();

                LedgerUpgradeType thisUpgradeType;
        for (auto it = b.upgrades.begin(); it != b.upgrades.end();)
        {
            if (!mUpgrades.isValid(*it, thisUpgradeType, true, mApp.getConfig(),
                                   lcl.header))
            {
                it = b.upgrades.erase(it);
            }
            else
            {
                it++;
            }
        }

        res = xdr::xdr_to_opaque(b);
    }

    return res;
}


std::string
HerderSCPDriver::toShortString(PublicKey const& pk) const
{
    return mApp.getConfig().toShortString(pk);
}

std::string
HerderSCPDriver::getValueString(Value const& v) const
{
    VIIValue b;
    if (v.empty())
    {
        return "[:empty:]";
    }

    try
    {
        xdr::xdr_from_opaque(v, b);

        return viiValueToString(mApp.getConfig(), b);
    }
    catch (...)
    {
        return "[:invalid:]";
    }
}

void
HerderSCPDriver::timerCallbackWrapper(uint64_t slotIndex, int timerID,
                                      std::function<void()> cb)
{
        if (trackingSCP() && nextConsensusLedgerIndex() != slotIndex)
    {
        CLOG(WARNING, "Herder")
            << "Herder rescheduled timer " << timerID << " for slot "
            << slotIndex << " with next slot " << nextConsensusLedgerIndex();
        setupTimer(slotIndex, timerID, std::chrono::seconds(1),
                   std::bind(&HerderSCPDriver::timerCallbackWrapper, this,
                             slotIndex, timerID, cb));
    }
    else
    {
        auto SCPTimingIt = mSCPExecutionTimes.find(slotIndex);
        if (SCPTimingIt != mSCPExecutionTimes.end())
        {
            auto& SCPTiming = SCPTimingIt->second;
            if (timerID == Slot::BALLOT_PROTOCOL_TIMER)
            {
                                ++SCPTiming.mPrepareTimeoutCount;
            }
            else
            {
                if (!SCPTiming.mPrepareStart)
                {
                                        ++SCPTiming.mNominationTimeoutCount;
                }
            }
        }

        cb();
    }
}

void
HerderSCPDriver::setupTimer(uint64_t slotIndex, int timerID,
                            std::chrono::milliseconds timeout,
                            std::function<void()> cb)
{
        if (slotIndex <= mApp.getHerder().getCurrentLedgerSeq())
    {
        mSCPTimers.erase(slotIndex);
        return;
    }

    auto& slotTimers = mSCPTimers[slotIndex];

    auto it = slotTimers.find(timerID);
    if (it == slotTimers.end())
    {
        it = slotTimers.emplace(timerID, std::make_unique<VirtualTimer>(mApp))
                 .first;
    }
    auto& timer = *it->second;
    timer.cancel();
    if (cb)
    {
        timer.expires_from_now(timeout);
        timer.async_wait(std::bind(&HerderSCPDriver::timerCallbackWrapper, this,
                                   slotIndex, timerID, cb),
                         &VirtualTimer::onFailureNoop);
    }
}

static bool
compareTxSets(TxSetFramePtr l, TxSetFramePtr r, Hash const& lh, Hash const& rh,
              LedgerHeader const& header, Hash const& s)
{
    if (l == nullptr)
    {
        return r != nullptr;
    }
    if (r == nullptr)
    {
        return false;
    }
    auto lSize = l->size(header);
    auto rSize = r->size(header);
    if (lSize < rSize)
    {
        return true;
    }
    else if (lSize > rSize)
    {
        return false;
    }
    if (header.ledgerVersion >= 11)
    {
        auto lFee = l->getTotalFees(header);
        auto rFee = r->getTotalFees(header);
        if (lFee < rFee)
        {
            return true;
        }
        else if (lFee > rFee)
        {
            return false;
        }
    }
    return lessThanXored(lh, rh, s);
}

Value
HerderSCPDriver::combineCandidates(uint64_t slotIndex,
                                   std::set<Value> const& candidates)
{
    CLOG(DEBUG, "Herder") << "Combining " << candidates.size() << " candidates";
    mSCPMetrics.mCombinedCandidates.Mark(candidates.size());

    Hash h;

    VIIValue comp(h, 0, emptyUpgradeSteps, VII_VALUE_BASIC);

    std::map<LedgerUpgradeType, LedgerUpgrade> upgrades;

    std::set<TransactionFramePtr> aggSet;

    auto const& lcl = mLedgerManager.getLastClosedLedgerHeader();

    Hash candidatesHash;

    std::vector<VIIValue> candidateValues;

    for (auto const& c : candidates)
    {
        candidateValues.emplace_back();
        VIIValue& sv = candidateValues.back();

        xdr::xdr_from_opaque(c, sv);
        candidatesHash ^= sha256(c);

                if (comp.closeTime < sv.closeTime)
        {
            comp.closeTime = sv.closeTime;
        }
        for (auto const& upgrade : sv.upgrades)
        {
            LedgerUpgrade lupgrade;
            xdr::xdr_from_opaque(upgrade, lupgrade);
            auto it = upgrades.find(lupgrade.type());
            if (it == upgrades.end())
            {
                upgrades.emplace(std::make_pair(lupgrade.type(), lupgrade));
            }
            else
            {
                LedgerUpgrade& clUpgrade = it->second;
                switch (lupgrade.type())
                {
                case LEDGER_UPGRADE_VERSION:
                                        clUpgrade.newLedgerVersion() =
                        std::max(clUpgrade.newLedgerVersion(),
                                 lupgrade.newLedgerVersion());
                    break;
                case LEDGER_UPGRADE_BASE_FEE:
                                        clUpgrade.newBaseFee() =
                        std::max(clUpgrade.newBaseFee(), lupgrade.newBaseFee());
                    break;
                case LEDGER_UPGRADE_MAX_TX_SET_SIZE:
                                        clUpgrade.newMaxTxSetSize() =
                        std::max(clUpgrade.newMaxTxSetSize(),
                                 lupgrade.newMaxTxSetSize());
                    break;
                case LEDGER_UPGRADE_BASE_RESERVE:
                                        clUpgrade.newBaseReserve() = std::max(
                        clUpgrade.newBaseReserve(), lupgrade.newBaseReserve());
                    break;
                default:
                                        throw std::runtime_error("invalid upgrade step");
                }
            }
        }
    }

        TxSetFramePtr bestTxSet;
    {
        Hash highest;
        TxSetFramePtr highestTxSet;
        for (auto const& sv : candidateValues)
        {
            TxSetFramePtr cTxSet = mPendingEnvelopes.getTxSet(sv.txSetHash);

            if (cTxSet && cTxSet->previousLedgerHash() == lcl.hash)
            {
                if (compareTxSets(highestTxSet, cTxSet, highest, sv.txSetHash,
                                  lcl.header, candidatesHash))
                {
                    highestTxSet = cTxSet;
                    highest = sv.txSetHash;
                }
            }
        }
                        bestTxSet = std::make_shared<TxSetFrame>(*highestTxSet);
    }

    for (auto const& upgrade : upgrades)
    {
        Value v(xdr::xdr_to_opaque(upgrade.second));
        comp.upgrades.emplace_back(v.begin(), v.end());
    }

        auto removed = bestTxSet->trimInvalid(mApp);
    comp.txSetHash = bestTxSet->getContentsHash();

    if (removed.size() != 0)
    {
        CLOG(WARNING, "Herder") << "Candidate set had " << removed.size()
                                << " invalid transactions";

                mApp.postOnMainThreadWithDelay(
            [this, bestTxSet]() {
                mPendingEnvelopes.recvTxSet(bestTxSet->getContentsHash(),
                                            bestTxSet);
            },
            "HerderSCPDriver: combineCandidates posts recvTxSet");
    }

        comp.ext.v(VII_VALUE_BASIC);
    return xdr::xdr_to_opaque(comp);
}

bool
HerderSCPDriver::toVIIValue(Value const& v, VIIValue& sv)
{
    try
    {
        xdr::xdr_from_opaque(v, sv);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

void
HerderSCPDriver::valueExternalized(uint64_t slotIndex, Value const& value)
{
    auto it = mSCPTimers.begin(); // cancel all timers below this slot
    while (it != mSCPTimers.end() && it->first <= slotIndex)
    {
        it = mSCPTimers.erase(it);
    }

    if (slotIndex <= mApp.getHerder().getCurrentLedgerSeq())
    {
                                                CLOG(DEBUG, "Herder")
            << "Ignoring old ledger externalize " << slotIndex;
        return;
    }

    VIIValue b;
    try
    {
        xdr::xdr_from_opaque(value, b);
    }
    catch (...)
    {
                        CLOG(ERROR, "Herder") << "HerderSCPDriver::valueExternalized"
                              << " Externalized VIIValue malformed";
        CLOG(ERROR, "Herder") << REPORT_INTERNAL_BUG;
                abort();
    }

            if (slotIndex > 2)
    {
        logQuorumInformation(slotIndex - 2);
    }

    if (!mCurrentValue.empty())
    {
                                                mSCP.stopNomination(mLedgerSeqNominating);
        mCurrentValue.clear();
    }

    if (!mTrackingSCP)
    {
        stateChanged();
    }

    mTrackingSCP = std::make_unique<ConsensusData>(slotIndex, b);

    if (!mLastTrackingSCP)
    {
        mLastTrackingSCP = std::make_unique<ConsensusData>(*mTrackingSCP);
    }

    mHerder.valueExternalized(slotIndex, b);
}

void
HerderSCPDriver::logQuorumInformation(uint64_t index)
{
    std::string res;
    auto v = mApp.getHerder().getJsonQuorumInfo(mSCP.getLocalNodeID(), true,
                                                false, index);
    auto qset = v.get("qset", "");
    if (!qset.empty())
    {
        std::string indexs = std::to_string(static_cast<uint32>(index));
        Json::FastWriter fw;
        CLOG(INFO, "Herder")
            << "Quorum information for " << index << " : " << fw.write(qset);
    }
}

void
HerderSCPDriver::nominate(uint64_t slotIndex, VIIValue const& value,
                          TxSetFramePtr proposedSet,
                          VIIValue const& previousValue)
{
    mCurrentValue = xdr::xdr_to_opaque(value);
    mLedgerSeqNominating = static_cast<uint32_t>(slotIndex);

    auto valueHash = sha256(xdr::xdr_to_opaque(mCurrentValue));
    CLOG(DEBUG, "Herder") << "HerderSCPDriver::triggerNextLedger"
                          << " txSet.size: "
                          << proposedSet->mTransactions.size()
                          << " previousLedgerHash: "
                          << hexAbbrev(proposedSet->previousLedgerHash())
                          << " value: " << hexAbbrev(valueHash)
                          << " slot: " << slotIndex;

    auto prevValue = xdr::xdr_to_opaque(previousValue);
    mSCP.nominate(slotIndex, mCurrentValue, prevValue);
}

SCPQuorumSetPtr
HerderSCPDriver::getQSet(Hash const& qSetHash)
{
    return mPendingEnvelopes.getQSet(qSetHash);
}

void
HerderSCPDriver::ballotDidHearFromQuorum(uint64_t, SCPBallot const&)
{
}

void
HerderSCPDriver::nominatingValue(uint64_t slotIndex, Value const& value)
{
    if (Logging::logDebug("Herder"))
        CLOG(DEBUG, "Herder") << "nominatingValue i:" << slotIndex
                              << " v: " << getValueString(value);
}

void
HerderSCPDriver::updatedCandidateValue(uint64_t slotIndex, Value const& value)
{
}

void
HerderSCPDriver::startedBallotProtocol(uint64_t slotIndex,
                                       SCPBallot const& ballot)
{
    recordSCPEvent(slotIndex, false);
}
void
HerderSCPDriver::acceptedBallotPrepared(uint64_t slotIndex,
                                        SCPBallot const& ballot)
{
}

void
HerderSCPDriver::confirmedBallotPrepared(uint64_t slotIndex,
                                         SCPBallot const& ballot)
{
}

void
HerderSCPDriver::acceptedCommit(uint64_t slotIndex, SCPBallot const& ballot)
{
}

optional<VirtualClock::time_point>
HerderSCPDriver::getPrepareStart(uint64_t slotIndex)
{
    optional<VirtualClock::time_point> res;
    auto it = mSCPExecutionTimes.find(slotIndex);
    if (it != mSCPExecutionTimes.end())
    {
        res = it->second.mPrepareStart;
    }
    return res;
}

void
HerderSCPDriver::recordSCPEvent(uint64_t slotIndex, bool isNomination)
{

    auto& timing = mSCPExecutionTimes[slotIndex];
    VirtualClock::time_point start = mApp.getClock().now();

    if (isNomination)
    {
        timing.mNominationStart =
            make_optional<VirtualClock::time_point>(start);
    }
    else
    {
        timing.mPrepareStart = make_optional<VirtualClock::time_point>(start);
    }
}

void
HerderSCPDriver::recordSCPExecutionMetrics(uint64_t slotIndex)
{
    auto externalizeStart = mApp.getClock().now();

        auto& qset = mApp.getConfig().QUORUM_SET;
    auto isSingleNode = qset.innerSets.size() == 0 &&
                        qset.validators.size() == 1 &&
                        qset.validators[0] == getSCP().getLocalNodeID();
    auto threshold = isSingleNode ? std::chrono::nanoseconds::zero()
                                  : Herder::TIMERS_THRESHOLD_NANOSEC;

    auto SCPTimingIt = mSCPExecutionTimes.find(slotIndex);
    if (SCPTimingIt == mSCPExecutionTimes.end())
    {
        return;
    }

    auto& SCPTiming = SCPTimingIt->second;

    mNominateTimeout.Update(SCPTiming.mNominationTimeoutCount);
    mPrepareTimeout.Update(SCPTiming.mPrepareTimeoutCount);

    auto recordTiming = [&](VirtualClock::time_point start,
                            VirtualClock::time_point end, medida::Timer& timer,
                            std::string const& logStr) {
        auto delta =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        CLOG(DEBUG, "Herder") << fmt::format("{} delta for slot {} is {} ns.",
                                             logStr, slotIndex, delta.count());
        if (delta >= threshold)
        {
            timer.Update(delta);
        }
    };

        if (SCPTiming.mNominationStart && SCPTiming.mPrepareStart)
    {
        recordTiming(*SCPTiming.mNominationStart, *SCPTiming.mPrepareStart,
                     mSCPMetrics.mNominateToPrepare, "Nominate");
    }

        if (SCPTiming.mPrepareStart)
    {
        recordTiming(*SCPTiming.mPrepareStart, externalizeStart,
                     mSCPMetrics.mPrepareToExternalize, "Prepare");
    }

        auto it = mSCPExecutionTimes.begin();
    while (it != mSCPExecutionTimes.end() && it->first < slotIndex)
    {
        it = mSCPExecutionTimes.erase(it);
    }
}

void
HerderSCPDriver::clearSCPExecutionEvents()
{
    mSCPExecutionTimes.clear();
}
}


#include "BallotProtocol.h"

#include "Slot.h"
#include "crypto/Hex.h"
#include "crypto/SHA.h"
#include "lib/json/json.h"
#include "scp/LocalNode.h"
#include "scp/QuorumSetUtils.h"
#include "util/GlobalChecks.h"
#include "util/Logging.h"
#include "util/XDROperators.h"
#include "xdrpp/marshal.h"
#include <functional>

namespace viichain
{
using namespace std::placeholders;

static const int MAX_ADVANCE_SLOT_RECURSION = 50;

BallotProtocol::BallotProtocol(Slot& slot)
    : mSlot(slot)
    , mHeardFromQuorum(false)
    , mPhase(SCP_PHASE_PREPARE)
    , mCurrentMessageLevel(0)
{
}

bool
BallotProtocol::isNewerStatement(NodeID const& nodeID, SCPStatement const& st)
{
    auto oldp = mLatestEnvelopes.find(nodeID);
    bool res = false;

    if (oldp == mLatestEnvelopes.end())
    {
        res = true;
    }
    else
    {
        res = isNewerStatement(oldp->second.statement, st);
    }
    return res;
}

bool
BallotProtocol::isNewerStatement(SCPStatement const& oldst,
                                 SCPStatement const& st)
{
    bool res = false;

        auto t = st.pledges.type();

        if (oldst.pledges.type() != t)
    {
        res = (oldst.pledges.type() < t);
    }
    else
    {
                if (t == SCPStatementType::SCP_ST_EXTERNALIZE)
        {
            res = false;
        }
        else if (t == SCPStatementType::SCP_ST_CONFIRM)
        {
                        auto const& oldC = oldst.pledges.confirm();
            auto const& c = st.pledges.confirm();
            int compBallot = compareBallots(oldC.ballot, c.ballot);
            if (compBallot < 0)
            {
                res = true;
            }
            else if (compBallot == 0)
            {
                if (oldC.nPrepared == c.nPrepared)
                {
                    res = (oldC.nH < c.nH);
                }
                else
                {
                    res = (oldC.nPrepared < c.nPrepared);
                }
            }
        }
        else
        {
                                    auto const& oldPrep = oldst.pledges.prepare();
            auto const& prep = st.pledges.prepare();

            int compBallot = compareBallots(oldPrep.ballot, prep.ballot);
            if (compBallot < 0)
            {
                res = true;
            }
            else if (compBallot == 0)
            {
                compBallot = compareBallots(oldPrep.prepared, prep.prepared);
                if (compBallot < 0)
                {
                    res = true;
                }
                else if (compBallot == 0)
                {
                    compBallot = compareBallots(oldPrep.preparedPrime,
                                                prep.preparedPrime);
                    if (compBallot < 0)
                    {
                        res = true;
                    }
                    else if (compBallot == 0)
                    {
                        res = (oldPrep.nH < prep.nH);
                    }
                }
            }
        }
    }

    return res;
}

void
BallotProtocol::recordEnvelope(SCPEnvelope const& env)
{
    auto const& st = env.statement;
    auto oldp = mLatestEnvelopes.find(st.nodeID);
    if (oldp == mLatestEnvelopes.end())
    {
        mLatestEnvelopes.insert(std::make_pair(st.nodeID, env));
    }
    else
    {
        oldp->second = env;
    }
    mSlot.recordStatement(env.statement);
}

SCP::EnvelopeState
BallotProtocol::processEnvelope(SCPEnvelope const& envelope, bool self)
{
    SCP::EnvelopeState res = SCP::EnvelopeState::INVALID;
    dbgAssert(envelope.statement.slotIndex == mSlot.getSlotIndex());

    SCPStatement const& statement = envelope.statement;
    NodeID const& nodeID = statement.nodeID;

    if (!isStatementSane(statement, self))
    {
        if (self)
        {
            CLOG(ERROR, "SCP") << "not sane statement from self, skipping "
                               << "  e: " << mSlot.getSCP().envToStr(envelope);
        }

        return SCP::EnvelopeState::INVALID;
    }

    if (!isNewerStatement(nodeID, statement))
    {
        if (self)
        {
            CLOG(ERROR, "SCP") << "stale statement from self, skipping "
                               << "  e: " << mSlot.getSCP().envToStr(envelope);
        }
        else
        {
            CLOG(TRACE, "SCP") << "stale statement, skipping "
                               << " i: " << mSlot.getSlotIndex();
        }

        return SCP::EnvelopeState::INVALID;
    }

    auto validationRes = validateValues(statement);
    if (validationRes != SCPDriver::kInvalidValue)
    {
        bool processed = false;

        if (mPhase != SCP_PHASE_EXTERNALIZE)
        {
            if (validationRes == SCPDriver::kMaybeValidValue)
            {
                mSlot.setFullyValidated(false);
            }

            recordEnvelope(envelope);
            processed = true;
            advanceSlot(statement);
            res = SCP::EnvelopeState::VALID;
        }

        if (!processed)
        {
                                    if (mPhase == SCP_PHASE_EXTERNALIZE &&
                mCommit->value == getWorkingBallot(statement).value)
            {
                recordEnvelope(envelope);
                res = SCP::EnvelopeState::VALID;
            }
            else
            {
                if (self)
                {
                    CLOG(ERROR, "SCP")
                        << "externalize statement with invalid value from "
                           "self, skipping "
                        << "  e: " << mSlot.getSCP().envToStr(envelope);
                }

                res = SCP::EnvelopeState::INVALID;
            }
        }
    }
    else
    {
                if (self)
        {
            CLOG(ERROR, "SCP") << "invalid value from self, skipping "
                               << "  e: " << mSlot.getSCP().envToStr(envelope);
        }
        else
        {
            CLOG(TRACE, "SCP") << "invalid value "
                               << " i: " << mSlot.getSlotIndex();
        }

        res = SCP::EnvelopeState::INVALID;
    }
    return res;
}

bool
BallotProtocol::isStatementSane(SCPStatement const& st, bool self)
{
    auto qSet = mSlot.getQuorumSetFromStatement(st);
    bool res = qSet != nullptr && isQuorumSetSane(*qSet, false);
    if (!res)
    {
        CLOG(DEBUG, "SCP") << "Invalid quorum set received";
        return false;
    }

    switch (st.pledges.type())
    {
    case SCPStatementType::SCP_ST_PREPARE:
    {
        auto const& p = st.pledges.prepare();
                bool isOK = self || p.ballot.counter > 0;

        isOK = isOK &&
               ((!p.preparedPrime || !p.prepared) ||
                (areBallotsLessAndIncompatible(*p.preparedPrime, *p.prepared)));

        isOK =
            isOK && (p.nH == 0 || (p.prepared && p.nH <= p.prepared->counter));

                isOK = isOK && (p.nC == 0 || (p.nH != 0 && p.ballot.counter >= p.nH &&
                                      p.nH >= p.nC));

        if (!isOK)
        {
            CLOG(TRACE, "SCP") << "Malformed PREPARE message";
            res = false;
        }
    }
    break;
    case SCPStatementType::SCP_ST_CONFIRM:
    {
        auto const& c = st.pledges.confirm();
                res = c.ballot.counter > 0;
        res = res && (c.nH <= c.ballot.counter);
        res = res && (c.nCommit <= c.nH);
        if (!res)
        {
            CLOG(TRACE, "SCP") << "Malformed CONFIRM message";
        }
    }
    break;
    case SCPStatementType::SCP_ST_EXTERNALIZE:
    {
        auto const& e = st.pledges.externalize();

        res = e.commit.counter > 0;
        res = res && e.nH >= e.commit.counter;

        if (!res)
        {
            CLOG(TRACE, "SCP") << "Malformed EXTERNALIZE message";
        }
    }
    break;
    default:
        dbgAbort();
    }

    return res;
}

bool
BallotProtocol::abandonBallot(uint32 n)
{
    CLOG(TRACE, "SCP") << "BallotProtocol::abandonBallot";
    bool res = false;
    Value v = mSlot.getLatestCompositeCandidate();
    if (v.empty())
    {
        if (mCurrentBallot)
        {
            v = mCurrentBallot->value;
        }
    }
    if (!v.empty())
    {
        if (n == 0)
        {
            res = bumpState(v, true);
        }
        else
        {
            res = bumpState(v, n);
        }
    }
    return res;
}

bool
BallotProtocol::bumpState(Value const& value, bool force)
{
    uint32 n;
    if (!force && mCurrentBallot)
    {
        return false;
    }

    n = mCurrentBallot ? (mCurrentBallot->counter + 1) : 1;

    return bumpState(value, n);
}

bool
BallotProtocol::bumpState(Value const& value, uint32 n)
{
    if (mPhase != SCP_PHASE_PREPARE && mPhase != SCP_PHASE_CONFIRM)
    {
        return false;
    }

    SCPBallot newb;

    newb.counter = n;

    if (mValueOverride)
    {
                        newb.value = *mValueOverride;
    }
    else
    {
        newb.value = value;
    }

    if (Logging::logTrace("SCP"))
        CLOG(TRACE, "SCP") << "BallotProtocol::bumpState"
                           << " i: " << mSlot.getSlotIndex()
                           << " v: " << mSlot.getSCP().ballotToStr(newb);

    bool updated = updateCurrentValue(newb);

    if (updated)
    {
        emitCurrentStateStatement();
        checkHeardFromQuorum();
    }

    return updated;
}

bool
BallotProtocol::updateCurrentValue(SCPBallot const& ballot)
{
    if (mPhase != SCP_PHASE_PREPARE && mPhase != SCP_PHASE_CONFIRM)
    {
        return false;
    }

    bool updated = false;
    if (!mCurrentBallot)
    {
        bumpToBallot(ballot, true);
        updated = true;
    }
    else
    {
        dbgAssert(compareBallots(*mCurrentBallot, ballot) <= 0);

        if (mCommit && !areBallotsCompatible(*mCommit, ballot))
        {
            return false;
        }

        int comp = compareBallots(*mCurrentBallot, ballot);
        if (comp < 0)
        {
            bumpToBallot(ballot, true);
            updated = true;
        }
        else if (comp > 0)
        {
                        
                                                            CLOG(ERROR, "SCP")
                << "BallotProtocol::updateCurrentValue attempt to bump to "
                   "a smaller value";
                                    return false;
        }
    }

    if (updated)
    {
        CLOG(TRACE, "SCP") << "BallotProtocol::updateCurrentValue updated";
    }

    checkInvariants();

    return updated;
}

void
BallotProtocol::bumpToBallot(SCPBallot const& ballot, bool check)
{
    if (Logging::logTrace("SCP"))
        CLOG(TRACE, "SCP") << "BallotProtocol::bumpToBallot"
                           << " i: " << mSlot.getSlotIndex()
                           << " b: " << mSlot.getSCP().ballotToStr(ballot);

        dbgAssert(mPhase != SCP_PHASE_EXTERNALIZE);

    if (check)
    {
                dbgAssert(!mCurrentBallot ||
                  compareBallots(ballot, *mCurrentBallot) >= 0);
    }

    bool gotBumped =
        !mCurrentBallot || (mCurrentBallot->counter != ballot.counter);

    if (!mCurrentBallot)
    {
        mSlot.getSCPDriver().startedBallotProtocol(mSlot.getSlotIndex(),
                                                   ballot);
    }

    mCurrentBallot = std::make_unique<SCPBallot>(ballot);

        if (mHighBallot && !areBallotsCompatible(*mCurrentBallot, *mHighBallot))
    {
        mHighBallot.reset();
    }

    if (gotBumped)
    {
        mHeardFromQuorum = false;
    }
}

void
BallotProtocol::startBallotProtocolTimer()
{
    std::chrono::milliseconds timeout =
        mSlot.getSCPDriver().computeTimeout(mCurrentBallot->counter);

    std::shared_ptr<Slot> slot = mSlot.shared_from_this();
    mSlot.getSCPDriver().setupTimer(
        mSlot.getSlotIndex(), Slot::BALLOT_PROTOCOL_TIMER, timeout,
        [slot]() { slot->getBallotProtocol().ballotProtocolTimerExpired(); });
}

void
BallotProtocol::stopBallotProtocolTimer()
{
    std::shared_ptr<Slot> slot = mSlot.shared_from_this();
    mSlot.getSCPDriver().setupTimer(mSlot.getSlotIndex(),
                                    Slot::BALLOT_PROTOCOL_TIMER,
                                    std::chrono::seconds::zero(), nullptr);
}

void
BallotProtocol::ballotProtocolTimerExpired()
{
    abandonBallot(0);
}

SCPStatement
BallotProtocol::createStatement(SCPStatementType const& type)
{
    SCPStatement statement;

    checkInvariants();

    statement.pledges.type(type);
    switch (type)
    {
    case SCPStatementType::SCP_ST_PREPARE:
    {
        auto& p = statement.pledges.prepare();
        p.quorumSetHash = getLocalNode()->getQuorumSetHash();
        if (mCurrentBallot)
        {
            p.ballot = *mCurrentBallot;
        }
        if (mCommit)
        {
            p.nC = mCommit->counter;
        }
        if (mPrepared)
        {
            p.prepared.activate() = *mPrepared;
        }
        if (mPreparedPrime)
        {
            p.preparedPrime.activate() = *mPreparedPrime;
        }
        if (mHighBallot)
        {
            p.nH = mHighBallot->counter;
        }
    }
    break;
    case SCPStatementType::SCP_ST_CONFIRM:
    {
        auto& c = statement.pledges.confirm();
        c.quorumSetHash = getLocalNode()->getQuorumSetHash();
        c.ballot = *mCurrentBallot;
        c.nPrepared = mPrepared->counter;
        c.nCommit = mCommit->counter;
        c.nH = mHighBallot->counter;
    }
    break;
    case SCPStatementType::SCP_ST_EXTERNALIZE:
    {
        auto& e = statement.pledges.externalize();
        e.commit = *mCommit;
        e.nH = mHighBallot->counter;
        e.commitQuorumSetHash = getLocalNode()->getQuorumSetHash();
    }
    break;
    default:
        dbgAbort();
    }

    return statement;
}

void
BallotProtocol::emitCurrentStateStatement()
{
    SCPStatementType t;

    switch (mPhase)
    {
    case SCP_PHASE_PREPARE:
        t = SCP_ST_PREPARE;
        break;
    case SCP_PHASE_CONFIRM:
        t = SCP_ST_CONFIRM;
        break;
    case SCP_PHASE_EXTERNALIZE:
        t = SCP_ST_EXTERNALIZE;
        break;
    default:
        dbgAbort();
    }

    SCPStatement statement = createStatement(t);
    SCPEnvelope envelope = mSlot.createEnvelope(statement);

    bool canEmit = (mCurrentBallot != nullptr);

                auto lastEnv = mLatestEnvelopes.find(mSlot.getSCP().getLocalNodeID());

    if (lastEnv == mLatestEnvelopes.end() || !(lastEnv->second == envelope))
    {
        if (mSlot.processEnvelope(envelope, true) == SCP::EnvelopeState::VALID)
        {
            if (canEmit &&
                (!mLastEnvelope || isNewerStatement(mLastEnvelope->statement,
                                                    envelope.statement)))
            {
                mLastEnvelope = std::make_shared<SCPEnvelope>(envelope);
                                                sendLatestEnvelope();
            }
        }
        else
        {
                                    throw std::runtime_error("moved to a bad state (ballot protocol)");
        }
    }
}

void
BallotProtocol::checkInvariants()
{
    if (mCurrentBallot)
    {
        dbgAssert(mCurrentBallot->counter != 0);
    }
    if (mPrepared && mPreparedPrime)
    {
        dbgAssert(areBallotsLessAndIncompatible(*mPreparedPrime, *mPrepared));
    }
    if (mHighBallot)
    {
        dbgAssert(mCurrentBallot);
        dbgAssert(areBallotsLessAndCompatible(*mHighBallot, *mCurrentBallot));
    }
    if (mCommit)
    {
        dbgAssert(mCurrentBallot);
        dbgAssert(areBallotsLessAndCompatible(*mCommit, *mHighBallot));
        dbgAssert(areBallotsLessAndCompatible(*mHighBallot, *mCurrentBallot));
    }

    switch (mPhase)
    {
    case SCP_PHASE_PREPARE:
        break;
    case SCP_PHASE_CONFIRM:
        dbgAssert(mCommit);
        break;
    case SCP_PHASE_EXTERNALIZE:
        dbgAssert(mCommit);
        dbgAssert(mHighBallot);
        break;
    default:
        dbgAbort();
    }
}

std::set<SCPBallot>
BallotProtocol::getPrepareCandidates(SCPStatement const& hint)
{
    std::set<SCPBallot> hintBallots;

    switch (hint.pledges.type())
    {
    case SCP_ST_PREPARE:
    {
        auto const& prep = hint.pledges.prepare();
        hintBallots.insert(prep.ballot);
        if (prep.prepared)
        {
            hintBallots.insert(*prep.prepared);
        }
        if (prep.preparedPrime)
        {
            hintBallots.insert(*prep.preparedPrime);
        }
    }
    break;
    case SCP_ST_CONFIRM:
    {
        auto const& con = hint.pledges.confirm();
        hintBallots.insert(SCPBallot(con.nPrepared, con.ballot.value));
        hintBallots.insert(SCPBallot(UINT32_MAX, con.ballot.value));
    }
    break;
    case SCP_ST_EXTERNALIZE:
    {
        auto const& ext = hint.pledges.externalize();
        hintBallots.insert(SCPBallot(UINT32_MAX, ext.commit.value));
    }
    break;
    default:
        abort();
    };

    std::set<SCPBallot> candidates;

    while (!hintBallots.empty())
    {
        auto last = --hintBallots.end();
        SCPBallot topVote = *last;
        hintBallots.erase(last);

        auto const& val = topVote.value;

                for (auto const& e : mLatestEnvelopes)
        {
            SCPStatement const& st = e.second.statement;
            switch (st.pledges.type())
            {
            case SCP_ST_PREPARE:
            {
                auto const& prep = st.pledges.prepare();
                if (areBallotsLessAndCompatible(prep.ballot, topVote))
                {
                    candidates.insert(prep.ballot);
                }
                if (prep.prepared &&
                    areBallotsLessAndCompatible(*prep.prepared, topVote))
                {
                    candidates.insert(*prep.prepared);
                }
                if (prep.preparedPrime &&
                    areBallotsLessAndCompatible(*prep.preparedPrime, topVote))
                {
                    candidates.insert(*prep.preparedPrime);
                }
            }
            break;
            case SCP_ST_CONFIRM:
            {
                auto const& con = st.pledges.confirm();
                if (areBallotsCompatible(topVote, con.ballot))
                {
                    candidates.insert(topVote);
                    if (con.nPrepared < topVote.counter)
                    {
                        candidates.insert(SCPBallot(con.nPrepared, val));
                    }
                }
            }
            break;
            case SCP_ST_EXTERNALIZE:
            {
                auto const& ext = st.pledges.externalize();
                if (areBallotsCompatible(topVote, ext.commit))
                {
                    candidates.insert(topVote);
                }
            }
            break;
            default:
                abort();
            }
        }
    }

    return candidates;
}

bool
BallotProtocol::updateCurrentIfNeeded(SCPBallot const& h)
{
    bool didWork = false;
    if (!mCurrentBallot || compareBallots(*mCurrentBallot, h) < 0)
    {
        bumpToBallot(h, true);
        didWork = true;
    }
    return didWork;
}

bool
BallotProtocol::attemptPreparedAccept(SCPStatement const& hint)
{
    if (mPhase != SCP_PHASE_PREPARE && mPhase != SCP_PHASE_CONFIRM)
    {
        return false;
    }

    auto candidates = getPrepareCandidates(hint);

        for (auto cur = candidates.rbegin(); cur != candidates.rend(); cur++)
    {
        SCPBallot ballot = *cur;

        if (mPhase == SCP_PHASE_CONFIRM)
        {
                                    if (!areBallotsLessAndCompatible(*mPrepared, ballot))
            {
                continue;
            }
            dbgAssert(areBallotsCompatible(*mCommit, ballot));
        }

        
                if (mPreparedPrime && compareBallots(ballot, *mPreparedPrime) <= 0)
        {
            continue;
        }

        if (mPrepared)
        {
                        if (areBallotsLessAndCompatible(ballot, *mPrepared))
            {
                continue;
            }
                    }

        bool accepted = federatedAccept(
                        [&ballot](SCPStatement const& st) {
                bool res;

                switch (st.pledges.type())
                {
                case SCP_ST_PREPARE:
                {
                    auto const& p = st.pledges.prepare();
                    res = areBallotsLessAndCompatible(ballot, p.ballot);
                }
                break;
                case SCP_ST_CONFIRM:
                {
                    auto const& c = st.pledges.confirm();
                    res = areBallotsCompatible(ballot, c.ballot);
                }
                break;
                case SCP_ST_EXTERNALIZE:
                {
                    auto const& e = st.pledges.externalize();
                    res = areBallotsCompatible(ballot, e.commit);
                }
                break;
                default:
                    res = false;
                    dbgAbort();
                }

                return res;
            },
            std::bind(&BallotProtocol::hasPreparedBallot, ballot, _1));
        if (accepted)
        {
            return setPreparedAccept(ballot);
        }
    }

    return false;
}

bool
BallotProtocol::setPreparedAccept(SCPBallot const& ballot)
{
    if (Logging::logTrace("SCP"))
        CLOG(TRACE, "SCP") << "BallotProtocol::setPreparedAccept"
                           << " i: " << mSlot.getSlotIndex()
                           << " b: " << mSlot.getSCP().ballotToStr(ballot);

        bool didWork = setPrepared(ballot);

        if (mCommit && mHighBallot)
    {
        if ((mPrepared &&
             areBallotsLessAndIncompatible(*mHighBallot, *mPrepared)) ||
            (mPreparedPrime &&
             areBallotsLessAndIncompatible(*mHighBallot, *mPreparedPrime)))
        {
            dbgAssert(mPhase == SCP_PHASE_PREPARE);
            mCommit.reset();
            didWork = true;
        }
    }

    if (didWork)
    {
        mSlot.getSCPDriver().acceptedBallotPrepared(mSlot.getSlotIndex(),
                                                    ballot);
        emitCurrentStateStatement();
    }

    return didWork;
}

bool
BallotProtocol::attemptPreparedConfirmed(SCPStatement const& hint)
{
    if (mPhase != SCP_PHASE_PREPARE)
    {
        return false;
    }

        if (!mPrepared)
    {
        return false;
    }

    auto candidates = getPrepareCandidates(hint);

        SCPBallot newH;
    bool newHfound = false;
    auto cur = candidates.rbegin();
    for (; cur != candidates.rend(); cur++)
    {
        SCPBallot ballot = *cur;

                if (mHighBallot && compareBallots(*mHighBallot, ballot) >= 0)
        {
            break;
        }

        bool ratified = federatedRatify(
            std::bind(&BallotProtocol::hasPreparedBallot, ballot, _1));
        if (ratified)
        {
            newH = ballot;
            newHfound = true;
            break;
        }
    }

    bool res = false;

    if (newHfound)
    {
        SCPBallot newC;
                        SCPBallot b = mCurrentBallot ? *mCurrentBallot : SCPBallot();
        if (!mCommit &&
            (!mPrepared || !areBallotsLessAndIncompatible(newH, *mPrepared)) &&
            (!mPreparedPrime ||
             !areBallotsLessAndIncompatible(newH, *mPreparedPrime)))
        {
                        for (; cur != candidates.rend(); cur++)
            {
                SCPBallot ballot = *cur;
                if (compareBallots(ballot, b) < 0)
                {
                    break;
                }
                                if (!areBallotsLessAndCompatible(*cur, newH))
                {
                    continue;
                }
                bool ratified = federatedRatify(
                    std::bind(&BallotProtocol::hasPreparedBallot, ballot, _1));
                if (ratified)
                {
                    newC = ballot;
                }
                else
                {
                    break;
                }
            }
        }
        res = setPreparedConfirmed(newC, newH);
    }
    return res;
}

bool
BallotProtocol::commitPredicate(SCPBallot const& ballot, Interval const& check,
                                SCPStatement const& st)
{
    bool res = false;
    auto const& pl = st.pledges;
    switch (pl.type())
    {
    case SCP_ST_PREPARE:
        break;
    case SCP_ST_CONFIRM:
    {
        auto const& c = pl.confirm();
        if (areBallotsCompatible(ballot, c.ballot))
        {
            res = c.nCommit <= check.first && check.second <= c.nH;
        }
    }
    break;
    case SCP_ST_EXTERNALIZE:
    {
        auto const& e = pl.externalize();
        if (areBallotsCompatible(ballot, e.commit))
        {
            res = e.commit.counter <= check.first;
        }
    }
    break;
    default:
        dbgAbort();
    }
    return res;
}

bool
BallotProtocol::setPreparedConfirmed(SCPBallot const& newC,
                                     SCPBallot const& newH)
{
    if (Logging::logTrace("SCP"))
        CLOG(TRACE, "SCP") << "BallotProtocol::setPreparedConfirmed"
                           << " i: " << mSlot.getSlotIndex()
                           << " h: " << mSlot.getSCP().ballotToStr(newH);

    bool didWork = false;

        mValueOverride = std::make_unique<Value>(newH.value);

        if (!mCurrentBallot || areBallotsCompatible(*mCurrentBallot, newH))
    {
        if (!mHighBallot || compareBallots(newH, *mHighBallot) > 0)
        {
            didWork = true;
            mHighBallot = std::make_unique<SCPBallot>(newH);
        }

        if (newC.counter != 0)
        {
            dbgAssert(!mCommit);
            mCommit = std::make_unique<SCPBallot>(newC);
            didWork = true;
        }

        if (didWork)
        {
            mSlot.getSCPDriver().confirmedBallotPrepared(mSlot.getSlotIndex(),
                                                         newH);
        }
    }

        didWork = updateCurrentIfNeeded(newH) || didWork;

    if (didWork)
    {
        emitCurrentStateStatement();
    }

    return didWork;
}

void
BallotProtocol::findExtendedInterval(Interval& candidate,
                                     std::set<uint32> const& boundaries,
                                     std::function<bool(Interval const&)> pred)
{
        for (auto it = boundaries.rbegin(); it != boundaries.rend(); it++)
    {
        uint32 b = *it;

        Interval cur;
        if (candidate.first == 0)
        {
                        cur = Interval(b, b);
        }
        else if (b > candidate.second) // invalid
        {
            continue;
        }
        else
        {
            cur.first = b;
            cur.second = candidate.second;
        }

        if (pred(cur))
        {
            candidate = cur;
        }
        else if (candidate.first != 0)
        {
                        break;
        }
    }
}

std::set<uint32>
BallotProtocol::getCommitBoundariesFromStatements(SCPBallot const& ballot)
{
    std::set<uint32> res;
    for (auto const& env : mLatestEnvelopes)
    {
        auto const& pl = env.second.statement.pledges;
        switch (pl.type())
        {
        case SCP_ST_PREPARE:
        {
            auto const& p = pl.prepare();
            if (areBallotsCompatible(ballot, p.ballot))
            {
                if (p.nC)
                {
                    res.emplace(p.nC);
                    res.emplace(p.nH);
                }
            }
        }
        break;
        case SCP_ST_CONFIRM:
        {
            auto const& c = pl.confirm();
            if (areBallotsCompatible(ballot, c.ballot))
            {
                res.emplace(c.nCommit);
                res.emplace(c.nH);
            }
        }
        break;
        case SCP_ST_EXTERNALIZE:
        {
            auto const& e = pl.externalize();
            if (areBallotsCompatible(ballot, e.commit))
            {
                res.emplace(e.commit.counter);
                res.emplace(e.nH);
                res.emplace(UINT32_MAX);
            }
        }
        break;
        default:
            dbgAbort();
        }
    }
    return res;
}

bool
BallotProtocol::attemptAcceptCommit(SCPStatement const& hint)
{
    if (mPhase != SCP_PHASE_PREPARE && mPhase != SCP_PHASE_CONFIRM)
    {
        return false;
    }

                SCPBallot ballot;
    switch (hint.pledges.type())
    {
    case SCPStatementType::SCP_ST_PREPARE:
    {
        auto const& prep = hint.pledges.prepare();
        if (prep.nC != 0)
        {
            ballot = SCPBallot(prep.nH, prep.ballot.value);
        }
        else
        {
            return false;
        }
    }
    break;
    case SCPStatementType::SCP_ST_CONFIRM:
    {
        auto const& con = hint.pledges.confirm();
        ballot = SCPBallot(con.nH, con.ballot.value);
    }
    break;
    case SCPStatementType::SCP_ST_EXTERNALIZE:
    {
        auto const& ext = hint.pledges.externalize();
        ballot = SCPBallot(ext.nH, ext.commit.value);
        break;
    }
    default:
        abort();
    };

    if (mPhase == SCP_PHASE_CONFIRM)
    {
        if (!areBallotsCompatible(ballot, *mHighBallot))
        {
            return false;
        }
    }

    auto pred = [&ballot, this](Interval const& cur) -> bool {
        return federatedAccept(
            [&](SCPStatement const& st) -> bool {
                bool res = false;
                auto const& pl = st.pledges;
                switch (pl.type())
                {
                case SCP_ST_PREPARE:
                {
                    auto const& p = pl.prepare();
                    if (areBallotsCompatible(ballot, p.ballot))
                    {
                        if (p.nC != 0)
                        {
                            res = p.nC <= cur.first && cur.second <= p.nH;
                        }
                    }
                }
                break;
                case SCP_ST_CONFIRM:
                {
                    auto const& c = pl.confirm();
                    if (areBallotsCompatible(ballot, c.ballot))
                    {
                        res = c.nCommit <= cur.first;
                    }
                }
                break;
                case SCP_ST_EXTERNALIZE:
                {
                    auto const& e = pl.externalize();
                    if (areBallotsCompatible(ballot, e.commit))
                    {
                        res = e.commit.counter <= cur.first;
                    }
                }
                break;
                default:
                    dbgAbort();
                }
                return res;
            },
            std::bind(&BallotProtocol::commitPredicate, ballot, cur, _1));
    };

        std::set<uint32> boundaries = getCommitBoundariesFromStatements(ballot);

    if (boundaries.empty())
    {
        return false;
    }

        Interval candidate;

    findExtendedInterval(candidate, boundaries, pred);

    bool res = false;

    if (candidate.first != 0)
    {
        if (mPhase != SCP_PHASE_CONFIRM ||
            candidate.second > mHighBallot->counter)
        {
            SCPBallot c = SCPBallot(candidate.first, ballot.value);
            SCPBallot h = SCPBallot(candidate.second, ballot.value);
            res = setAcceptCommit(c, h);
        }
    }

    return res;
}

bool
BallotProtocol::setAcceptCommit(SCPBallot const& c, SCPBallot const& h)
{
    if (Logging::logTrace("SCP"))
        CLOG(TRACE, "SCP") << "BallotProtocol::setAcceptCommit"
                           << " i: " << mSlot.getSlotIndex()
                           << " new c: " << mSlot.getSCP().ballotToStr(c)
                           << " new h: " << mSlot.getSCP().ballotToStr(h);

    bool didWork = false;

        mValueOverride = std::make_unique<Value>(h.value);

    if (!mHighBallot || !mCommit || compareBallots(*mHighBallot, h) != 0 ||
        compareBallots(*mCommit, c) != 0)
    {
        mCommit = std::make_unique<SCPBallot>(c);
        mHighBallot = std::make_unique<SCPBallot>(h);

        didWork = true;
    }

    if (mPhase == SCP_PHASE_PREPARE)
    {
        mPhase = SCP_PHASE_CONFIRM;
        if (mCurrentBallot && !areBallotsLessAndCompatible(h, *mCurrentBallot))
        {
            bumpToBallot(h, false);
        }
        mPreparedPrime.reset();

        didWork = true;
    }

    if (didWork)
    {
        updateCurrentIfNeeded(*mHighBallot);

        mSlot.getSCPDriver().acceptedCommit(mSlot.getSlotIndex(), h);
        emitCurrentStateStatement();
    }

    return didWork;
}

static uint32
statementBallotCounter(SCPStatement const& st)
{
    switch (st.pledges.type())
    {
    case SCP_ST_PREPARE:
        return st.pledges.prepare().ballot.counter;
    case SCP_ST_CONFIRM:
        return st.pledges.confirm().ballot.counter;
    case SCP_ST_EXTERNALIZE:
        return UINT32_MAX;
    default:
                abort();
    }
}

static bool
hasVBlockingSubsetStrictlyAheadOf(std::shared_ptr<LocalNode> localNode,
                                  std::map<NodeID, SCPEnvelope> const& map,
                                  uint32_t n)
{
    return LocalNode::isVBlocking(
        localNode->getQuorumSet(), map,
        [&](SCPStatement const& st) { return statementBallotCounter(st) > n; });
}


bool
BallotProtocol::attemptBump()
{
    if (mPhase == SCP_PHASE_PREPARE || mPhase == SCP_PHASE_CONFIRM)
    {

                                auto localNode = getLocalNode();
        uint32 localCounter = mCurrentBallot ? mCurrentBallot->counter : 0;
        if (!hasVBlockingSubsetStrictlyAheadOf(localNode, mLatestEnvelopes,
                                               localCounter))
        {
            return false;
        }

                std::set<uint32> allCounters;
        for (auto const& e : mLatestEnvelopes)
        {
            uint32_t c = statementBallotCounter(e.second.statement);
            if (c > localCounter)
                allCounters.insert(c);
        }

                                        for (uint32_t n : allCounters)
        {
            if (!hasVBlockingSubsetStrictlyAheadOf(localNode, mLatestEnvelopes,
                                                   n))
            {
                                return abandonBallot(n);
            }
        }
    }
    return false;
}

bool
BallotProtocol::attemptConfirmCommit(SCPStatement const& hint)
{
    if (mPhase != SCP_PHASE_CONFIRM)
    {
        return false;
    }

    if (!mHighBallot || !mCommit)
    {
        return false;
    }

            SCPBallot ballot;
    switch (hint.pledges.type())
    {
    case SCPStatementType::SCP_ST_PREPARE:
    {
        return false;
    }
    break;
    case SCPStatementType::SCP_ST_CONFIRM:
    {
        auto const& con = hint.pledges.confirm();
        ballot = SCPBallot(con.nH, con.ballot.value);
    }
    break;
    case SCPStatementType::SCP_ST_EXTERNALIZE:
    {
        auto const& ext = hint.pledges.externalize();
        ballot = SCPBallot(ext.nH, ext.commit.value);
        break;
    }
    default:
        abort();
    };

    if (!areBallotsCompatible(ballot, *mCommit))
    {
        return false;
    }

    std::set<uint32> boundaries = getCommitBoundariesFromStatements(ballot);
    Interval candidate;

    auto pred = [&ballot, this](Interval const& cur) -> bool {
        return federatedRatify(
            std::bind(&BallotProtocol::commitPredicate, ballot, cur, _1));
    };

    findExtendedInterval(candidate, boundaries, pred);

    bool res = candidate.first != 0;
    if (res)
    {
        SCPBallot c = SCPBallot(candidate.first, ballot.value);
        SCPBallot h = SCPBallot(candidate.second, ballot.value);
        return setConfirmCommit(c, h);
    }
    return res;
}

bool
BallotProtocol::setConfirmCommit(SCPBallot const& c, SCPBallot const& h)
{
    if (Logging::logTrace("SCP"))
        CLOG(TRACE, "SCP") << "BallotProtocol::setConfirmCommit"
                           << " i: " << mSlot.getSlotIndex()
                           << " new c: " << mSlot.getSCP().ballotToStr(c)
                           << " new h: " << mSlot.getSCP().ballotToStr(h);

    mCommit = std::make_unique<SCPBallot>(c);
    mHighBallot = std::make_unique<SCPBallot>(h);
    updateCurrentIfNeeded(*mHighBallot);

    mPhase = SCP_PHASE_EXTERNALIZE;

    emitCurrentStateStatement();

    mSlot.stopNomination();

    mSlot.getSCPDriver().valueExternalized(mSlot.getSlotIndex(),
                                           mCommit->value);

    return true;
}

bool
BallotProtocol::hasPreparedBallot(SCPBallot const& ballot,
                                  SCPStatement const& st)
{
    bool res;

    switch (st.pledges.type())
    {
    case SCP_ST_PREPARE:
    {
        auto const& p = st.pledges.prepare();
        res =
            (p.prepared && areBallotsLessAndCompatible(ballot, *p.prepared)) ||
            (p.preparedPrime &&
             areBallotsLessAndCompatible(ballot, *p.preparedPrime));
    }
    break;
    case SCP_ST_CONFIRM:
    {
        auto const& c = st.pledges.confirm();
        SCPBallot prepared(c.nPrepared, c.ballot.value);
        res = areBallotsLessAndCompatible(ballot, prepared);
    }
    break;
    case SCP_ST_EXTERNALIZE:
    {
        auto const& e = st.pledges.externalize();
        res = areBallotsCompatible(ballot, e.commit);
    }
    break;
    default:
        res = false;
        dbgAbort();
    }

    return res;
}

Hash
BallotProtocol::getCompanionQuorumSetHashFromStatement(SCPStatement const& st)
{
    Hash h;
    switch (st.pledges.type())
    {
    case SCP_ST_PREPARE:
        h = st.pledges.prepare().quorumSetHash;
        break;
    case SCP_ST_CONFIRM:
        h = st.pledges.confirm().quorumSetHash;
        break;
    case SCP_ST_EXTERNALIZE:
        h = st.pledges.externalize().commitQuorumSetHash;
        break;
    default:
        dbgAbort();
    }
    return h;
}

SCPBallot
BallotProtocol::getWorkingBallot(SCPStatement const& st)
{
    SCPBallot res;
    switch (st.pledges.type())
    {
    case SCP_ST_PREPARE:
        res = st.pledges.prepare().ballot;
        break;
    case SCP_ST_CONFIRM:
    {
        auto const& con = st.pledges.confirm();
        res = SCPBallot(con.nCommit, con.ballot.value);
    }
    break;
    case SCP_ST_EXTERNALIZE:
        res = st.pledges.externalize().commit;
        break;
    default:
        dbgAbort();
    }
    return res;
}

bool
BallotProtocol::setPrepared(SCPBallot const& ballot)
{
    bool didWork = false;

        if (mPrepared)
    {
        int comp = compareBallots(*mPrepared, ballot);
        if (comp < 0)
        {
                        if (!areBallotsCompatible(*mPrepared, ballot))
            {
                mPreparedPrime = std::make_unique<SCPBallot>(*mPrepared);
            }
            mPrepared = std::make_unique<SCPBallot>(ballot);
            didWork = true;
        }
        else if (comp > 0)
        {
                                                                        
            if (!mPreparedPrime ||
                ((compareBallots(*mPreparedPrime, ballot) < 0) &&
                 !areBallotsCompatible(*mPrepared, ballot)))
            {
                mPreparedPrime = std::make_unique<SCPBallot>(ballot);
                didWork = true;
            }
        }
    }
    else
    {
        mPrepared = std::make_unique<SCPBallot>(ballot);
        didWork = true;
    }
    return didWork;
}

int
BallotProtocol::compareBallots(std::unique_ptr<SCPBallot> const& b1,
                               std::unique_ptr<SCPBallot> const& b2)
{
    int res;
    if (b1 && b2)
    {
        res = compareBallots(*b1, *b2);
    }
    else if (b1 && !b2)
    {
        res = 1;
    }
    else if (!b1 && b2)
    {
        res = -1;
    }
    else
    {
        res = 0;
    }
    return res;
}

int
BallotProtocol::compareBallots(SCPBallot const& b1, SCPBallot const& b2)
{
    if (b1.counter < b2.counter)
    {
        return -1;
    }
    else if (b2.counter < b1.counter)
    {
        return 1;
    }
        if (b1.value < b2.value)
    {
        return -1;
    }
    else if (b2.value < b1.value)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

bool
BallotProtocol::areBallotsCompatible(SCPBallot const& b1, SCPBallot const& b2)
{
    return b1.value == b2.value;
}

bool
BallotProtocol::areBallotsLessAndIncompatible(SCPBallot const& b1,
                                              SCPBallot const& b2)
{
    return (compareBallots(b1, b2) <= 0) && !areBallotsCompatible(b1, b2);
}

bool
BallotProtocol::areBallotsLessAndCompatible(SCPBallot const& b1,
                                            SCPBallot const& b2)
{
    return (compareBallots(b1, b2) <= 0) && areBallotsCompatible(b1, b2);
}

void
BallotProtocol::setStateFromEnvelope(SCPEnvelope const& e)
{
    if (mCurrentBallot)
    {
        throw std::runtime_error(
            "Cannot set state after starting ballot protocol");
    }

    recordEnvelope(e);

    mLastEnvelope = std::make_shared<SCPEnvelope>(e);
    mLastEnvelopeEmit = mLastEnvelope;

    auto const& pl = e.statement.pledges;

    switch (pl.type())
    {
    case SCPStatementType::SCP_ST_PREPARE:
    {
        auto const& prep = pl.prepare();
        auto const& b = prep.ballot;
        bumpToBallot(b, true);
        if (prep.prepared)
        {
            mPrepared = std::make_unique<SCPBallot>(*prep.prepared);
        }
        if (prep.preparedPrime)
        {
            mPreparedPrime = std::make_unique<SCPBallot>(*prep.preparedPrime);
        }
        if (prep.nH)
        {
            mHighBallot = std::make_unique<SCPBallot>(prep.nH, b.value);
        }
        if (prep.nC)
        {
            mCommit = std::make_unique<SCPBallot>(prep.nC, b.value);
        }
        mPhase = SCP_PHASE_PREPARE;
    }
    break;
    case SCPStatementType::SCP_ST_CONFIRM:
    {
        auto const& c = pl.confirm();
        auto const& v = c.ballot.value;
        bumpToBallot(c.ballot, true);
        mPrepared = std::make_unique<SCPBallot>(c.nPrepared, v);
        mHighBallot = std::make_unique<SCPBallot>(c.nH, v);
        mCommit = std::make_unique<SCPBallot>(c.nCommit, v);
        mPhase = SCP_PHASE_CONFIRM;
    }
    break;
    case SCPStatementType::SCP_ST_EXTERNALIZE:
    {
        auto const& ext = pl.externalize();
        auto const& v = ext.commit.value;
        bumpToBallot(SCPBallot(UINT32_MAX, v), true);
        mPrepared = std::make_unique<SCPBallot>(UINT32_MAX, v);
        mHighBallot = std::make_unique<SCPBallot>(ext.nH, v);
        mCommit = std::make_unique<SCPBallot>(ext.commit);
        mPhase = SCP_PHASE_EXTERNALIZE;
    }
    break;
    default:
        dbgAbort();
    }
}

std::vector<SCPEnvelope>
BallotProtocol::getCurrentState() const
{
    std::vector<SCPEnvelope> res;
    res.reserve(mLatestEnvelopes.size());
    for (auto const& n : mLatestEnvelopes)
    {
                if (!(n.first == mSlot.getSCP().getLocalNodeID()) ||
            mSlot.isFullyValidated())
        {
            res.emplace_back(n.second);
        }
    }
    return res;
}

SCPEnvelope const*
BallotProtocol::getLatestMessage(NodeID const& id) const
{
    auto it = mLatestEnvelopes.find(id);
    if (it != mLatestEnvelopes.end())
    {
        return &it->second;
    }
    return nullptr;
}

std::vector<SCPEnvelope>
BallotProtocol::getExternalizingState() const
{
    std::vector<SCPEnvelope> res;
    if (mPhase == SCP_PHASE_EXTERNALIZE)
    {
        res.reserve(mLatestEnvelopes.size());
        for (auto const& n : mLatestEnvelopes)
        {
            if (!(n.first == mSlot.getSCP().getLocalNodeID()))
            {
                                                                if (areBallotsCompatible(getWorkingBallot(n.second.statement),
                                         *mCommit))
                {
                    res.emplace_back(n.second);
                }
            }
            else if (mSlot.isFullyValidated())
            {
                                res.emplace_back(n.second);
            }
        }
    }
    return res;
}

void
BallotProtocol::advanceSlot(SCPStatement const& hint)
{
    mCurrentMessageLevel++;
    if (Logging::logTrace("SCP"))
        CLOG(TRACE, "SCP") << "BallotProtocol::advanceSlot "
                           << mCurrentMessageLevel << " " << getLocalState();

    if (mCurrentMessageLevel >= MAX_ADVANCE_SLOT_RECURSION)
    {
        throw std::runtime_error(
            "maximum number of transitions reached in advanceSlot");
    }

        
            
    bool didWork = false;

    didWork = attemptPreparedAccept(hint) || didWork;

    didWork = attemptPreparedConfirmed(hint) || didWork;

    didWork = attemptAcceptCommit(hint) || didWork;

    didWork = attemptConfirmCommit(hint) || didWork;

        if (mCurrentMessageLevel == 1)
    {
        bool didBump = false;
        do
        {
                        didBump = attemptBump();
            didWork = didBump || didWork;
        } while (didBump);

        checkHeardFromQuorum();
    }

    if (Logging::logTrace("SCP"))
        CLOG(TRACE, "SCP") << "BallotProtocol::advanceSlot "
                           << mCurrentMessageLevel << " - exiting "
                           << getLocalState();

    --mCurrentMessageLevel;

    if (didWork)
    {
        sendLatestEnvelope();
    }
}

SCPDriver::ValidationLevel
BallotProtocol::validateValues(SCPStatement const& st)
{
    std::set<Value> values;
    switch (st.pledges.type())
    {
    case SCPStatementType::SCP_ST_PREPARE:
    {
        auto const& prep = st.pledges.prepare();
        auto const& b = prep.ballot;
        if (b.counter != 0)
        {
            values.insert(prep.ballot.value);
        }
        if (prep.prepared)
        {
            values.insert(prep.prepared->value);
        }
    }
    break;
    case SCPStatementType::SCP_ST_CONFIRM:
        values.insert(st.pledges.confirm().ballot.value);
        break;
    case SCPStatementType::SCP_ST_EXTERNALIZE:
        values.insert(st.pledges.externalize().commit.value);
        break;
    default:
                return SCPDriver::kInvalidValue;
    }
    SCPDriver::ValidationLevel res = SCPDriver::kFullyValidatedValue;
    for (auto const& v : values)
    {
        auto tr =
            mSlot.getSCPDriver().validateValue(mSlot.getSlotIndex(), v, false);
        if (tr != SCPDriver::kFullyValidatedValue)
        {
            if (tr == SCPDriver::kInvalidValue)
            {
                res = SCPDriver::kInvalidValue;
            }
            else
            {
                res = SCPDriver::kMaybeValidValue;
            }
        }
    }
    return res;
}

void
BallotProtocol::sendLatestEnvelope()
{
        if (mCurrentMessageLevel == 0 && mLastEnvelope && mSlot.isFullyValidated())
    {
        if (!mLastEnvelopeEmit || mLastEnvelope != mLastEnvelopeEmit)
        {
            mLastEnvelopeEmit = mLastEnvelope;
            mSlot.getSCPDriver().emitEnvelope(*mLastEnvelopeEmit);
        }
    }
}

const char* BallotProtocol::phaseNames[SCP_PHASE_NUM] = {"PREPARE", "FINISH",
                                                         "EXTERNALIZE"};

Json::Value
BallotProtocol::getJsonInfo()
{
    Json::Value ret;
    ret["heard"] = mHeardFromQuorum;
    ret["ballot"] = mSlot.getSCP().ballotToStr(mCurrentBallot);
    ret["phase"] = phaseNames[mPhase];

    ret["state"] = getLocalState();
    return ret;
}

Json::Value
BallotProtocol::getJsonQuorumInfo(NodeID const& id, bool summary, bool fullKeys)
{
    Json::Value ret;
    auto& phase = ret["phase"];

        SCPBallot b;
    Hash qSetHash;

    auto stateit = mLatestEnvelopes.find(id);
    if (stateit == mLatestEnvelopes.end())
    {
        phase = "unknown";
        if (id == mSlot.getLocalNode()->getNodeID())
        {
            qSetHash = mSlot.getLocalNode()->getQuorumSetHash();
        }
    }
    else
    {
        auto const& st = stateit->second.statement;

        switch (st.pledges.type())
        {
        case SCPStatementType::SCP_ST_PREPARE:
            phase = "PREPARE";
            b = st.pledges.prepare().ballot;
            break;
        case SCPStatementType::SCP_ST_CONFIRM:
            phase = "CONFIRM";
            b = st.pledges.confirm().ballot;
            break;
        case SCPStatementType::SCP_ST_EXTERNALIZE:
            phase = "EXTERNALIZE";
            b = st.pledges.externalize().commit;
            break;
        default:
            dbgAbort();
        }
                        qSetHash = mSlot.getCompanionQuorumSetHashFromStatement(st);
    }

    Json::Value& disagree = ret["disagree"];
    Json::Value& missing = ret["missing"];
    Json::Value& delayed = ret["delayed"];

    int n_missing = 0, n_disagree = 0, n_delayed = 0;

    int agree = 0;
    auto qSet = mSlot.getSCPDriver().getQSet(qSetHash);
    if (!qSet)
    {
        phase = "expired";
        return ret;
    }
    LocalNode::forAllNodes(*qSet, [&](NodeID const& n) {
        auto it = mLatestEnvelopes.find(n);
        if (it == mLatestEnvelopes.end())
        {
            if (!summary)
            {
                missing.append(mSlot.getSCPDriver().toStrKey(n, fullKeys));
            }
            n_missing++;
        }
        else
        {
            auto& st = it->second.statement;
            if (areBallotsCompatible(getWorkingBallot(st), b))
            {
                agree++;
                auto t = st.pledges.type();
                if (!(t == SCPStatementType::SCP_ST_EXTERNALIZE ||
                      (t == SCPStatementType::SCP_ST_CONFIRM &&
                       st.pledges.confirm().ballot.counter == UINT32_MAX)))
                {
                    if (!summary)
                    {
                        delayed.append(
                            mSlot.getSCPDriver().toStrKey(n, fullKeys));
                    }
                    n_delayed++;
                }
            }
            else
            {
                if (!summary)
                {
                    disagree.append(mSlot.getSCPDriver().toStrKey(n, fullKeys));
                }
                n_disagree++;
            }
        }
    });
    if (summary)
    {
        missing = n_missing;
        disagree = n_disagree;
        delayed = n_delayed;
    }

    auto f = LocalNode::findClosestVBlocking(*qSet, mLatestEnvelopes,
                                             [&](SCPStatement const& st) {
                                                 return areBallotsCompatible(
                                                     getWorkingBallot(st), b);
                                             },
                                             &id);
    ret["fail_at"] = static_cast<int>(f.size());

    if (!summary)
    {
        auto& f_ex = ret["fail_with"];
        for (auto const& n : f)
        {
            f_ex.append(mSlot.getSCPDriver().toStrKey(n, fullKeys));
        }
        ret["value"] = getLocalNode()->toJson(*qSet, fullKeys);
    }

    ret["hash"] = hexAbbrev(qSetHash);
    ret["agree"] = agree;

    return ret;
}

std::string
BallotProtocol::getLocalState() const
{
    std::ostringstream oss;

    oss << "i: " << mSlot.getSlotIndex() << " | " << phaseNames[mPhase]
        << " | b: " << mSlot.getSCP().ballotToStr(mCurrentBallot)
        << " | p: " << mSlot.getSCP().ballotToStr(mPrepared)
        << " | p': " << mSlot.getSCP().ballotToStr(mPreparedPrime)
        << " | h: " << mSlot.getSCP().ballotToStr(mHighBallot)
        << " | c: " << mSlot.getSCP().ballotToStr(mCommit)
        << " | M: " << mLatestEnvelopes.size();
    return oss.str();
}

std::shared_ptr<LocalNode>
BallotProtocol::getLocalNode()
{
    return mSlot.getSCP().getLocalNode();
}

bool
BallotProtocol::federatedAccept(StatementPredicate voted,
                                StatementPredicate accepted)
{
    return mSlot.federatedAccept(voted, accepted, mLatestEnvelopes);
}

bool
BallotProtocol::federatedRatify(StatementPredicate voted)
{
    return mSlot.federatedRatify(voted, mLatestEnvelopes);
}

void
BallotProtocol::checkHeardFromQuorum()
{
                            if (mCurrentBallot)
    {
        if (LocalNode::isQuorum(
                getLocalNode()->getQuorumSet(), mLatestEnvelopes,
                std::bind(&Slot::getQuorumSetFromStatement, &mSlot, _1),
                [&](SCPStatement const& st) {
                    bool res;
                    if (st.pledges.type() == SCP_ST_PREPARE)
                    {
                        res = mCurrentBallot->counter <=
                              st.pledges.prepare().ballot.counter;
                    }
                    else
                    {
                        res = true;
                    }
                    return res;
                }))
        {
            bool oldHQ = mHeardFromQuorum;
            mHeardFromQuorum = true;
            if (!oldHQ)
            {
                                mSlot.getSCPDriver().ballotDidHearFromQuorum(
                    mSlot.getSlotIndex(), *mCurrentBallot);
                if (mPhase != SCP_PHASE_EXTERNALIZE)
                {
                    startBallotProtocolTimer();
                }
            }
            if (mPhase == SCP_PHASE_EXTERNALIZE)
            {
                stopBallotProtocolTimer();
            }
        }
        else
        {
            mHeardFromQuorum = false;
            stopBallotProtocolTimer();
        }
    }
}
}

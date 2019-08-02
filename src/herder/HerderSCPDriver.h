#pragma once


#include "herder/Herder.h"
#include "herder/TxSetFrame.h"
#include "scp/SCPDriver.h"
#include "xdr/vii-ledger.h"

namespace medida
{
class Counter;
class Meter;
class Timer;
class Histogram;
}

namespace viichain
{
class Application;
class HerderImpl;
class LedgerManager;
class PendingEnvelopes;
class SCP;
class Upgrades;
class VirtualTimer;
struct StellarValue;
struct SCPEnvelope;

class HerderSCPDriver : public SCPDriver
{
  public:
    struct ConsensusData
    {
        uint64_t mConsensusIndex;
        StellarValue mConsensusValue;
        ConsensusData(uint64_t index, StellarValue const& b)
            : mConsensusIndex(index), mConsensusValue(b)
        {
        }
    };

    HerderSCPDriver(Application& app, HerderImpl& herder,
                    Upgrades const& upgrades,
                    PendingEnvelopes& pendingEnvelopes);
    ~HerderSCPDriver();

    void bootstrap();
    void lostSync();

    Herder::State getState() const;

    ConsensusData*
    trackingSCP() const
    {
        return mTrackingSCP.get();
    }
    ConsensusData*
    lastTrackingSCP() const
    {
        return mLastTrackingSCP.get();
    }

    void restoreSCPState(uint64_t index, StellarValue const& value);

        uint32
    lastConsensusLedgerIndex() const
    {
        assert(mTrackingSCP->mConsensusIndex <= UINT32_MAX);
        return static_cast<uint32>(mTrackingSCP->mConsensusIndex);
    }

        uint32
    nextConsensusLedgerIndex() const
    {
        return lastConsensusLedgerIndex() + 1;
    }

    SCP&
    getSCP()
    {
        return mSCP;
    }

    void recordSCPExecutionMetrics(uint64_t slotIndex);
    void recordSCPEvent(uint64_t slotIndex, bool isNomination);

        void signEnvelope(SCPEnvelope& envelope) override;
    void emitEnvelope(SCPEnvelope const& envelope) override;

        SCPDriver::ValidationLevel validateValue(uint64_t slotIndex,
                                             Value const& value,
                                             bool nomination) override;
    Value extractValidValue(uint64_t slotIndex, Value const& value) override;

        std::string toShortString(PublicKey const& pk) const override;
    std::string getValueString(Value const& v) const override;

        void setupTimer(uint64_t slotIndex, int timerID,
                    std::chrono::milliseconds timeout,
                    std::function<void()> cb) override;

        Value combineCandidates(uint64_t slotIndex,
                            std::set<Value> const& candidates) override;
    void valueExternalized(uint64_t slotIndex, Value const& value) override;

            void nominate(uint64_t slotIndex, StellarValue const& value,
                  TxSetFramePtr proposedSet, StellarValue const& previousValue);

    SCPQuorumSetPtr getQSet(Hash const& qSetHash) override;

        void ballotDidHearFromQuorum(uint64_t slotIndex,
                                 SCPBallot const& ballot) override;
    void nominatingValue(uint64_t slotIndex, Value const& value) override;
    void updatedCandidateValue(uint64_t slotIndex, Value const& value) override;
    void startedBallotProtocol(uint64_t slotIndex,
                               SCPBallot const& ballot) override;
    void acceptedBallotPrepared(uint64_t slotIndex,
                                SCPBallot const& ballot) override;
    void confirmedBallotPrepared(uint64_t slotIndex,
                                 SCPBallot const& ballot) override;
    void acceptedCommit(uint64_t slotIndex, SCPBallot const& ballot) override;

    optional<VirtualClock::time_point> getPrepareStart(uint64_t slotIndex);

            bool toStellarValue(Value const& v, StellarValue& sv);

        bool checkCloseTime(uint64_t slotIndex, uint64_t lastCloseTime,
                        StellarValue const& b) const;

  private:
    Application& mApp;
    HerderImpl& mHerder;
    LedgerManager& mLedgerManager;
    Upgrades const& mUpgrades;
    PendingEnvelopes& mPendingEnvelopes;
    SCP mSCP;

    struct SCPMetrics
    {
        medida::Meter& mEnvelopeSign;

        medida::Meter& mValueValid;
        medida::Meter& mValueInvalid;

                medida::Meter& mCombinedCandidates;

                medida::Timer& mNominateToPrepare;
        medida::Timer& mPrepareToExternalize;

        SCPMetrics(Application& app);
    };

    SCPMetrics mSCPMetrics;

        medida::Histogram& mNominateTimeout;
        medida::Histogram& mPrepareTimeout;

    struct SCPTiming
    {
        optional<VirtualClock::time_point> mNominationStart;
        optional<VirtualClock::time_point> mPrepareStart;

                int64_t mNominationTimeoutCount{0};
                int64_t mPrepareTimeoutCount{0};
    };

                std::map<uint64_t, SCPTiming> mSCPExecutionTimes;

    uint32_t mLedgerSeqNominating;
    Value mCurrentValue;

            std::map<uint64_t, std::map<int, std::unique_ptr<VirtualTimer>>> mSCPTimers;

                        std::unique_ptr<ConsensusData> mTrackingSCP;

                std::unique_ptr<ConsensusData> mLastTrackingSCP;

    void stateChanged();

    SCPDriver::ValidationLevel validateValueHelper(uint64_t slotIndex,
                                                   StellarValue const& sv,
                                                   bool nomination) const;

            bool isSlotCompatibleWithCurrentState(uint64_t slotIndex) const;

    void logQuorumInformation(uint64_t index);

    void clearSCPExecutionEvents();

    void timerCallbackWrapper(uint64_t slotIndex, int timerID,
                              std::function<void()> cb);
};
}

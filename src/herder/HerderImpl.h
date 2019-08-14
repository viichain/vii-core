#pragma once


#include "herder/Herder.h"
#include "herder/HerderSCPDriver.h"
#include "herder/PendingEnvelopes.h"
#include "herder/TransactionQueue.h"
#include "herder/Upgrades.h"
#include "util/Timer.h"
#include "util/XDROperators.h"
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

namespace medida
{
class Meter;
class Counter;
class Timer;
}

namespace viichain
{
class Application;
class LedgerManager;
class HerderSCPDriver;

class HerderImpl : public Herder
{
  public:
    HerderImpl(Application& app);
    ~HerderImpl();

    State getState() const override;
    std::string getStateHuman() const override;

    void syncMetrics() override;

        void bootstrap() override;

    void restoreState() override;

    SCP& getSCP();
    HerderSCPDriver&
    getHerderSCPDriver()
    {
        return mHerderSCPDriver;
    }

    void valueExternalized(uint64 slotIndex, VIIValue const& value);
    void emitEnvelope(SCPEnvelope const& envelope);

    TransactionQueue::AddResult
    recvTransaction(TransactionFramePtr tx) override;

    EnvelopeStatus recvSCPEnvelope(SCPEnvelope const& envelope) override;
    EnvelopeStatus recvSCPEnvelope(SCPEnvelope const& envelope,
                                   const SCPQuorumSet& qset,
                                   TxSetFrame txset) override;

    void sendSCPStateToPeer(uint32 ledgerSeq, Peer::pointer peer) override;

    bool recvSCPQuorumSet(Hash const& hash, const SCPQuorumSet& qset) override;
    bool recvTxSet(Hash const& hash, const TxSetFrame& txset) override;
    void peerDoesntHave(MessageType type, uint256 const& itemID,
                        Peer::pointer peer) override;
    TxSetFramePtr getTxSet(Hash const& hash) override;
    SCPQuorumSetPtr getQSet(Hash const& qSetHash) override;

    void processSCPQueue();

    uint32_t getCurrentLedgerSeq() const override;

    SequenceNumber getMaxSeqInPendingTxs(AccountID const&) override;

    void triggerNextLedger(uint32_t ledgerSeqToTrigger) override;

    void setUpgrades(Upgrades::UpgradeParameters const& upgrades) override;
    std::string getUpgradesJson() override;

    bool resolveNodeID(std::string const& s, PublicKey& retKey) override;

    Json::Value getJsonInfo(size_t limit, bool fullKeys = false) override;
    Json::Value getJsonQuorumInfo(NodeID const& id, bool summary, bool fullKeys,
                                  uint64 index) override;
    Json::Value getJsonTransitiveQuorumIntersectionInfo(bool fullKeys) const;
    virtual Json::Value getJsonTransitiveQuorumInfo(NodeID const& id,
                                                    bool summary,
                                                    bool fullKeys) override;
    QuorumTracker::QuorumMap const& getCurrentlyTrackedQuorum() const override;

#ifdef BUILD_TESTS
        PendingEnvelopes& getPendingEnvelopes();
#endif

        bool verifyEnvelope(SCPEnvelope const& envelope);
        void signEnvelope(SecretKey const& s, SCPEnvelope& envelope);

        bool verifyVIIValueSignature(VIIValue const& sv);
        void signVIIValue(SecretKey const& s, VIIValue& sv);

  private:
                bool checkCloseTime(SCPEnvelope const& envelope, bool enforceRecent);

    void ledgerClosed();

    void startRebroadcastTimer();
    void rebroadcast();
    void broadcast(SCPEnvelope const& e);

    void processSCPQueueUpToIndex(uint64 slotIndex);

    TransactionQueue mTransactionQueue;

    void
    updateTransactionQueue(std::vector<TransactionFramePtr> const& applied);

    PendingEnvelopes mPendingEnvelopes;
    Upgrades mUpgrades;
    HerderSCPDriver mHerderSCPDriver;

    void herderOutOfSync();

        void getMoreSCPState();

            uint64 mLastSlotSaved;

        VirtualTimer mTrackingTimer;

        VirtualClock::time_point mLastExternalize;

        void persistSCPState(uint64 slot);
        void restoreSCPState();

        void persistUpgrades();
    void restoreUpgrades();

                void trackingHeartBeat();

    VirtualTimer mTriggerTimer;

    VirtualTimer mRebroadcastTimer;

    Application& mApp;
    LedgerManager& mLedgerManager;

    struct SCPMetrics
    {
        medida::Meter& mLostSync;

        medida::Meter& mEnvelopeEmit;
        medida::Meter& mEnvelopeReceive;

                        medida::Counter& mCumulativeStatements;

                medida::Meter& mEnvelopeValidSig;
        medida::Meter& mEnvelopeInvalidSig;

        SCPMetrics(Application& app);
    };

    SCPMetrics mSCPMetrics;

            void checkAndMaybeReanalyzeQuorumMap();

    struct QuorumMapIntersectionState
    {
        uint32_t mLastCheckLedger{0};
        uint32_t mLastGoodLedger{0};
        size_t mNumNodes{0};
        Hash mLastCheckQuorumMapHash{};
        bool mRecalculating{false};
        std::pair<std::vector<PublicKey>, std::vector<PublicKey>>
            mPotentialSplit{};

        bool
        hasAnyResults() const
        {
            return mLastGoodLedger != 0;
        }

        bool
        enjoysQuorunIntersection() const
        {
            return mLastCheckLedger == mLastGoodLedger;
        }
    };
    QuorumMapIntersectionState mLastQuorumMapIntersectionState;
};
}

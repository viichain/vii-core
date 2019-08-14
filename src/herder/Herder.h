#pragma once


#include "TxSetFrame.h"
#include "Upgrades.h"
#include "herder/QuorumTracker.h"
#include "herder/TransactionQueue.h"
#include "lib/json/json-forwards.h"
#include "overlay/Peer.h"
#include "overlay/VIIXDR.h"
#include "scp/SCP.h"
#include "util/Timer.h"
#include <functional>
#include <memory>
#include <string>

namespace viichain
{
class Application;
class XDROutputFileStream;

class Herder
{
  public:
        static std::chrono::seconds const EXP_LEDGER_TIMESPAN_SECONDS;

        static std::chrono::seconds const MAX_SCP_TIMEOUT_SECONDS;

        static std::chrono::seconds const CONSENSUS_STUCK_TIMEOUT_SECONDS;

        static std::chrono::seconds const MAX_TIME_SLIP_SECONDS;

        static std::chrono::seconds const NODE_EXPIRATION_SECONDS;

        static uint32 const LEDGER_VALIDITY_BRACKET;

        static uint32 const MAX_SLOTS_TO_REMEMBER;

        static std::chrono::nanoseconds const TIMERS_THRESHOLD_NANOSEC;

    static std::unique_ptr<Herder> create(Application& app);

    enum State
    {
        HERDER_SYNCING_STATE,
        HERDER_TRACKING_STATE,
        HERDER_NUM_STATE
    };

    enum EnvelopeStatus
    {
                        ENVELOPE_STATUS_DISCARDED,
                ENVELOPE_STATUS_SKIPPED_SELF,
                ENVELOPE_STATUS_FETCHING,
                        ENVELOPE_STATUS_READY,
                ENVELOPE_STATUS_PROCESSED,
    };

    virtual State getState() const = 0;
    virtual std::string getStateHuman() const = 0;

            virtual void syncMetrics() = 0;

    virtual void bootstrap() = 0;

        virtual void restoreState() = 0;

    virtual bool recvSCPQuorumSet(Hash const& hash,
                                  SCPQuorumSet const& qset) = 0;
    virtual bool recvTxSet(Hash const& hash, TxSetFrame const& txset) = 0;
        virtual TransactionQueue::AddResult
    recvTransaction(TransactionFramePtr tx) = 0;
    virtual void peerDoesntHave(viichain::MessageType type,
                                uint256 const& itemID, Peer::pointer peer) = 0;
    virtual TxSetFramePtr getTxSet(Hash const& hash) = 0;
    virtual SCPQuorumSetPtr getQSet(Hash const& qSetHash) = 0;

        virtual EnvelopeStatus recvSCPEnvelope(SCPEnvelope const& envelope) = 0;

        virtual EnvelopeStatus recvSCPEnvelope(SCPEnvelope const& envelope,
                                           const SCPQuorumSet& qset,
                                           TxSetFrame txset) = 0;

        virtual void sendSCPStateToPeer(uint32 ledgerSeq, Peer::pointer peer) = 0;

            virtual uint32_t getCurrentLedgerSeq() const = 0;

            virtual SequenceNumber getMaxSeqInPendingTxs(AccountID const&) = 0;

    virtual void triggerNextLedger(uint32_t ledgerSeqToTrigger) = 0;

        virtual bool resolveNodeID(std::string const& s, PublicKey& retKey) = 0;

        virtual void setUpgrades(Upgrades::UpgradeParameters const& upgrades) = 0;
        virtual std::string getUpgradesJson() = 0;

    virtual ~Herder()
    {
    }

    virtual Json::Value getJsonInfo(size_t limit, bool fullKeys = false) = 0;
    virtual Json::Value getJsonQuorumInfo(NodeID const& id, bool summary,
                                          bool fullKeys, uint64 index) = 0;
    virtual Json::Value getJsonTransitiveQuorumInfo(NodeID const& id,
                                                    bool summary,
                                                    bool fullKeys) = 0;
    virtual QuorumTracker::QuorumMap const&
    getCurrentlyTrackedQuorum() const = 0;
};
}

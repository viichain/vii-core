#pragma once


#include "BallotProtocol.h"
#include "LocalNode.h"
#include "NominationProtocol.h"
#include "lib/json/json-forwards.h"
#include "scp/SCP.h"
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace viichain
{
class Node;

class Slot : public std::enable_shared_from_this<Slot>
{
    const uint64 mSlotIndex; // the index this slot is tracking
    SCP& mSCP;

    BallotProtocol mBallotProtocol;
    NominationProtocol mNominationProtocol;

            struct HistoricalStatement
    {
        time_t mWhen;
        SCPStatement mStatement;
        bool mValidated;
    };

    std::vector<HistoricalStatement> mStatementsHistory;

        bool mFullyValidated;

  public:
    Slot(uint64 slotIndex, SCP& SCP);

    uint64
    getSlotIndex() const
    {
        return mSlotIndex;
    }

    SCP&
    getSCP()
    {
        return mSCP;
    }

    SCPDriver&
    getSCPDriver()
    {
        return mSCP.getDriver();
    }

    SCPDriver const&
    getSCPDriver() const
    {
        return mSCP.getDriver();
    }

    BallotProtocol&
    getBallotProtocol()
    {
        return mBallotProtocol;
    }

    Value const& getLatestCompositeCandidate();

        std::vector<SCPEnvelope> getLatestMessagesSend() const;

            void setStateFromEnvelope(SCPEnvelope const& e);

        std::vector<SCPEnvelope> getCurrentState() const;

            SCPEnvelope const* getLatestMessage(NodeID const& id) const;

        std::vector<SCPEnvelope> getExternalizingState() const;

        void recordStatement(SCPStatement const& st);

                    SCP::EnvelopeState processEnvelope(SCPEnvelope const& envelope, bool self);

    bool abandonBallot();

                        bool bumpState(Value const& value, bool force);

        bool nominate(Value const& value, Value const& previousValue,
                  bool timedout);

    void stopNomination();

        std::set<NodeID> getNominationLeaders() const;

    bool isFullyValidated() const;
    void setFullyValidated(bool fullyValidated);

    
    size_t
    getStatementCount() const
    {
        return mStatementsHistory.size();
    }

            Json::Value getJsonInfo(bool fullKeys = false);

        Json::Value getJsonQuorumInfo(NodeID const& id, bool summary,
                                  bool fullKeys = false);

                    static Hash getCompanionQuorumSetHashFromStatement(SCPStatement const& st);

        static std::vector<Value> getStatementValues(SCPStatement const& st);

            SCPQuorumSetPtr getQuorumSetFromStatement(SCPStatement const& st);

        SCPEnvelope createEnvelope(SCPStatement const& statement);

    
            bool federatedAccept(StatementPredicate voted, StatementPredicate accepted,
                         std::map<NodeID, SCPEnvelope> const& envs);
            bool federatedRatify(StatementPredicate voted,
                         std::map<NodeID, SCPEnvelope> const& envs);

    std::shared_ptr<LocalNode> getLocalNode();

    enum timerIDs
    {
        NOMINATION_TIMER = 0,
        BALLOT_PROTOCOL_TIMER = 1
    };

  protected:
    std::vector<SCPEnvelope> getEntireCurrentState();
    friend class TestSCP;
};
}

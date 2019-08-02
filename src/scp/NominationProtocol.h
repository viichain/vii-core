#pragma once


#include "lib/json/json-forwards.h"
#include "scp/SCP.h"
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace viichain
{
class NominationProtocol
{
  protected:
    Slot& mSlot;

    int32 mRoundNumber;
    std::set<Value> mVotes;                           // X
    std::set<Value> mAccepted;                        // Y
    std::set<Value> mCandidates;                      // Z
    std::map<NodeID, SCPEnvelope> mLatestNominations; // N

    std::unique_ptr<SCPEnvelope>
        mLastEnvelope; // last envelope emitted by this node

        std::set<NodeID> mRoundLeaders;

        bool mNominationStarted;

        Value mLatestCompositeCandidate;

        Value mPreviousValue;

    bool isNewerStatement(NodeID const& nodeID, SCPNomination const& st);
    static bool isNewerStatement(SCPNomination const& oldst,
                                 SCPNomination const& st);

                static bool isSubsetHelper(xdr::xvector<Value> const& p,
                               xdr::xvector<Value> const& v, bool& notEqual);

    SCPDriver::ValidationLevel validateValue(Value const& v);
    Value extractValidValue(Value const& value);

    bool isSane(SCPStatement const& st);

    void recordEnvelope(SCPEnvelope const& env);

    void emitNomination();

        static bool acceptPredicate(Value const& v, SCPStatement const& st);

        static void applyAll(SCPNomination const& nom,
                         std::function<void(Value const&)> processor);

        void updateRoundLeaders();

            uint64 hashNode(bool isPriority, NodeID const& nodeID);

        uint64 hashValue(Value const& value);

    uint64 getNodePriority(NodeID const& nodeID, SCPQuorumSet const& qset);

                Value getNewValueFromNomination(SCPNomination const& nom);

  public:
    NominationProtocol(Slot& slot);

    SCP::EnvelopeState processEnvelope(SCPEnvelope const& envelope);

    static std::vector<Value> getStatementValues(SCPStatement const& st);

        bool nominate(Value const& value, Value const& previousValue,
                  bool timedout);

        void stopNomination();

        std::set<NodeID> const& getLeaders() const;

    Value const&
    getLatestCompositeCandidate() const
    {
        return mLatestCompositeCandidate;
    }

    Json::Value getJsonInfo();

    SCPEnvelope*
    getLastMessageSend() const
    {
        return mLastEnvelope.get();
    }

    void setStateFromEnvelope(SCPEnvelope const& e);

    std::vector<SCPEnvelope> getCurrentState() const;

            SCPEnvelope const* getLatestMessage(NodeID const& id) const;
};
}

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
class Node;
class Slot;

typedef std::function<bool(SCPStatement const& st)> StatementPredicate;

class BallotProtocol
{
    Slot& mSlot;

    bool mHeardFromQuorum;

        enum SCPPhase
    {
        SCP_PHASE_PREPARE,
        SCP_PHASE_CONFIRM,
        SCP_PHASE_EXTERNALIZE,
        SCP_PHASE_NUM
    };
        static const char* phaseNames[];

    std::unique_ptr<SCPBallot> mCurrentBallot;      // b
    std::unique_ptr<SCPBallot> mPrepared;           // p
    std::unique_ptr<SCPBallot> mPreparedPrime;      // p'
    std::unique_ptr<SCPBallot> mHighBallot;         // h
    std::unique_ptr<SCPBallot> mCommit;             // c
    std::map<NodeID, SCPEnvelope> mLatestEnvelopes; // M
    SCPPhase mPhase;                                // Phi
    std::unique_ptr<Value> mValueOverride;          // z

    int mCurrentMessageLevel; // number of messages triggered in one run

    std::shared_ptr<SCPEnvelope>
        mLastEnvelope; // last envelope generated by this node

    std::shared_ptr<SCPEnvelope>
        mLastEnvelopeEmit; // last envelope emitted by this node

  public:
    BallotProtocol(Slot& slot);

                    SCP::EnvelopeState processEnvelope(SCPEnvelope const& envelope, bool self);

    void ballotProtocolTimerExpired();
            bool abandonBallot(uint32 n);

                        bool bumpState(Value const& value, bool force);
        bool bumpState(Value const& value, uint32 n);

    
            Json::Value getJsonInfo();

        Json::Value getJsonQuorumInfo(NodeID const& id, bool summary,
                                  bool fullKeys = false);

                    static Hash getCompanionQuorumSetHashFromStatement(SCPStatement const& st);

            static SCPBallot getWorkingBallot(SCPStatement const& st);

    SCPEnvelope*
    getLastMessageSend() const
    {
        return mLastEnvelopeEmit.get();
    }

    void setStateFromEnvelope(SCPEnvelope const& e);

    std::vector<SCPEnvelope> getCurrentState() const;

            SCPEnvelope const* getLatestMessage(NodeID const& id) const;

    std::vector<SCPEnvelope> getExternalizingState() const;

  private:
                void advanceSlot(SCPStatement const& hint);

        SCPDriver::ValidationLevel validateValues(SCPStatement const& st);

        void sendLatestEnvelope();

                                
            
        bool attemptPreparedAccept(SCPStatement const& hint);
        bool setPreparedAccept(SCPBallot const& prepared);

            bool attemptPreparedConfirmed(SCPStatement const& hint);
        bool setPreparedConfirmed(SCPBallot const& newC, SCPBallot const& newH);

        bool attemptAcceptCommit(SCPStatement const& hint);
        bool setAcceptCommit(SCPBallot const& c, SCPBallot const& h);

        bool attemptConfirmCommit(SCPStatement const& hint);
    bool setConfirmCommit(SCPBallot const& acceptCommitLow,
                          SCPBallot const& acceptCommitHigh);

        bool attemptBump();

        std::set<SCPBallot> getPrepareCandidates(SCPStatement const& hint);

        bool updateCurrentIfNeeded(SCPBallot const& h);

        using Interval = std::pair<uint32, uint32>;

                static void findExtendedInterval(Interval& candidate,
                                     std::set<uint32> const& boundaries,
                                     std::function<bool(Interval const&)> pred);

            std::set<uint32> getCommitBoundariesFromStatements(SCPBallot const& ballot);

        
        static bool hasPreparedBallot(SCPBallot const& ballot,
                                  SCPStatement const& st);

        static bool commitPredicate(SCPBallot const& ballot, Interval const& check,
                                SCPStatement const& st);

        bool setPrepared(SCPBallot const& ballot);

    
        static int compareBallots(std::unique_ptr<SCPBallot> const& b1,
                              std::unique_ptr<SCPBallot> const& b2);
    static int compareBallots(SCPBallot const& b1, SCPBallot const& b2);

        static bool areBallotsCompatible(SCPBallot const& b1, SCPBallot const& b2);

        static bool areBallotsLessAndIncompatible(SCPBallot const& b1,
                                              SCPBallot const& b2);
        static bool areBallotsLessAndCompatible(SCPBallot const& b1,
                                            SCPBallot const& b2);

    
            bool isNewerStatement(NodeID const& nodeID, SCPStatement const& st);

        static bool isNewerStatement(SCPStatement const& oldst,
                                 SCPStatement const& st);

        bool isStatementSane(SCPStatement const& st, bool self);

        void recordEnvelope(SCPEnvelope const& env);

    
                    void bumpToBallot(SCPBallot const& ballot, bool check);

                bool updateCurrentValue(SCPBallot const& ballot);

            void emitCurrentStateStatement();

        void checkInvariants();

        SCPStatement createStatement(SCPStatementType const& type);

            std::string getLocalState() const;

    std::shared_ptr<LocalNode> getLocalNode();

    bool federatedAccept(StatementPredicate voted, StatementPredicate accepted);
    bool federatedRatify(StatementPredicate voted);

    void startBallotProtocolTimer();
    void stopBallotProtocolTimer();
    void checkHeardFromQuorum();
};
}
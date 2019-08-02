#pragma once


#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <set>

#include "xdr/vii-SCP.h"

namespace viichain
{
typedef std::shared_ptr<SCPQuorumSet> SCPQuorumSetPtr;

class SCPDriver
{
  public:
    virtual ~SCPDriver()
    {
    }

        virtual void signEnvelope(SCPEnvelope& envelope) = 0;

                                            virtual SCPQuorumSetPtr getQSet(Hash const& qSetHash) = 0;

                
            virtual void emitEnvelope(SCPEnvelope const& envelope) = 0;

    
                                            enum ValidationLevel
    {
        kInvalidValue,        // value is invalid for sure
        kFullyValidatedValue, // value is valid for sure
        kMaybeValidValue      // value may be valid
    };
    virtual ValidationLevel
    validateValue(uint64 slotIndex, Value const& value, bool nomination)
    {
        return kMaybeValidValue;
    }

                        virtual Value
    extractValidValue(uint64 slotIndex, Value const& value)
    {
        return Value();
    }

            virtual std::string getValueString(Value const& v) const;

        virtual std::string toStrKey(PublicKey const& pk,
                                 bool fullKey = true) const;

        virtual std::string toShortString(PublicKey const& pk) const;

            virtual uint64 computeHashNode(uint64 slotIndex, Value const& prev,
                                   bool isPriority, int32_t roundNumber,
                                   NodeID const& nodeID);

            virtual uint64 computeValueHash(uint64 slotIndex, Value const& prev,
                                    int32_t roundNumber, Value const& value);

            virtual Value combineCandidates(uint64 slotIndex,
                                    std::set<Value> const& candidates) = 0;

            virtual void setupTimer(uint64 slotIndex, int timerID,
                            std::chrono::milliseconds timeout,
                            std::function<void()> cb) = 0;

                virtual std::chrono::milliseconds computeTimeout(uint32 roundNumber);

    
            virtual void
    valueExternalized(uint64 slotIndex, Value const& value)
    {
    }

            virtual void
    nominatingValue(uint64 slotIndex, Value const& value)
    {
    }

        
                virtual void
    updatedCandidateValue(uint64 slotIndex, Value const& value)
    {
    }

            virtual void
    startedBallotProtocol(uint64 slotIndex, SCPBallot const& ballot)
    {
    }

        virtual void
    acceptedBallotPrepared(uint64 slotIndex, SCPBallot const& ballot)
    {
    }

        virtual void
    confirmedBallotPrepared(uint64 slotIndex, SCPBallot const& ballot)
    {
    }

        virtual void
    acceptedCommit(uint64 slotIndex, SCPBallot const& ballot)
    {
    }

                virtual void
    ballotDidHearFromQuorum(uint64 slotIndex, SCPBallot const& ballot)
    {
    }
};
}

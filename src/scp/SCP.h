#pragma once


#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <set>

#include "crypto/SecretKey.h"
#include "lib/json/json-forwards.h"
#include "scp/SCPDriver.h"

namespace viichain
{
class Node;
class Slot;
class LocalNode;
typedef std::shared_ptr<SCPQuorumSet> SCPQuorumSetPtr;

class SCP
{
    SCPDriver& mDriver;

  public:
    SCP(SCPDriver& driver, NodeID const& nodeID, bool isValidator,
        SCPQuorumSet const& qSetLocal);

    SCPDriver&
    getDriver()
    {
        return mDriver;
    }
    SCPDriver const&
    getDriver() const
    {
        return mDriver;
    }

    enum EnvelopeState
    {
        INVALID, // the envelope is considered invalid
        VALID    // the envelope is valid
    };

                EnvelopeState receiveEnvelope(SCPEnvelope const& envelope);

            bool nominate(uint64 slotIndex, Value const& value,
                  Value const& previousValue);

        void stopNomination(uint64 slotIndex);

        void updateLocalQuorumSet(SCPQuorumSet const& qSet);
    SCPQuorumSet const& getLocalQuorumSet();

        NodeID const& getLocalNodeID();

        std::shared_ptr<LocalNode> getLocalNode();

    Json::Value getJsonInfo(size_t limit, bool fullKeys = false);

            Json::Value getJsonQuorumInfo(NodeID const& id, bool summary,
                                  bool fullKeys = false, uint64 index = 0);

            void purgeSlots(uint64 maxSlotIndex);

        bool isValidator();

        bool isSlotFullyValidated(uint64 slotIndex);

            size_t getKnownSlotsCount() const;
    size_t getCumulativeStatemtCount() const;

        std::vector<SCPEnvelope> getLatestMessagesSend(uint64 slotIndex);

            void setStateFromEnvelope(uint64 slotIndex, SCPEnvelope const& e);

        bool empty() const;
        uint64 getLowSlotIndex() const;
        uint64 getHighSlotIndex() const;

        std::vector<SCPEnvelope> getCurrentState(uint64 slotIndex);

            SCPEnvelope const* getLatestMessage(NodeID const& id);

            std::vector<SCPEnvelope> getExternalizingState(uint64 slotIndex);

        std::string getValueString(Value const& v) const;
    std::string ballotToStr(SCPBallot const& ballot) const;
    std::string ballotToStr(std::unique_ptr<SCPBallot> const& ballot) const;
    std::string envToStr(SCPEnvelope const& envelope,
                         bool fullKeys = false) const;
    std::string envToStr(SCPStatement const& st, bool fullKeys = false) const;

  protected:
    std::shared_ptr<LocalNode> mLocalNode;
    std::map<uint64, std::shared_ptr<Slot>> mKnownSlots;

        std::shared_ptr<Slot> getSlot(uint64 slotIndex, bool create);

    friend class TestSCP;
};
}

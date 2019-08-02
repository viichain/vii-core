#pragma once


#include <memory>
#include <set>
#include <vector>

#include "scp/SCP.h"
#include "util/HashOfHash.h"

namespace viichain
{
class LocalNode
{
  protected:
    const NodeID mNodeID;
    const bool mIsValidator;
    SCPQuorumSet mQSet;
    Hash mQSetHash;

        Hash gSingleQSetHash;                      // hash of the singleton qset
    std::shared_ptr<SCPQuorumSet> mSingleQSet; // {{mNodeID}}

    SCP* mSCP;

  public:
    LocalNode(NodeID const& nodeID, bool isValidator, SCPQuorumSet const& qSet,
              SCP* scp);

    NodeID const& getNodeID();

    void updateQuorumSet(SCPQuorumSet const& qSet);

    SCPQuorumSet const& getQuorumSet();
    Hash const& getQuorumSetHash();
    bool isValidator();

        static SCPQuorumSetPtr getSingletonQSet(NodeID const& nodeID);

        static void forAllNodes(SCPQuorumSet const& qset,
                            std::function<void(NodeID const&)> proc);

            static uint64 getNodeWeight(NodeID const& nodeID, SCPQuorumSet const& qset);

        static bool isQuorumSlice(SCPQuorumSet const& qSet,
                              std::vector<NodeID> const& nodeSet);
    static bool isVBlocking(SCPQuorumSet const& qSet,
                            std::vector<NodeID> const& nodeSet);

    
            static bool
    isVBlocking(SCPQuorumSet const& qSet,
                std::map<NodeID, SCPEnvelope> const& map,
                std::function<bool(SCPStatement const&)> const& filter =
                    [](SCPStatement const&) { return true; });

                        static bool
    isQuorum(SCPQuorumSet const& qSet, std::map<NodeID, SCPEnvelope> const& map,
             std::function<SCPQuorumSetPtr(SCPStatement const&)> const& qfun,
             std::function<bool(SCPStatement const&)> const& filter =
                 [](SCPStatement const&) { return true; });

                static std::vector<NodeID>
    findClosestVBlocking(SCPQuorumSet const& qset,
                         std::set<NodeID> const& nodes, NodeID const* excluded);

    static std::vector<NodeID> findClosestVBlocking(
        SCPQuorumSet const& qset, std::map<NodeID, SCPEnvelope> const& map,
        std::function<bool(SCPStatement const&)> const& filter =
            [](SCPStatement const&) { return true; },
        NodeID const* excluded = nullptr);

    static Json::Value toJson(SCPQuorumSet const& qSet,
                              std::function<std::string(PublicKey const&)> r);

    Json::Value toJson(SCPQuorumSet const& qSet, bool fullKeys) const;
    std::string to_string(SCPQuorumSet const& qSet) const;

    static uint64 computeWeight(uint64 m, uint64 total, uint64 threshold);

  protected:
        static SCPQuorumSet buildSingletonQSet(NodeID const& nodeID);

        static bool isQuorumSliceInternal(SCPQuorumSet const& qset,
                                      std::vector<NodeID> const& nodeSet);
    static bool isVBlockingInternal(SCPQuorumSet const& qset,
                                    std::vector<NodeID> const& nodeSet);
};
}

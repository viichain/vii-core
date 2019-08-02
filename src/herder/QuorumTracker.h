#pragma once


#include "scp/SCP.h"
#include "util/NonCopyable.h"
#include <unordered_map>

namespace viichain
{

class QuorumTracker : public NonMovableOrCopyable
{
  public:
    using QuorumMap = std::unordered_map<NodeID, SCPQuorumSetPtr>;

  private:
    SCP& mSCP;
    QuorumMap mQuorum;

  public:
    QuorumTracker(SCP& scp);

        bool isNodeDefinitelyInQuorum(NodeID const& id);

                                    bool expand(NodeID const& id, SCPQuorumSetPtr qSet);

        void rebuild(std::function<SCPQuorumSetPtr(NodeID const&)> lookup);

        QuorumMap const& getQuorum() const;
};
}

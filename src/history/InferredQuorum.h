#pragma once


#include "crypto/SecretKey.h"
#include "herder/QuorumTracker.h"
#include "main/Config.h"
#include "overlay/StellarXDR.h"
#include "util/HashOfHash.h"
#include <string>
#include <unordered_map>

namespace viichain
{

struct InferredQuorum
{
    InferredQuorum();
    InferredQuorum(QuorumTracker::QuorumMap const& qmap);
    std::unordered_map<Hash, SCPQuorumSet> mQsets;
    std::unordered_map<PublicKey, std::vector<Hash>> mQsetHashes;
    std::unordered_map<PublicKey, size_t> mPubKeys;
    void noteSCPHistory(SCPHistoryEntry const& hist);
    void noteQset(SCPQuorumSet const& qset);
    void noteQsetHash(PublicKey const& pk, Hash const& hash);
    void notePubKey(PublicKey const& pk);
    std::string toString(Config const& cfg) const;
    void writeQuorumGraph(Config const& cfg, std::ostream& out) const;
    QuorumTracker::QuorumMap getQuorumMap() const;
};
}

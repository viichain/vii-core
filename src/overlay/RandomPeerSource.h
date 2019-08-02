#pragma once


#include "overlay/PeerManager.h"

#include <functional>
#include <vector>

namespace viichain
{

enum class PeerType;
class PeerManager;
struct PeerQuery;

class RandomPeerSource
{
  public:
    static PeerQuery maxFailures(int maxFailures, bool outbound);
    static PeerQuery nextAttemptCutoff(PeerType peerType);

    explicit RandomPeerSource(PeerManager& peerManager, PeerQuery peerQuery);

    std::vector<PeerBareAddress>
    getRandomPeers(int size, std::function<bool(PeerBareAddress const&)> pred);

  private:
    PeerManager& mPeerManager;
    PeerQuery const mPeerQuery;
    std::vector<PeerBareAddress> mPeerCache;
};
}

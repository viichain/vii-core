#pragma once


#include "overlay/Peer.h"
#include "util/HashOfHash.h"
#include "util/NonCopyable.h"
#include "util/Timer.h"
#include <deque>
#include <functional>
#include <map>
#include <util/optional.h>

namespace medida
{
class Counter;
}

namespace viichain
{

class Tracker;
class TxSetFrame;
struct SCPQuorumSet;
using TxSetFramePtr = std::shared_ptr<TxSetFrame>;
using SCPQuorumSetPtr = std::shared_ptr<SCPQuorumSet>;
using AskPeer = std::function<void(Peer::pointer, Hash)>;

class ItemFetcher : private NonMovableOrCopyable
{
  public:
    using TrackerPtr = std::shared_ptr<Tracker>;

        explicit ItemFetcher(Application& app, AskPeer askPeer);

        void fetch(Hash itemHash, const SCPEnvelope& envelope);

        void stopFetch(Hash itemHash, const SCPEnvelope& envelope);

        uint64 getLastSeenSlotIndex(Hash itemHash) const;

        std::vector<SCPEnvelope> fetchingFor(Hash itemHash) const;

        void stopFetchingBelow(uint64 slotIndex);

        void doesntHave(Hash const& itemHash, Peer::pointer peer);

        void recv(Hash itemHash);

  protected:
    void stopFetchingBelowInternal(uint64 slotIndex);

    Application& mApp;
    std::map<Hash, std::shared_ptr<Tracker>> mTrackers;

  private:
    AskPeer mAskPeer;
};
}

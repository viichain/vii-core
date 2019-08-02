#pragma once


#include "overlay/Peer.h"
#include "util/Timer.h"
#include "xdr/vii-types.h"

#include <functional>
#include <utility>
#include <vector>

namespace viichain
{

class Application;

using AskPeer = std::function<void(Peer::pointer, Hash)>;

class Tracker
{
  private:
    AskPeer mAskPeer;
    Application& mApp;
    Peer::pointer mLastAskedPeer;
    int mNumListRebuild;
    std::deque<Peer::pointer> mPeersToAsk;
    VirtualTimer mTimer;
    std::vector<std::pair<Hash, SCPEnvelope>> mWaitingEnvelopes;
    Hash mItemHash;
    medida::Meter& mTryNextPeer;
    uint64 mLastSeenSlotIndex{0};

  public:
        explicit Tracker(Application& app, Hash const& hash, AskPeer& askPeer);
    virtual ~Tracker();

        bool
    empty() const
    {
        return mWaitingEnvelopes.empty();
    }

        const std::vector<std::pair<Hash, SCPEnvelope>>&
    waitingEnvelopes() const
    {
        return mWaitingEnvelopes;
    }

        size_t
    size() const
    {
        return mWaitingEnvelopes.size();
    }

        SCPEnvelope pop();

        bool clearEnvelopesBelow(uint64 slotIndex);

        void listen(const SCPEnvelope& env);

        void discard(const SCPEnvelope& env);

        void cancel();

        void doesntHave(Peer::pointer peer);

        void tryNextPeer();

        uint64
    getLastSeenSlotIndex() const
    {
        return mLastSeenSlotIndex;
    }

        void
    resetLastSeenSlotIndex()
    {
        mLastSeenSlotIndex = 0;
    }
};
}

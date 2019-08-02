#pragma once
#include "crypto/SecretKey.h"
#include "herder/Herder.h"
#include "herder/QuorumTracker.h"
#include "lib/json/json.h"
#include "lib/util/lrucache.hpp"
#include "overlay/ItemFetcher.h"
#include <autocheck/function.hpp>
#include <map>
#include <medida/medida.h>
#include <queue>
#include <set>
#include <util/optional.h>

namespace viichain
{

class HerderImpl;

struct SlotEnvelopes
{
        std::vector<SCPEnvelope> mProcessedEnvelopes;
        std::set<SCPEnvelope> mDiscardedEnvelopes;
        std::set<SCPEnvelope> mFetchingEnvelopes;
        std::vector<SCPEnvelope> mReadyEnvelopes;
};

class PendingEnvelopes
{
    Application& mApp;
    HerderImpl& mHerder;

        std::map<uint64, SlotEnvelopes> mEnvelopes;

        cache::lru_cache<Hash, SCPQuorumSetPtr> mQsetCache;

    ItemFetcher mTxSetFetcher;
    ItemFetcher mQuorumSetFetcher;

    using TxSetFramCacheItem = std::pair<uint64, TxSetFramePtr>;
        cache::lru_cache<Hash, TxSetFramCacheItem> mTxSetCache;

    bool mRebuildQuorum;
    QuorumTracker mQuorumTracker;

    medida::Counter& mProcessedCount;
    medida::Counter& mDiscardedCount;
    medida::Counter& mFetchingCount;
    medida::Counter& mReadyCount;

            void discardSCPEnvelopesWithQSet(Hash hash);

    void updateMetrics();

  public:
    PendingEnvelopes(Application& app, HerderImpl& herder);
    ~PendingEnvelopes();

        Herder::EnvelopeStatus recvSCPEnvelope(SCPEnvelope const& envelope);

        void addSCPQuorumSet(Hash hash, const SCPQuorumSet& qset);

        bool recvSCPQuorumSet(Hash hash, const SCPQuorumSet& qset);

        void addTxSet(Hash hash, uint64 lastSeenSlotIndex, TxSetFramePtr txset);

        bool recvTxSet(Hash hash, TxSetFramePtr txset);
    void discardSCPEnvelope(SCPEnvelope const& envelope);

    void peerDoesntHave(MessageType type, Hash const& itemID,
                        Peer::pointer peer);

    bool isDiscarded(SCPEnvelope const& envelope) const;
    bool isFullyFetched(SCPEnvelope const& envelope);
    void startFetch(SCPEnvelope const& envelope);
    void stopFetch(SCPEnvelope const& envelope);
    void touchFetchCache(SCPEnvelope const& envelope);

    void envelopeReady(SCPEnvelope const& envelope);

    bool pop(uint64 slotIndex, SCPEnvelope& ret);

    void eraseBelow(uint64 slotIndex);

    void slotClosed(uint64 slotIndex);

    std::vector<uint64> readySlots();

    Json::Value getJsonInfo(size_t limit);

    TxSetFramePtr getTxSet(Hash const& hash);
    SCPQuorumSetPtr getQSet(Hash const& hash);

            bool isNodeDefinitelyInQuorum(NodeID const& node);

    void rebuildQuorumTrackerState();
    QuorumTracker::QuorumMap const& getCurrentlyTrackedQuorum() const;

        void envelopeProcessed(SCPEnvelope const& env);
};
}

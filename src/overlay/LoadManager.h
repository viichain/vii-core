#pragma once


#include "crypto/SecretKey.h"
#include "overlay/Peer.h"
#include "util/HashOfHash.h"
#include "util/lrucache.hpp"
#include "xdr/vii-types.h"

#include "medida/meter.h"
#include "medida/metrics_registry.h"
#include "medida/timer.h"

#include "util/Timer.h"

namespace viichain
{

class Application;

class LoadManager
{
                                            
  public:
    LoadManager();
    ~LoadManager();
    void reportLoads(std::map<NodeID, Peer::pointer> const& peers,
                     Application& app);

                struct PeerCosts
    {
        PeerCosts();
        bool isLessThan(std::shared_ptr<PeerCosts> other);
        medida::Meter mTimeSpent;
        medida::Meter mBytesSend;
        medida::Meter mBytesRecv;
        medida::Meter mSQLQueries;
    };

    std::shared_ptr<PeerCosts> getPeerCosts(NodeID const& peer);

  private:
    cache::lru_cache<NodeID, std::shared_ptr<PeerCosts>> mPeerCosts;

  public:
                void maybeShedExcessLoad(Application& app);

                class PeerContext
    {
        Application& mApp;
        NodeID const& mNode;

        VirtualClock::time_point mWorkStart;
        std::uint64_t mBytesSendStart;
        std::uint64_t mBytesRecvStart;
        std::uint64_t mSQLQueriesStart;

      public:
        PeerContext(Application& app, NodeID const& node);
        ~PeerContext();
    };
};
}

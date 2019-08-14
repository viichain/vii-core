#pragma once


#include "overlay/Peer.h"
#include "overlay/VIIXDR.h"
#include <map>

namespace medida
{
class Counter;
}

namespace viichain
{

class Floodgate
{
    class FloodRecord
    {
      public:
        typedef std::shared_ptr<FloodRecord> pointer;

        uint32_t mLedgerSeq;
        VIIMessage mMessage;
        std::set<std::string> mPeersTold;

        FloodRecord(VIIMessage const& msg, uint32_t ledger,
                    Peer::pointer peer);
    };

    std::map<uint256, FloodRecord::pointer> mFloodMap;
    Application& mApp;
    medida::Counter& mFloodMapSize;
    medida::Meter& mSendFromBroadcast;
    bool mShuttingDown;

  public:
    Floodgate(Application& app);
        void clearBelow(uint32_t currentLedger);
        bool addRecord(VIIMessage const& msg, Peer::pointer fromPeer);

    void broadcast(VIIMessage const& msg, bool force);

        std::set<Peer::pointer> getPeersKnows(Hash const& h);

    void shutdown();
};
}

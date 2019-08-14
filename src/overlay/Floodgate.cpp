
#include "overlay/Floodgate.h"
#include "crypto/Hex.h"
#include "crypto/SHA.h"
#include "herder/Herder.h"
#include "main/Application.h"
#include "medida/counter.h"
#include "medida/metrics_registry.h"
#include "overlay/OverlayManager.h"
#include "util/Logging.h"
#include "util/XDROperators.h"
#include "xdrpp/marshal.h"

namespace viichain
{
Floodgate::FloodRecord::FloodRecord(VIIMessage const& msg, uint32_t ledger,
                                    Peer::pointer peer)
    : mLedgerSeq(ledger), mMessage(msg)
{
    if (peer)
        mPeersTold.insert(peer->toString());
}

Floodgate::Floodgate(Application& app)
    : mApp(app)
    , mFloodMapSize(
          app.getMetrics().NewCounter({"overlay", "memory", "flood-known"}))
    , mSendFromBroadcast(app.getMetrics().NewMeter(
          {"overlay", "flood", "broadcast"}, "message"))
    , mShuttingDown(false)
{
}

void
Floodgate::clearBelow(uint32_t currentLedger)
{
    for (auto it = mFloodMap.cbegin(); it != mFloodMap.cend();)
    {
                if (it->second->mLedgerSeq + 10 < currentLedger)
        {
            mFloodMap.erase(it++);
        }
        else
        {
            ++it;
        }
    }
    mFloodMapSize.set_count(mFloodMap.size());
}

bool
Floodgate::addRecord(VIIMessage const& msg, Peer::pointer peer)
{
    if (mShuttingDown)
    {
        return false;
    }
    Hash index = sha256(xdr::xdr_to_opaque(msg));
    auto result = mFloodMap.find(index);
    if (result == mFloodMap.end())
    { // we have never seen this message
        mFloodMap[index] = std::make_shared<FloodRecord>(
            msg, mApp.getHerder().getCurrentLedgerSeq(), peer);
        mFloodMapSize.set_count(mFloodMap.size());
        return true;
    }
    else
    {
        result->second->mPeersTold.insert(peer->toString());
        return false;
    }
}

void
Floodgate::broadcast(VIIMessage const& msg, bool force)
{
    if (mShuttingDown)
    {
        return;
    }
    Hash index = sha256(xdr::xdr_to_opaque(msg));
    CLOG(TRACE, "Overlay") << "broadcast " << hexAbbrev(index);

    auto result = mFloodMap.find(index);
    if (result == mFloodMap.end() || force)
    { // no one has sent us this message
        FloodRecord::pointer record = std::make_shared<FloodRecord>(
            msg, mApp.getHerder().getCurrentLedgerSeq(), Peer::pointer());
        result = mFloodMap.insert(std::make_pair(index, record)).first;
        mFloodMapSize.set_count(mFloodMap.size());
    }
        auto& peersTold = result->second->mPeersTold;

        auto peers = mApp.getOverlayManager().getAuthenticatedPeers();

    for (auto peer : peers)
    {
        assert(peer.second->isAuthenticated());
        if (peersTold.find(peer.second->toString()) == peersTold.end())
        {
            mSendFromBroadcast.Mark();
            peer.second->sendMessage(msg);
            peersTold.insert(peer.second->toString());
        }
    }
    CLOG(TRACE, "Overlay") << "broadcast " << hexAbbrev(index) << " told "
                           << peersTold.size();
}

std::set<Peer::pointer>
Floodgate::getPeersKnows(Hash const& h)
{
    std::set<Peer::pointer> res;
    auto record = mFloodMap.find(h);
    if (record != mFloodMap.end())
    {
        auto& ids = record->second->mPeersTold;
        auto const& peers = mApp.getOverlayManager().getAuthenticatedPeers();
        for (auto& p : peers)
        {
            if (ids.find(p.second->toString()) != ids.end())
            {
                res.insert(p.second);
            }
        }
    }
    return res;
}

void
Floodgate::shutdown()
{
    mShuttingDown = true;
    mFloodMap.clear();
}
}

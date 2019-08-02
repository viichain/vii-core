
#include "overlay/ItemFetcher.h"
#include "crypto/Hex.h"
#include "crypto/SHA.h"
#include "herder/Herder.h"
#include "herder/TxSetFrame.h"
#include "main/Application.h"
#include "medida/metrics_registry.h"
#include "overlay/OverlayManager.h"
#include "overlay/StellarXDR.h"
#include "overlay/Tracker.h"
#include "util/Logging.h"
#include "xdrpp/marshal.h"

namespace viichain
{

ItemFetcher::ItemFetcher(Application& app, AskPeer askPeer)
    : mApp(app), mAskPeer(askPeer)
{
}

void
ItemFetcher::fetch(Hash itemHash, const SCPEnvelope& envelope)
{
    CLOG(TRACE, "Overlay") << "fetch " << hexAbbrev(itemHash);
    auto entryIt = mTrackers.find(itemHash);
    if (entryIt == mTrackers.end())
    { // not being tracked
        TrackerPtr tracker =
            std::make_shared<Tracker>(mApp, itemHash, mAskPeer);
        mTrackers[itemHash] = tracker;

        tracker->listen(envelope);
        tracker->tryNextPeer();
    }
    else
    {
        entryIt->second->listen(envelope);
    }
}

void
ItemFetcher::stopFetch(Hash itemHash, const SCPEnvelope& envelope)
{
    CLOG(TRACE, "Overlay") << "stopFetch " << hexAbbrev(itemHash);
    const auto& iter = mTrackers.find(itemHash);
    if (iter != mTrackers.end())
    {
        auto const& tracker = iter->second;

        CLOG(TRACE, "Overlay")
            << "stopFetch " << hexAbbrev(itemHash) << " : " << tracker->size();
        tracker->discard(envelope);
        if (tracker->empty())
        {
                                    tracker->cancel();
        }
    }
}

uint64
ItemFetcher::getLastSeenSlotIndex(Hash itemHash) const
{
    auto iter = mTrackers.find(itemHash);
    if (iter == mTrackers.end())
    {
        return 0;
    }

    return iter->second->getLastSeenSlotIndex();
}

std::vector<SCPEnvelope>
ItemFetcher::fetchingFor(Hash itemHash) const
{
    auto result = std::vector<SCPEnvelope>{};
    auto iter = mTrackers.find(itemHash);
    if (iter == mTrackers.end())
    {
        return result;
    }

    auto const& waiting = iter->second->waitingEnvelopes();
    std::transform(
        std::begin(waiting), std::end(waiting), std::back_inserter(result),
        [](std::pair<Hash, SCPEnvelope> const& x) { return x.second; });
    return result;
}

void
ItemFetcher::stopFetchingBelow(uint64 slotIndex)
{
            mApp.postOnMainThread(
        [this, slotIndex]() { stopFetchingBelowInternal(slotIndex); },
        "ItemFetcher: stopFetchingBelow");
}

void
ItemFetcher::stopFetchingBelowInternal(uint64 slotIndex)
{
    for (auto iter = mTrackers.begin(); iter != mTrackers.end();)
    {
        if (!iter->second->clearEnvelopesBelow(slotIndex))
        {
            iter = mTrackers.erase(iter);
        }
        else
        {
            iter++;
        }
    }
}

void
ItemFetcher::doesntHave(Hash const& itemHash, Peer::pointer peer)
{
    const auto& iter = mTrackers.find(itemHash);
    if (iter != mTrackers.end())
    {
        iter->second->doesntHave(peer);
    }
}

void
ItemFetcher::recv(Hash itemHash)
{
    CLOG(TRACE, "Overlay") << "Recv " << hexAbbrev(itemHash);
    const auto& iter = mTrackers.find(itemHash);

    if (iter != mTrackers.end())
    {
                        auto& tracker = iter->second;

        CLOG(TRACE, "Overlay")
            << "Recv " << hexAbbrev(itemHash) << " : " << tracker->size();

        while (!tracker->empty())
        {
            mApp.getHerder().recvSCPEnvelope(tracker->pop());
        }
                tracker->resetLastSeenSlotIndex();
        tracker->cancel();
    }
}
}

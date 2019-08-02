#pragma once


#include "overlay/Peer.h"
#include "overlay/StellarXDR.h"

namespace viichain
{

class LoadManager;
class PeerAuth;
class PeerBareAddress;
class PeerManager;

class OverlayManager
{
  public:
    static std::unique_ptr<OverlayManager> create(Application& app);

        static void dropAll(Database& db);

                virtual void ledgerClosed(uint32_t lastClosedledgerSeq) = 0;

            virtual void broadcastMessage(StellarMessage const& msg,
                                  bool force = false) = 0;

                    virtual void recvFloodedMsg(StellarMessage const& msg,
                                Peer::pointer peer) = 0;

        virtual std::vector<Peer::pointer> getRandomAuthenticatedPeers() = 0;

            virtual Peer::pointer getConnectedPeer(PeerBareAddress const& address) = 0;

        virtual void addInboundConnection(Peer::pointer peer) = 0;

        virtual bool addOutboundConnection(Peer::pointer peer) = 0;

            virtual void removePeer(Peer* peer) = 0;

                            virtual bool acceptAuthenticatedPeer(Peer::pointer peer) = 0;

    virtual bool isPreferred(Peer* peer) const = 0;

        virtual std::vector<Peer::pointer> const&
    getInboundPendingPeers() const = 0;

        virtual std::vector<Peer::pointer> const&
    getOutboundPendingPeers() const = 0;

        virtual std::vector<Peer::pointer> getPendingPeers() const = 0;

        virtual int getPendingPeersCount() const = 0;

        virtual std::map<NodeID, Peer::pointer> const&
    getInboundAuthenticatedPeers() const = 0;

        virtual std::map<NodeID, Peer::pointer> const&
    getOutboundAuthenticatedPeers() const = 0;

        virtual std::map<NodeID, Peer::pointer> getAuthenticatedPeers() const = 0;

        virtual int getAuthenticatedPeersCount() const = 0;

        virtual void connectTo(PeerBareAddress const& address) = 0;

        virtual std::set<Peer::pointer> getPeersKnows(Hash const& h) = 0;

        virtual OverlayMetrics& getOverlayMetrics() = 0;

        virtual PeerAuth& getPeerAuth() = 0;

        virtual LoadManager& getLoadManager() = 0;

        virtual PeerManager& getPeerManager() = 0;

        virtual void start() = 0;
        virtual void shutdown() = 0;

    virtual bool isShuttingDown() const = 0;

    virtual ~OverlayManager()
    {
    }
};
}

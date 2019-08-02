
#include "PeerDoor.h"
#include "Peer.h"
#include "main/Application.h"
#include "main/Config.h"
#include "overlay/OverlayManager.h"
#include "overlay/TCPPeer.h"
#include "util/Logging.h"
#include <memory>

namespace viichain
{

using asio::ip::tcp;
using namespace std;

PeerDoor::PeerDoor(Application& app)
    : mApp(app), mAcceptor(mApp.getClock().getIOContext())
{
}

void
PeerDoor::start()
{
    if (!mApp.getConfig().RUN_STANDALONE)
    {
        tcp::endpoint endpoint(tcp::v4(), mApp.getConfig().PEER_PORT);
        CLOG(INFO, "Overlay") << "Binding to endpoint " << endpoint;
        mAcceptor.open(endpoint.protocol());
        mAcceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
        mAcceptor.bind(endpoint);
        mAcceptor.listen();
        acceptNextPeer();
    }
}

void
PeerDoor::close()
{
    if (mAcceptor.is_open())
    {
        asio::error_code ec;
                mAcceptor.close(ec);
    }
}

void
PeerDoor::acceptNextPeer()
{
    if (mApp.getOverlayManager().isShuttingDown())
    {
        return;
    }

    CLOG(DEBUG, "Overlay") << "PeerDoor acceptNextPeer()";
    auto sock =
        make_shared<TCPPeer::SocketType>(mApp.getClock().getIOContext());
    mAcceptor.async_accept(sock->next_layer(),
                           [this, sock](asio::error_code const& ec) {
                               if (ec)
                                   this->acceptNextPeer();
                               else
                                   this->handleKnock(sock);
                           });
}

void
PeerDoor::handleKnock(shared_ptr<TCPPeer::SocketType> socket)
{
    CLOG(DEBUG, "Overlay") << "PeerDoor handleKnock() @"
                           << mApp.getConfig().PEER_PORT;
    Peer::pointer peer = TCPPeer::accept(mApp, socket);
    if (peer)
    {
        mApp.getOverlayManager().addInboundConnection(peer);
    }
    acceptNextPeer();
}
}

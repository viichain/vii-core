
#include "overlay/TCPPeer.h"
#include "database/Database.h"
#include "main/Application.h"
#include "main/Config.h"
#include "main/ErrorMessages.h"
#include "medida/meter.h"
#include "medida/metrics_registry.h"
#include "overlay/LoadManager.h"
#include "overlay/OverlayManager.h"
#include "overlay/OverlayMetrics.h"
#include "overlay/PeerManager.h"
#include "overlay/StellarXDR.h"
#include "util/GlobalChecks.h"
#include "util/Logging.h"
#include "xdrpp/marshal.h"

using namespace soci;

namespace viichain
{

using namespace std;


TCPPeer::TCPPeer(Application& app, Peer::PeerRole role,
                 std::shared_ptr<TCPPeer::SocketType> socket)
    : Peer(app, role), mSocket(socket)
{
}

TCPPeer::pointer
TCPPeer::initiate(Application& app, PeerBareAddress const& address)
{
    assert(address.getType() == PeerBareAddress::Type::IPv4);

    CLOG(DEBUG, "Overlay") << "TCPPeer:initiate"
                           << " to " << address.toString();
    assertThreadIsMain();
    auto socket = make_shared<SocketType>(app.getClock().getIOContext());
    auto result = make_shared<TCPPeer>(app, WE_CALLED_REMOTE, socket);
    result->mAddress = address;
    result->startIdleTimer();
    asio::ip::tcp::endpoint endpoint(
        asio::ip::address::from_string(address.getIP()), address.getPort());
    socket->next_layer().async_connect(
        endpoint, [result](asio::error_code const& error) {
            asio::error_code ec;
            if (!error)
            {
                asio::ip::tcp::no_delay nodelay(true);
                result->mSocket->next_layer().set_option(nodelay, ec);
            }
            else
            {
                ec = error;
            }

            result->connectHandler(ec);
        });
    return result;
}

TCPPeer::pointer
TCPPeer::accept(Application& app, shared_ptr<TCPPeer::SocketType> socket)
{
    assertThreadIsMain();
    shared_ptr<TCPPeer> result;
    asio::error_code ec;

    asio::ip::tcp::no_delay nodelay(true);
    socket->next_layer().set_option(nodelay, ec);

    if (!ec)
    {
        CLOG(DEBUG, "Overlay") << "TCPPeer:accept"
                               << "@" << app.getConfig().PEER_PORT;
        result = make_shared<TCPPeer>(app, REMOTE_CALLED_US, socket);
        result->startIdleTimer();
        result->startRead();
    }
    else
    {
        CLOG(DEBUG, "Overlay")
            << "TCPPeer:accept"
            << "@" << app.getConfig().PEER_PORT << " error " << ec.message();
    }

    return result;
}

TCPPeer::~TCPPeer()
{
    assertThreadIsMain();
    mIdleTimer.cancel();
    if (mSocket)
    {
                        asio::error_code ec;

#ifndef _WIN32
                        mSocket->next_layer().cancel(ec);
#endif
        mSocket->close(ec);
    }
}

std::string
TCPPeer::getIP() const
{
    std::string result;

    asio::error_code ec;
    auto ep = mSocket->next_layer().remote_endpoint(ec);
    if (!ec)
    {
        result = ep.address().to_string();
    }

    return result;
}

void
TCPPeer::sendMessage(xdr::msg_ptr&& xdrBytes)
{
    if (mState == CLOSING)
    {
        CLOG(ERROR, "Overlay")
            << "Trying to send message to " << toString() << " after drop";
        CLOG(ERROR, "Overlay") << REPORT_INTERNAL_BUG;
        return;
    }

    if (Logging::logTrace("Overlay"))
        CLOG(TRACE, "Overlay") << "TCPPeer:sendMessage to " << toString();
    assertThreadIsMain();

        auto buf = std::make_shared<xdr::msg_ptr>(std::move(xdrBytes));

    auto self = static_pointer_cast<TCPPeer>(shared_from_this());

    self->mWriteQueue.emplace(buf);

    if (!self->mWriting)
    {
        self->mWriting = true;
                self->messageSender();
    }
}

void
TCPPeer::shutdown()
{
    if (mShutdownScheduled)
    {
                CLOG(ERROR, "Overlay") << "Double schedule of shutdown " << toString();
        CLOG(ERROR, "Overlay") << REPORT_INTERNAL_BUG;
        return;
    }

    mIdleTimer.cancel();
    mShutdownScheduled = true;
    auto self = static_pointer_cast<TCPPeer>(shared_from_this());

            self->getApp().postOnMainThread(
        [self]() {
                                                                                                                                                            asio::error_code ec;
            self->mSocket->next_layer().shutdown(
                asio::ip::tcp::socket::shutdown_both, ec);
            if (ec)
            {
                CLOG(DEBUG, "Overlay")
                    << "TCPPeer::drop shutdown socket failed: " << ec.message();
            }
            self->getApp().postOnMainThread(
                [self]() {
                                                                                                                                                                                                        asio::error_code ec2;
                    self->mSocket->close(ec2);
                    if (ec2)
                    {
                        CLOG(DEBUG, "Overlay")
                            << "TCPPeer::drop close socket failed: "
                            << ec2.message();
                    }
                },
                "TCPPeer: close");
        },
        "TCPPeer: shutdown");
}

void
TCPPeer::messageSender()
{
    assertThreadIsMain();

    auto self = static_pointer_cast<TCPPeer>(shared_from_this());

        if (mWriteQueue.empty())
    {
        mLastEmpty = mApp.getClock().now();
        mSocket->async_flush([self](asio::error_code const& ec, std::size_t) {
            self->writeHandler(ec, 0);
            if (!ec)
            {
                if (!self->mWriteQueue.empty())
                {
                    self->messageSender();
                }
                else
                {
                    self->mWriting = false;
                                                            if (self->mDelayedShutdown)
                    {
                        self->shutdown();
                    }
                }
            }
        });
        return;
    }

                auto buf = mWriteQueue.front();

    asio::async_write(*(mSocket.get()),
                      asio::buffer((*buf)->raw_data(), (*buf)->raw_size()),
                      [self](asio::error_code const& ec, std::size_t length) {
                          self->writeHandler(ec, length);
                          self->mWriteQueue.pop(); // done with front element

                                                    if (!ec)
                          {
                              self->messageSender();
                          }
                      });
}

void
TCPPeer::writeHandler(asio::error_code const& error,
                      std::size_t bytes_transferred)
{
    assertThreadIsMain();
    mLastWrite = mApp.getClock().now();

    if (error)
    {
        if (isConnected())
        {
                                    getOverlayMetrics().mErrorWrite.Mark();
            CLOG(ERROR, "Overlay")
                << "Error during sending message to " << toString();
        }
        if (mDelayedShutdown)
        {
                        shutdown();
        }
        else
        {
                        drop("error during write", Peer::DropDirection::WE_DROPPED_REMOTE,
                 Peer::DropMode::IGNORE_WRITE_QUEUE);
        }
    }
    else if (bytes_transferred != 0)
    {
        LoadManager::PeerContext loadCtx(mApp, mPeerID);
        getOverlayMetrics().mMessageWrite.Mark();
        getOverlayMetrics().mByteWrite.Mark(bytes_transferred);
    }
}

void
TCPPeer::startRead()
{
    assertThreadIsMain();
    if (shouldAbort())
    {
        return;
    }

    auto self = static_pointer_cast<TCPPeer>(shared_from_this());

    assert(self->mIncomingHeader.size() == 0);

    if (Logging::logTrace("Overlay"))
        CLOG(TRACE, "Overlay") << "TCPPeer::startRead to " << self->toString();

    self->mIncomingHeader.resize(4);
    asio::async_read(*(self->mSocket.get()),
                     asio::buffer(self->mIncomingHeader),
                     [self](asio::error_code ec, std::size_t length) {
                         if (Logging::logTrace("Overlay"))
                             CLOG(TRACE, "Overlay")
                                 << "TCPPeer::startRead calledback " << ec
                                 << " length:" << length;
                         self->readHeaderHandler(ec, length);
                     });
}

int
TCPPeer::getIncomingMsgLength()
{
    int length = mIncomingHeader[0];
    length &= 0x7f; // clear the XDR 'continuation' bit
    length <<= 8;
    length |= mIncomingHeader[1];
    length <<= 8;
    length |= mIncomingHeader[2];
    length <<= 8;
    length |= mIncomingHeader[3];
    if (length <= 0 ||
        (!isAuthenticated() && (length > MAX_UNAUTH_MESSAGE_SIZE)) ||
        length > MAX_MESSAGE_SIZE)
    {
        getOverlayMetrics().mErrorRead.Mark();
        CLOG(ERROR, "Overlay")
            << "TCP: message size unacceptable: " << length
            << (isAuthenticated() ? "" : " while not authenticated");
        drop("error during read", Peer::DropDirection::WE_DROPPED_REMOTE,
             Peer::DropMode::IGNORE_WRITE_QUEUE);
        length = 0;
    }
    return (length);
}

void
TCPPeer::connected()
{
    startRead();
}

void
TCPPeer::readHeaderHandler(asio::error_code const& error,
                           std::size_t bytes_transferred)
{
    assertThreadIsMain();
                
    if (!error)
    {
        receivedBytes(bytes_transferred, false);
        int length = getIncomingMsgLength();
        if (length != 0)
        {
            mIncomingBody.resize(length);
            auto self = static_pointer_cast<TCPPeer>(shared_from_this());
            asio::async_read(*mSocket.get(), asio::buffer(mIncomingBody),
                             [self](asio::error_code ec, std::size_t length) {
                                 self->readBodyHandler(ec, length);
                             });
        }
    }
    else
    {
        if (isConnected())
        {
                                    getOverlayMetrics().mErrorRead.Mark();
            CLOG(DEBUG, "Overlay")
                << "readHeaderHandler error: " << error.message() << ": "
                << toString();
        }
        drop("error during read", Peer::DropDirection::WE_DROPPED_REMOTE,
             Peer::DropMode::IGNORE_WRITE_QUEUE);
    }
}

void
TCPPeer::readBodyHandler(asio::error_code const& error,
                         std::size_t bytes_transferred)
{
    assertThreadIsMain();
                
    if (!error)
    {
        receivedBytes(bytes_transferred, true);
        recvMessage();
        mIncomingHeader.clear();
        startRead();
    }
    else
    {
        if (isConnected())
        {
                                    getOverlayMetrics().mErrorRead.Mark();
            CLOG(ERROR, "Overlay")
                << "readBodyHandler error: " << error.message() << " :"
                << toString();
        }
        drop("error during read", Peer::DropDirection::WE_DROPPED_REMOTE,
             Peer::DropMode::IGNORE_WRITE_QUEUE);
    }
}

void
TCPPeer::recvMessage()
{
    assertThreadIsMain();
    try
    {
        xdr::xdr_get g(mIncomingBody.data(),
                       mIncomingBody.data() + mIncomingBody.size());
        AuthenticatedMessage am;
        xdr::xdr_argpack_archive(g, am);
        Peer::recvMessage(am);
    }
    catch (xdr::xdr_runtime_error& e)
    {
        CLOG(ERROR, "Overlay") << "recvMessage got a corrupt xdr: " << e.what();
        sendErrorAndDrop(ERR_DATA, "received corrupt XDR",
                         Peer::DropMode::IGNORE_WRITE_QUEUE);
    }
}

void
TCPPeer::drop(std::string const& reason, DropDirection dropDirection,
              DropMode dropMode)
{
    assertThreadIsMain();
    if (shouldAbort())
    {
        return;
    }

    if (mState != GOT_AUTH)
    {
        CLOG(DEBUG, "Overlay") << "TCPPeer::drop " << toString() << " in state "
                               << mState << " we called:" << mRole;
    }
    else if (dropDirection == Peer::DropDirection::WE_DROPPED_REMOTE)
    {
        CLOG(INFO, "Overlay")
            << "Dropping peer " << toString() << "; reason: " << reason;
    }
    else
    {
        CLOG(INFO, "Overlay")
            << "Peer " << toString() << " dropped us; reason: " << reason;
    }

    mState = CLOSING;

    auto self = static_pointer_cast<TCPPeer>(shared_from_this());
    getApp().getOverlayManager().removePeer(this);

        if ((dropMode == Peer::DropMode::IGNORE_WRITE_QUEUE) || !mWriting)
    {
        self->shutdown();
    }
    else
    {
        self->mDelayedShutdown = true;
    }
}
}

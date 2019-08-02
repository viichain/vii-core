#pragma once


#include "overlay/Peer.h"
#include "util/Timer.h"
#include <queue>

namespace medida
{
class Meter;
}

namespace viichain
{

static auto const MAX_UNAUTH_MESSAGE_SIZE = 0x1000;
static auto const MAX_MESSAGE_SIZE = 0x1000000;

class TCPPeer : public Peer
{
  public:
    typedef asio::buffered_stream<asio::ip::tcp::socket> SocketType;

  private:
    std::shared_ptr<SocketType> mSocket;
    std::vector<uint8_t> mIncomingHeader;
    std::vector<uint8_t> mIncomingBody;

    std::queue<std::shared_ptr<xdr::msg_ptr>> mWriteQueue;
    bool mWriting{false};
    bool mDelayedShutdown{false};
    bool mShutdownScheduled{false};

    void recvMessage();
    void sendMessage(xdr::msg_ptr&& xdrBytes) override;

    void messageSender();

    int getIncomingMsgLength();
    virtual void connected() override;
    void startRead();

    void writeHandler(asio::error_code const& error,
                      std::size_t bytes_transferred) override;
    void readHeaderHandler(asio::error_code const& error,
                           std::size_t bytes_transferred) override;
    void readBodyHandler(asio::error_code const& error,
                         std::size_t bytes_transferred) override;
    void shutdown();

  public:
    typedef std::shared_ptr<TCPPeer> pointer;

    TCPPeer(Application& app, Peer::PeerRole role,
            std::shared_ptr<SocketType> socket); // hollow
                                                                                                                                                   
    static pointer initiate(Application& app, PeerBareAddress const& address);
    static pointer accept(Application& app, std::shared_ptr<SocketType> socket);

    virtual ~TCPPeer();

    virtual void drop(std::string const& reason, DropDirection dropDirection,
                      DropMode dropMode) override;

    std::string getIP() const override;
};
}

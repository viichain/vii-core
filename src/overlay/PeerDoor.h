#pragma once


#include "util/asio.h"
#include "TCPPeer.h"
#include <memory>

namespace viichain
{
class Application;
class PeerDoorStub;

class PeerDoor
{
  protected:
    Application& mApp;
    asio::ip::tcp::acceptor mAcceptor;

    virtual void acceptNextPeer();
    virtual void handleKnock(std::shared_ptr<TCPPeer::SocketType> pSocket);

    friend PeerDoorStub;

  public:
    typedef std::shared_ptr<PeerDoor> pointer;

    PeerDoor(Application&);

    void start();
    void close();
};
}

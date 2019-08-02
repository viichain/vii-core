#pragma once


#include "main/Config.h"
#include "xdr/vii-overlay.h"

namespace viichain
{

class Application;

class PeerBareAddress
{
  public:
    enum class Type
    {
        EMPTY,
        IPv4
    };

    PeerBareAddress();
    explicit PeerBareAddress(std::string ip, unsigned short port);
    explicit PeerBareAddress(PeerAddress const& pa);

    static PeerBareAddress
    resolve(std::string const& ipPort, Application& app,
            unsigned short defaultPort = DEFAULT_PEER_PORT);

    bool
    isEmpty() const
    {
        return mType == Type::EMPTY;
    }

    Type
    getType() const
    {
        return mType;
    }

    std::string const&
    getIP() const
    {
        return mIP;
    }

    unsigned short
    getPort() const
    {
        return mPort;
    }

    std::string toString() const;

    bool isPrivate() const;
    bool isLocalhost() const;

    friend bool operator==(PeerBareAddress const& x, PeerBareAddress const& y);
    friend bool operator!=(PeerBareAddress const& x, PeerBareAddress const& y);
    friend bool operator<(PeerBareAddress const& x, PeerBareAddress const& y);

  private:
    Type mType;
    std::string mIP;
    unsigned short mPort;
};
}

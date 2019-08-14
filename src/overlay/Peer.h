#pragma once


#include "util/asio.h"
#include "database/Database.h"
#include "overlay/PeerBareAddress.h"
#include "overlay/VIIXDR.h"
#include "util/NonCopyable.h"
#include "util/Timer.h"
#include "xdrpp/message.h"

namespace medida
{
class Timer;
class Meter;
}

namespace viichain
{

typedef std::shared_ptr<SCPQuorumSet> SCPQuorumSetPtr;

class Application;
class LoopbackPeer;
struct OverlayMetrics;

class Peer : public std::enable_shared_from_this<Peer>,
             public NonMovableOrCopyable
{

  public:
    typedef std::shared_ptr<Peer> pointer;

    enum PeerState
    {
        CONNECTING = 0,
        CONNECTED = 1,
        GOT_HELLO = 2,
        GOT_AUTH = 3,
        CLOSING = 4
    };

    enum PeerRole
    {
        REMOTE_CALLED_US,
        WE_CALLED_REMOTE
    };

    enum class DropMode
    {
        FLUSH_WRITE_QUEUE,
        IGNORE_WRITE_QUEUE
    };

    enum class DropDirection
    {
        REMOTE_DROPPED_US,
        WE_DROPPED_REMOTE
    };

  protected:
    Application& mApp;

    PeerRole mRole;
    PeerState mState;
    NodeID mPeerID;
    uint256 mSendNonce;
    uint256 mRecvNonce;

    HmacSha256Key mSendMacKey;
    HmacSha256Key mRecvMacKey;
    uint64_t mSendMacSeq{0};
    uint64_t mRecvMacSeq{0};

    std::string mRemoteVersion;
    uint32_t mRemoteOverlayMinVersion;
    uint32_t mRemoteOverlayVersion;
    PeerBareAddress mAddress;

    VirtualTimer mIdleTimer;
    VirtualClock::time_point mLastRead;
    VirtualClock::time_point mLastWrite;
    VirtualClock::time_point mLastEmpty;

    OverlayMetrics& getOverlayMetrics();

    bool shouldAbort() const;
    void recvMessage(VIIMessage const& msg);
    void recvMessage(AuthenticatedMessage const& msg);
    void recvMessage(xdr::msg_ptr const& xdrBytes);

    virtual void recvError(VIIMessage const& msg);
    void updatePeerRecordAfterEcho();
    void updatePeerRecordAfterAuthentication();
    void recvAuth(VIIMessage const& msg);
    void recvDontHave(VIIMessage const& msg);
    void recvGetPeers(VIIMessage const& msg);
    void recvHello(Hello const& elo);
    void recvPeers(VIIMessage const& msg);

    void recvGetTxSet(VIIMessage const& msg);
    void recvTxSet(VIIMessage const& msg);
    void recvTransaction(VIIMessage const& msg);
    void recvGetSCPQuorumSet(VIIMessage const& msg);
    void recvSCPQuorumSet(VIIMessage const& msg);
    void recvSCPMessage(VIIMessage const& msg);
    void recvGetSCPState(VIIMessage const& msg);

    void sendHello();
    void sendAuth();
    void sendSCPQuorumSet(SCPQuorumSetPtr qSet);
    void sendDontHave(MessageType type, uint256 const& itemID);
    void sendPeers();
    void sendError(ErrorCode error, std::string const& message);

                                virtual void sendMessage(xdr::msg_ptr&& xdrBytes) = 0;
    virtual void
    connected()
    {
    }

    virtual AuthCert getAuthCert();

    void startIdleTimer();
    void idleTimerExpired(asio::error_code const& error);
    std::chrono::seconds getIOTimeout() const;

        void receivedBytes(size_t byteCount, bool gotFullMessage);

  public:
    Peer(Application& app, PeerRole role);

    Application&
    getApp()
    {
        return mApp;
    }

    void sendGetTxSet(uint256 const& setID);
    void sendGetQuorumSet(uint256 const& setID);
    void sendGetPeers();
    void sendGetScpState(uint32 ledgerSeq);
    void sendErrorAndDrop(ErrorCode error, std::string const& message,
                          DropMode dropMode);

    void sendMessage(VIIMessage const& msg);

    PeerRole
    getRole() const
    {
        return mRole;
    }

    bool isConnected() const;
    bool isAuthenticated() const;

    PeerState
    getState() const
    {
        return mState;
    }

    std::string const&
    getRemoteVersion() const
    {
        return mRemoteVersion;
    }

    uint32_t
    getRemoteOverlayMinVersion() const
    {
        return mRemoteOverlayMinVersion;
    }

    uint32_t
    getRemoteOverlayVersion() const
    {
        return mRemoteOverlayVersion;
    }

    PeerBareAddress const&
    getAddress()
    {
        return mAddress;
    }

    NodeID
    getPeerID()
    {
        return mPeerID;
    }

    std::string toString();
    virtual std::string getIP() const = 0;

            virtual void connectHandler(asio::error_code const& ec);

    virtual void
    writeHandler(asio::error_code const& error, size_t bytes_transferred)
    {
    }

    virtual void
    readHeaderHandler(asio::error_code const& error, size_t bytes_transferred)
    {
    }

    virtual void
    readBodyHandler(asio::error_code const& error, size_t bytes_transferred)
    {
    }

    virtual void drop(std::string const& reason, DropDirection dropDirection,
                      DropMode dropMode) = 0;
    virtual ~Peer()
    {
    }

    friend class LoopbackPeer;
};
}

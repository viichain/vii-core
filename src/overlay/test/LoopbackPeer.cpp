
#include "overlay/test/LoopbackPeer.h"
#include "crypto/Random.h"
#include "main/Application.h"
#include "medida/timer.h"
#include "overlay/LoadManager.h"
#include "overlay/OverlayManager.h"
#include "overlay/OverlayMetrics.h"
#include "overlay/StellarXDR.h"
#include "util/Logging.h"
#include "util/Math.h"
#include "xdrpp/marshal.h"

namespace viichain
{

using namespace std;


LoopbackPeer::LoopbackPeer(Application& app, PeerRole role) : Peer(app, role)
{
}

std::string
LoopbackPeer::getIP() const
{
    return "127.0.0.1";
}

AuthCert
LoopbackPeer::getAuthCert()
{
    auto c = Peer::getAuthCert();
    if (mDamageCert)
    {
        c.expiration++;
    }
    return c;
}

void
LoopbackPeer::sendMessage(xdr::msg_ptr&& msg)
{
    if (mRemote.expired())
    {
        drop("remote expired", Peer::DropDirection::WE_DROPPED_REMOTE,
             Peer::DropMode::IGNORE_WRITE_QUEUE);
        return;
    }

        if (mDamageAuth)
    {
        auto bytes = randomBytes(mRecvMacKey.key.size());
        std::copy(bytes.begin(), bytes.end(), mRecvMacKey.key.begin());
    }

        mOutQueue.emplace_back(std::move(msg));
        while (mOutQueue.size() > mMaxQueueDepth && !mCorked)
    {
                                auto remote = mRemote.lock();
        if (remote && remote->getStraggling())
        {
            if (rand_flip() || rand_flip())
            {
                CLOG(INFO, "Overlay")
                    << "Loopback send-to-straggler pausing, "
                    << "outbound queue at " << mOutQueue.size();
                break;
            }
            else
            {
                CLOG(INFO, "Overlay")
                    << "Loopback send-to-straggler sending, "
                    << "outbound queue at " << mOutQueue.size();
            }
        }
        deliverOne();
    }
}

void
LoopbackPeer::drop(std::string const& reason, DropDirection direction, DropMode)
{
    if (mState == CLOSING)
    {
        return;
    }

    mDropReason = reason;
    mState = CLOSING;
    mIdleTimer.cancel();
    getApp().getOverlayManager().removePeer(this);

    auto remote = mRemote.lock();
    if (remote)
    {
        remote->getApp().postOnMainThread(
            [remote, reason, direction]() {
                remote->drop("remote dropping because of " + reason,
                             direction == Peer::DropDirection::WE_DROPPED_REMOTE
                                 ? Peer::DropDirection::REMOTE_DROPPED_US
                                 : Peer::DropDirection::WE_DROPPED_REMOTE,
                             Peer::DropMode::IGNORE_WRITE_QUEUE);

            },
            "LoopbackPeer: drop");
    }
}

static bool
damageMessage(default_random_engine& gen, xdr::msg_ptr& msg)
{
    size_t bitsFlipped = 0;
    char* d = msg->raw_data();
    char* e = msg->end();
    size_t sz = e - d;
    if (sz > 0)
    {
        auto dist = uniform_int_distribution<size_t>(0, sz - 1);
        auto byteDist = uniform_int_distribution<int>(0, 7);
        size_t nDamage = dist(gen);
        while (nDamage != 0)
        {
            --nDamage;
            auto pos = dist(gen);
            d[pos] ^= 1 << byteDist(gen);
            bitsFlipped++;
        }
    }
    return bitsFlipped != 0;
}

static xdr::msg_ptr
duplicateMessage(xdr::msg_ptr const& msg)
{
    xdr::msg_ptr msg2 = xdr::message_t::alloc(msg->size());
    memcpy(msg2->raw_data(), msg->raw_data(), msg->raw_size());
    return msg2;
}

void
LoopbackPeer::processInQueue()
{
    if (!mInQueue.empty() && mState != CLOSING)
    {
        auto const& m = mInQueue.front();
        receivedBytes(m->size(), true);
        recvMessage(m);
        mInQueue.pop();

        if (!mInQueue.empty())
        {
            auto self = static_pointer_cast<LoopbackPeer>(shared_from_this());
            mApp.postOnMainThread([self]() { self->processInQueue(); },
                                  "LoopbackPeer: processInQueue");
        }
    }
}

void
LoopbackPeer::deliverOne()
{
        if (mRemote.expired())
    {
        return;
    }

    if (!mOutQueue.empty() && !mCorked)
    {
        xdr::msg_ptr msg = std::move(mOutQueue.front());
        mOutQueue.pop_front();

        
                if (mDuplicateProb(gRandomEngine))
        {
            CLOG(INFO, "Overlay") << "LoopbackPeer duplicated message";
            mOutQueue.emplace_front(duplicateMessage(msg));
            mStats.messagesDuplicated++;
        }

                if (mReorderProb(gRandomEngine) && mOutQueue.size() > 0)
        {
            CLOG(INFO, "Overlay") << "LoopbackPeer reordered message";
            mStats.messagesReordered++;
            mOutQueue.emplace_back(std::move(msg));
            return;
        }

                if (mDamageProb(gRandomEngine))
        {
            CLOG(INFO, "Overlay") << "LoopbackPeer damaged message";
            if (damageMessage(gRandomEngine, msg))
                mStats.messagesDamaged++;
        }

                if (mDropProb(gRandomEngine))
        {
            CLOG(INFO, "Overlay") << "LoopbackPeer dropped message";
            mStats.messagesDropped++;
            return;
        }

        size_t nBytes = msg->raw_size();
        mStats.bytesDelivered += nBytes;

                                auto remote = mRemote.lock();
        if (remote)
        {
                        remote->mInQueue.emplace(std::move(msg));
            remote->getApp().postOnMainThread(
                [remote]() { remote->processInQueue(); },
                "LoopbackPeer: processInQueue in deliverOne");
        }
        LoadManager::PeerContext loadCtx(mApp, mPeerID);
        mLastWrite = mApp.getClock().now();
        if (mOutQueue.empty())
        {
            mLastEmpty = mApp.getClock().now();
        }
        getOverlayMetrics().mMessageWrite.Mark();
        getOverlayMetrics().mByteWrite.Mark(nBytes);

            }
}

void
LoopbackPeer::deliverAll()
{
    while (!mOutQueue.empty() && !mCorked)
    {
        deliverOne();
    }
}

void
LoopbackPeer::dropAll()
{
    mOutQueue.clear();
}

size_t
LoopbackPeer::getBytesQueued() const
{
    size_t t = 0;
    for (auto const& m : mOutQueue)
    {
        t += m->raw_size();
    }
    return t;
}

size_t
LoopbackPeer::getMessagesQueued() const
{
    return mOutQueue.size();
}

LoopbackPeer::Stats const&
LoopbackPeer::getStats() const
{
    return mStats;
}

bool
LoopbackPeer::getCorked() const
{
    return mCorked;
}

void
LoopbackPeer::setCorked(bool c)
{
    mCorked = c;
}

bool
LoopbackPeer::getStraggling() const
{
    return mStraggling;
}

void
LoopbackPeer::setStraggling(bool s)
{
    mStraggling = s;
}

size_t
LoopbackPeer::getMaxQueueDepth() const
{
    return mMaxQueueDepth;
}

void
LoopbackPeer::setMaxQueueDepth(size_t sz)
{
    mMaxQueueDepth = sz;
}

double
LoopbackPeer::getDamageProbability() const
{
    return mDamageProb.p();
}

static void
checkProbRange(double d)
{
    if (d < 0.0 || d > 1.0)
    {
        throw std::runtime_error("probability out of range");
    }
}

void
LoopbackPeer::setDamageProbability(double d)
{
    checkProbRange(d);
    mDamageProb = bernoulli_distribution(d);
}

double
LoopbackPeer::getDropProbability() const
{
    return mDropProb.p();
}

void
LoopbackPeer::setDamageCert(bool b)
{
    mDamageCert = b;
}

bool
LoopbackPeer::getDamageCert() const
{
    return mDamageCert;
}

void
LoopbackPeer::setDamageAuth(bool b)
{
    mDamageAuth = b;
}

bool
LoopbackPeer::getDamageAuth() const
{
    return mDamageAuth;
}

void
LoopbackPeer::setDropProbability(double d)
{
    checkProbRange(d);
    mDropProb = bernoulli_distribution(d);
}

double
LoopbackPeer::getDuplicateProbability() const
{
    return mDuplicateProb.p();
}

void
LoopbackPeer::setDuplicateProbability(double d)
{
    checkProbRange(d);
    mDuplicateProb = bernoulli_distribution(d);
}

double
LoopbackPeer::getReorderProbability() const
{
    return mReorderProb.p();
}

void
LoopbackPeer::setReorderProbability(double d)
{
    checkProbRange(d);
    mReorderProb = bernoulli_distribution(d);
}

LoopbackPeerConnection::LoopbackPeerConnection(Application& initiator,
                                               Application& acceptor)
    : mInitiator(make_shared<LoopbackPeer>(initiator, Peer::WE_CALLED_REMOTE))
    , mAcceptor(make_shared<LoopbackPeer>(acceptor, Peer::REMOTE_CALLED_US))
{
    mInitiator->mRemote = mAcceptor;
    mInitiator->mState = Peer::CONNECTED;

    mAcceptor->mRemote = mInitiator;
    mAcceptor->mState = Peer::CONNECTED;

    initiator.getOverlayManager().addOutboundConnection(mInitiator);
    acceptor.getOverlayManager().addInboundConnection(mAcceptor);

            if (mInitiator->mState != Peer::CONNECTED ||
        mAcceptor->mState != Peer::CONNECTED)
    {
        return;
    }

    mInitiator->startIdleTimer();
    mAcceptor->startIdleTimer();

    auto init = mInitiator;
    mInitiator->getApp().postOnMainThread(
        [init]() { init->connectHandler(asio::error_code()); },
        "LoopbackPeer: connect");
}

LoopbackPeerConnection::~LoopbackPeerConnection()
{
            mInitiator->drop("loopback destruction",
                     Peer::DropDirection::WE_DROPPED_REMOTE,
                     Peer::DropMode::IGNORE_WRITE_QUEUE);
}

std::shared_ptr<LoopbackPeer>
LoopbackPeerConnection::getInitiator() const
{
    return mInitiator;
}

std::shared_ptr<LoopbackPeer>
LoopbackPeerConnection::getAcceptor() const
{
    return mAcceptor;
}
}
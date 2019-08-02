
#include "util/Timer.h"
#include "main/Application.h"
#include "util/GlobalChecks.h"
#include "util/Logging.h"
#include <chrono>
#include <cstdio>
#include <thread>

namespace viichain
{

using namespace std;

static const uint32_t RECENT_CRANK_WINDOW = 1024;

VirtualClock::VirtualClock(Mode mode) : mMode(mode), mRealTimer(mIOContext)
{
    resetIdleCrankPercent();
}

VirtualClock::time_point
VirtualClock::now() noexcept
{
    if (mMode == REAL_TIME)
    {
        return std::chrono::system_clock::now();
    }
    else
    {
        return mVirtualNow;
    }
}

void
VirtualClock::maybeSetRealtimer()
{
    if (mMode == REAL_TIME)
    {
        auto nextp = next();
        if (nextp != mRealTimer.expires_at())
        {
            mRealTimer.expires_at(nextp);
            mRealTimer.async_wait([this](asio::error_code const& ec) {
                if (ec == asio::error::operation_aborted)
                {
                    ++this->nRealTimerCancelEvents;
                }
                else
                {
                    this->advanceToNow();
                }
            });
        }
    }
}

bool
VirtualClockEventCompare::operator()(shared_ptr<VirtualClockEvent> a,
                                     shared_ptr<VirtualClockEvent> b)
{
    return *a < *b;
}

VirtualClock::time_point
VirtualClock::next()
{
    assertThreadIsMain();
    VirtualClock::time_point least = time_point::max();
    if (!mEvents.empty())
    {
        if (mEvents.top()->mWhen < least)
        {
            least = mEvents.top()->mWhen;
        }
    }
    return least;
}

VirtualClock::time_point
VirtualClock::from_time_t(std::time_t timet)
{
    time_point epoch;
    return epoch + std::chrono::seconds(timet);
}

std::time_t
VirtualClock::to_time_t(time_point point)
{
    return static_cast<std::time_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            point.time_since_epoch())
            .count());
}

#ifdef _WIN32
time_t
timegm(struct tm* tm)
{
        return _mkgmtime(tm);
}
#endif

std::tm
VirtualClock::pointToTm(time_point point)
{
    std::time_t rawtime = to_time_t(point);
    std::tm out;
#ifdef _WIN32
        std::tm* tmPtr = gmtime(&rawtime);
    out = *tmPtr;
#else
        gmtime_r(&rawtime, &out);
#endif
    return out;
}

VirtualClock::time_point
VirtualClock::tmToPoint(tm t)
{
    time_t tt = timegm(&t);
    return VirtualClock::time_point() + std::chrono::seconds(tt);
}

std::tm
VirtualClock::isoStringToTm(std::string const& iso)
{
    std::tm res;
    int y, M, d, h, m, s;
    if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%dZ", &y, &M, &d, &h, &m,
                    &s) != 6)
    {
        throw std::invalid_argument("Could not parse iso date");
    }
    std::memset(&res, 0, sizeof(res));
    res.tm_year = y - 1900;
    res.tm_mon = M - 1;
    res.tm_mday = d;
    res.tm_hour = h;
    res.tm_min = m;
    res.tm_sec = s;
    res.tm_isdst = 0;
    res.tm_wday = 0;
    res.tm_yday = 0;
    return res;
}

std::string
VirtualClock::tmToISOString(std::tm const& tm)
{
    char buf[sizeof("0000-00-00T00:00:00Z")];
    size_t conv = strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    assert(conv == (sizeof(buf) - 1));
    return std::string(buf);
}

std::string
VirtualClock::pointToISOString(time_point point)
{
    return tmToISOString(pointToTm(point));
}

void
VirtualClock::enqueue(shared_ptr<VirtualClockEvent> ve)
{
    if (mDestructing)
    {
        return;
    }
    assertThreadIsMain();
        mEvents.emplace(ve);
    maybeSetRealtimer();
}

void
VirtualClock::flushCancelledEvents()
{
    if (mDestructing)
    {
        return;
    }
    if (mFlushesIgnored <= mEvents.size())
    {
                                                mFlushesIgnored++;
        return;
    }
    assertThreadIsMain();
    
    auto toKeep = vector<shared_ptr<VirtualClockEvent>>();
    toKeep.reserve(mEvents.size());

    while (!mEvents.empty())
    {
        auto const& e = mEvents.top();
        if (!e->getTriggered())
        {
            toKeep.emplace_back(e);
        }
        mEvents.pop();
    }
    mFlushesIgnored = 0;
    mEvents = PrQueue(toKeep.begin(), toKeep.end());
    maybeSetRealtimer();
}

bool
VirtualClock::cancelAllEvents()
{
    assertThreadIsMain();

    bool wasEmpty = mEvents.empty();
    while (!mEvents.empty())
    {
        auto ev = mEvents.top();
        mEvents.pop();
        ev->cancel();
    }
    mEvents = PrQueue();
    return !wasEmpty;
}

void
VirtualClock::setCurrentVirtualTime(time_point t)
{
    assert(mMode == VIRTUAL_TIME);
    mVirtualNow = t;
}

size_t
VirtualClock::crank(bool block)
{
    if (mDestructing)
    {
        return 0;
    }

    size_t nWorkDone = 0;

    {
        std::lock_guard<std::recursive_mutex> lock(mDelayExecutionMutex);
        
        mDelayExecution = true;

        nRealTimerCancelEvents = 0;

        if (mMode == REAL_TIME)
        {
                                    nWorkDone += advanceToNow();
        }

                                const size_t WORK_BATCH_SIZE = 100;
        size_t lastPoll;
        size_t i = 0;
        do
        {
                        lastPoll = mIOContext.poll_one();
            nWorkDone += lastPoll;
        } while (lastPoll != 0 && ++i < WORK_BATCH_SIZE);

        nWorkDone -= nRealTimerCancelEvents;

        if (!mDelayedExecutionQueue.empty())
        {
                                                nWorkDone++;
            for (auto&& f : mDelayedExecutionQueue)
            {
                asio::post(mIOContext, std::move(f));
            }
            mDelayedExecutionQueue.clear();
        }

        if (mMode == VIRTUAL_TIME && nWorkDone == 0)
        {
                                                nWorkDone += advanceToNext();
        }

        mDelayExecution = false;
    }

        if (block && nWorkDone == 0)
    {
        nWorkDone += mIOContext.run_one();
    }

    noteCrankOccurred(nWorkDone == 0);

    return nWorkDone;
}

void
VirtualClock::postToCurrentCrank(std::function<void()>&& f)
{
    asio::post(mIOContext, std::move(f));
}

void
VirtualClock::postToNextCrank(std::function<void()>&& f)
{
    std::lock_guard<std::recursive_mutex> lock(mDelayExecutionMutex);

    if (!mDelayExecution)
    {
                                                                
                mDelayExecution = true;
        asio::post(mIOContext, std::move(f));
    }
    else
    {
        mDelayedExecutionQueue.emplace_back(std::move(f));
    }
}

void
VirtualClock::noteCrankOccurred(bool hadIdle)
{
                ++mRecentCrankCount;
    if (hadIdle)
    {
        ++mRecentIdleCrankCount;
    }

                                            if (mRecentCrankCount > RECENT_CRANK_WINDOW)
    {
        mRecentCrankCount >>= 1;
        mRecentIdleCrankCount >>= 1;
    }
}

uint32_t
VirtualClock::recentIdleCrankPercent() const
{
    if (mRecentCrankCount == 0)
    {
        return 0;
    }
    uint32_t v = static_cast<uint32_t>(
        (100ULL * static_cast<uint64_t>(mRecentIdleCrankCount)) /
        static_cast<uint64_t>(mRecentCrankCount));

    LOG(DEBUG) << "Estimated clock loop idle: " << v << "% ("
               << mRecentIdleCrankCount << "/" << mRecentCrankCount << ")";

        assert(v <= 100);

    return v;
}

void
VirtualClock::resetIdleCrankPercent()
{
    mRecentCrankCount = RECENT_CRANK_WINDOW >> 1;
    mRecentIdleCrankCount = RECENT_CRANK_WINDOW >> 1;
}

asio::io_context&
VirtualClock::getIOContext()
{
    return mIOContext;
}

VirtualClock::~VirtualClock()
{
    mDestructing = true;
    cancelAllEvents();
}

size_t
VirtualClock::advanceToNow()
{
    if (mDestructing)
    {
        return 0;
    }
    assertThreadIsMain();

            auto n = now();
    vector<shared_ptr<VirtualClockEvent>> toDispatch;
    while (!mEvents.empty())
    {
        if (mEvents.top()->mWhen > n)
            break;
        toDispatch.push_back(mEvents.top());
        mEvents.pop();
    }
                for (auto ev : toDispatch)
    {
        ev->trigger();
    }
        maybeSetRealtimer();
    return toDispatch.size();
}

size_t
VirtualClock::advanceToNext()
{
    if (mDestructing)
    {
        return 0;
    }
    assert(mMode == VIRTUAL_TIME);
    assertThreadIsMain();
    if (mEvents.empty())
    {
        return 0;
    }

    mVirtualNow = next();
    return advanceToNow();
}

VirtualClockEvent::VirtualClockEvent(
    VirtualClock::time_point when, size_t seq,
    std::function<void(asio::error_code)> callback)
    : mCallback(callback), mTriggered(false), mWhen(when), mSeq(seq)
{
}

bool
VirtualClockEvent::getTriggered()
{
    return mTriggered;
}
void
VirtualClockEvent::trigger()
{
    if (!mTriggered)
    {
        mTriggered = true;
        mCallback(asio::error_code());
        mCallback = nullptr;
    }
}

void
VirtualClockEvent::cancel()
{
    if (!mTriggered)
    {
        mTriggered = true;
        mCallback(asio::error::operation_aborted);
        mCallback = nullptr;
    }
}

bool
VirtualClockEvent::operator<(VirtualClockEvent const& other) const
{
                                return mWhen > other.mWhen || (mWhen == other.mWhen && mSeq > other.mSeq);
}

VirtualTimer::VirtualTimer(Application& app) : VirtualTimer(app.getClock())
{
}

VirtualTimer::VirtualTimer(VirtualClock& clock)
    : mClock(clock)
    , mExpiryTime(mClock.now())
    , mCancelled(false)
    , mDeleting(false)
{
}

VirtualTimer::~VirtualTimer()
{
    mDeleting = true;
    cancel();
}

void
VirtualTimer::cancel()
{
    if (!mCancelled)
    {
        mCancelled = true;
        for (auto ev : mEvents)
        {
            ev->cancel();
        }
        mClock.flushCancelledEvents();
        mEvents.clear();
    }
}

VirtualClock::time_point const&
VirtualTimer::expiry_time() const
{
    return mExpiryTime;
}

size_t
VirtualTimer::seq() const
{
    return mEvents.empty() ? 0 : mEvents.back()->mSeq + 1;
}

void
VirtualTimer::expires_at(VirtualClock::time_point t)
{
    cancel();
    mExpiryTime = t;
    mCancelled = false;
}

void
VirtualTimer::expires_from_now(VirtualClock::duration d)
{
    cancel();
    mExpiryTime = mClock.now() + d;
    mCancelled = false;
}

void
VirtualTimer::async_wait(function<void(asio::error_code)> const& fn)
{
    if (!mCancelled)
    {
        assert(!mDeleting);
        auto ve = make_shared<VirtualClockEvent>(mExpiryTime, seq(), fn);
        mClock.enqueue(ve);
        mEvents.push_back(ve);
    }
}

void
VirtualTimer::async_wait(std::function<void()> const& onSuccess,
                         std::function<void(asio::error_code)> const& onFailure)
{
    if (!mCancelled)
    {
        assert(!mDeleting);
        auto ve = make_shared<VirtualClockEvent>(
            mExpiryTime, seq(), [onSuccess, onFailure](asio::error_code error) {
                if (error)
                    onFailure(error);
                else
                    onSuccess();
            });
        mClock.enqueue(ve);
        mEvents.push_back(ve);
    }
}
}

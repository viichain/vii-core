#pragma once


#include "util/asio.h"
#include "util/NonCopyable.h"

#include <chrono>
#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>

namespace viichain
{

class VirtualTimer;
class Application;
class VirtualClockEvent;
class VirtualClockEventCompare
{
  public:
    bool operator()(std::shared_ptr<VirtualClockEvent> a,
                    std::shared_ptr<VirtualClockEvent> b);
};

class VirtualClock
{
  public:
            static_assert(sizeof(uint64_t) == sizeof(std::time_t),
                  "Require 64bit time_t");

                typedef std::chrono::system_clock::duration duration;
    typedef duration::rep rep;
    typedef duration::period period;
    typedef std::chrono::system_clock::time_point time_point;
    static const bool is_steady = false;

            static std::time_t to_time_t(time_point);
    static time_point from_time_t(std::time_t);

    static std::tm pointToTm(time_point);
    static VirtualClock::time_point tmToPoint(tm t);

    static std::tm isoStringToTm(std::string const& iso);
    static std::string tmToISOString(std::tm const& tm);
    static std::string pointToISOString(time_point point);

    enum Mode
    {
        REAL_TIME,
        VIRTUAL_TIME
    };

  private:
    asio::io_context mIOContext;
    Mode mMode;

    uint32_t mRecentCrankCount;
    uint32_t mRecentIdleCrankCount;

    size_t nRealTimerCancelEvents{0};
    time_point mVirtualNow;

    bool mDelayExecution{true};
    std::recursive_mutex mDelayExecutionMutex;
    std::vector<std::function<void()>> mDelayedExecutionQueue;

    using PrQueue =
        std::priority_queue<std::shared_ptr<VirtualClockEvent>,
                            std::vector<std::shared_ptr<VirtualClockEvent>>,
                            VirtualClockEventCompare>;
    PrQueue mEvents;
    size_t mFlushesIgnored = 0;

    bool mDestructing{false};

    void maybeSetRealtimer();
    size_t advanceToNext();
    size_t advanceToNow();

        asio::basic_waitable_timer<std::chrono::system_clock> mRealTimer;

  public:
                
    VirtualClock(Mode mode = VIRTUAL_TIME);
    ~VirtualClock();
    size_t crank(bool block = true);
    void noteCrankOccurred(bool hadIdle);
    uint32_t recentIdleCrankPercent() const;
    void resetIdleCrankPercent();
    asio::io_context& getIOContext();

                time_point now() noexcept;

    void enqueue(std::shared_ptr<VirtualClockEvent> ve);
    void flushCancelledEvents();
    bool cancelAllEvents();

            void setCurrentVirtualTime(time_point t);

        time_point next();

    void postToCurrentCrank(std::function<void()>&& f);
    void postToNextCrank(std::function<void()>&& f);
};

class VirtualClockEvent : public NonMovableOrCopyable
{
    std::function<void(asio::error_code)> mCallback;
    bool mTriggered;

  public:
    VirtualClock::time_point mWhen;
    size_t mSeq;
    VirtualClockEvent(VirtualClock::time_point when, size_t seq,
                      std::function<void(asio::error_code)> callback);
    bool getTriggered();
    void trigger();
    void cancel();
    bool operator<(VirtualClockEvent const& other) const;
};

class VirtualTimer : private NonMovableOrCopyable
{
    VirtualClock& mClock;
    VirtualClock::time_point mExpiryTime;
    std::vector<std::shared_ptr<VirtualClockEvent>> mEvents;
    bool mCancelled;
    bool mDeleting;

  public:
    VirtualTimer(Application& app);
    VirtualTimer(VirtualClock& app);
    ~VirtualTimer();

    VirtualClock::time_point const& expiry_time() const;
    size_t seq() const;
    void expires_at(VirtualClock::time_point t);
    void expires_from_now(VirtualClock::duration d);
    template <typename R, typename P>
    void
    expires_from_now(std::chrono::duration<R, P> const& d)
    {
        expires_from_now(std::chrono::duration_cast<VirtualClock::duration>(d));
    }
    void async_wait(std::function<void(asio::error_code)> const& fn);
    void async_wait(std::function<void()> const& onSuccess,
                    std::function<void(asio::error_code)> const& onFailure);
    void cancel();

    static void onFailureNoop(asio::error_code const&){};
};

#ifdef STELLAR_CORE_REAL_TIMER_FOR_CERTAIN_NOT_JUST_VIRTUAL_TIME
class RealTimer : public asio::basic_waitable_timer<std::chrono::system_clock>
{
  public:
    RealTimer(asio::io_context& io)
        : asio::basic_waitable_timer<std::chrono::system_clock>(io)
    {
    }
};
#endif
}

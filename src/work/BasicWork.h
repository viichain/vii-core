#pragma once

#include "main/Application.h"
#include <set>

namespace viichain
{

class Application;

class BasicWork : public std::enable_shared_from_this<BasicWork>,
                  private NonMovableOrCopyable
{
  public:
    static size_t const RETRY_NEVER;
    static size_t const RETRY_ONCE;
    static size_t const RETRY_A_FEW;
    static size_t const RETRY_A_LOT;
    static size_t const RETRY_FOREVER;

        enum class State
    {
                                WORK_RUNNING,
                                        WORK_WAITING,
                WORK_ABORTED,
                WORK_SUCCESS,
                                WORK_FAILURE
    };

    BasicWork(Application& app, std::string name, size_t maxRetries);
    virtual ~BasicWork();

    std::string const& getName() const;
    virtual std::string getStatus() const;
    State getState() const;
    bool isDone() const;

        void crankWork();

                        void startWork(std::function<void()> notificationCallback);

                    virtual void shutdown();

    bool
    isAborting() const
    {
        return mState == InternalState::ABORTING;
    }

  protected:
            virtual State onRun() = 0;

                            virtual bool onAbort() = 0;

                                                virtual void onReset();
    virtual void onFailureRetry();
    virtual void onFailureRaise();
    virtual void onSuccess();

                        void wakeUp(std::function<void()> innerCallback = nullptr);

        std::function<void()>
    wakeSelfUpCallback(std::function<void()> innerCallback = nullptr);

    Application& mApp;

  private:
            enum class InternalState
    {
                PENDING,
        RUNNING,
        WAITING,
                                ABORTING,
        ABORTED,
                        RETRYING,
        SUCCESS,
        FAILURE
    };
    using Transition = std::pair<InternalState, InternalState>;

            void reset();

    VirtualClock::duration getRetryDelay() const;
    void setState(InternalState s);
    void waitForRetry();
    InternalState getInternalState(State s) const;
    void assertValidTransition(Transition const& t) const;
    static std::string stateName(InternalState st);
    uint64_t getRetryETA() const;

    std::function<void()> mNotifyCallback;
    std::string const mName;
    std::unique_ptr<VirtualTimer> mRetryTimer;

    InternalState mState{InternalState::PENDING};
    size_t mRetries{0};
    size_t const mMaxRetries{RETRY_A_FEW};

        static std::set<Transition> const ALLOWED_TRANSITIONS;
};
}
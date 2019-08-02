
#include "lib/catch.hpp"
#include "lib/util/format.h"
#include "main/Application.h"
#include "main/Config.h"
#include "process/ProcessManager.h"
#include "test/TestUtils.h"
#include "test/test.h"
#include "work/WorkScheduler.h"

#include "historywork/RunCommandWork.h"
#include "work/BatchWork.h"
#include <cstdio>
#include <fstream>
#include <random>
#include <xdrpp/autocheck.h>

using namespace viichain;

class TestBasicWork : public BasicWork
{
    bool mShouldFail;

  public:
    int const mNumSteps;
    int mCount;
    int mRunningCount{0};
    int mSuccessCount{0};
    int mFailureCount{0};
    int mRetryCount{0};
    int mAbortCount{0};

    TestBasicWork(Application& app, std::string name, bool fail = false,
                  int steps = 3, size_t retries = RETRY_ONCE)
        : BasicWork(app, std::move(name), retries)
        , mShouldFail(fail)
        , mNumSteps(steps)
        , mCount(steps)
    {
    }

    void
    forceWakeUp()
    {
        wakeUp();
    }

  protected:
    BasicWork::State
    onRun() override
    {
        CLOG(DEBUG, "Work") << "Running " << getName();
        mRunningCount++;
        if (--mCount > 0)
        {
            return State::WORK_RUNNING;
        }
        return mShouldFail ? State::WORK_FAILURE : State::WORK_SUCCESS;
    }

    bool
    onAbort() override
    {
        CLOG(DEBUG, "Work") << "Aborting " << getName();
        ++mAbortCount;
        return true;
    }

    void
    onSuccess() override
    {
        ++mSuccessCount;
    }

    void
    onFailureRaise() override
    {
        ++mFailureCount;
    }

    void
    onFailureRetry() override
    {
        ++mRetryCount;
    }

    void
    onReset() override
    {
        mCount = mNumSteps;
    }
};

class TestWaitingWork : public TestBasicWork
{
    VirtualTimer mTimer;

  public:
    int mWaitingCount{0};
    int mWakeUpCount{0};

    TestWaitingWork(Application& app, std::string name)
        : TestBasicWork(app, name), mTimer(app.getClock())
    {
    }

  protected:
    BasicWork::State
    onRun() override
    {
        ++mRunningCount;
        if (--mCount > 0)
        {
            std::weak_ptr<TestWaitingWork> weak(
                std::static_pointer_cast<TestWaitingWork>(shared_from_this()));
            auto handler = [weak](asio::error_code const& ec) {
                auto self = weak.lock();
                if (self)
                {
                    ++(self->mWakeUpCount);
                    self->wakeUp();
                }
            };

            mTimer.expires_from_now(std::chrono::milliseconds(1));
            mTimer.async_wait(handler);

            ++mWaitingCount;
            return BasicWork::State::WORK_WAITING;
        }

        return BasicWork::State::WORK_SUCCESS;
    }
};

TEST_CASE("BasicWork test", "[work][basicwork]")
{
    VirtualClock clock;
    Config const& cfg = getTestConfig();
    Application::pointer appPtr = createTestApplication(clock, cfg);
    auto& wm = appPtr->getWorkScheduler();

    auto checkSuccess = [](TestBasicWork const& w) {
        REQUIRE(w.getState() == TestBasicWork::State::WORK_SUCCESS);
        REQUIRE(w.mRunningCount == w.mNumSteps);
        REQUIRE(w.mSuccessCount == 1);
        REQUIRE(w.mFailureCount == 0);
        REQUIRE(w.mRetryCount == 0);
    };

    SECTION("one work")
    {
        auto w = wm.scheduleWork<TestBasicWork>("test-work");
        while (!wm.allChildrenSuccessful())
        {
            clock.crank();
        }
        checkSuccess(*w);
    }

    SECTION("2 works round robin")
    {
        auto w1 = wm.scheduleWork<TestBasicWork>("test-work1");
        auto w2 = wm.scheduleWork<TestBasicWork>("test-work2");
        while (!wm.allChildrenSuccessful())
        {
            clock.crank();
        }
        checkSuccess(*w1);
        checkSuccess(*w2);
    }
    SECTION("work waiting")
    {
        auto w = wm.scheduleWork<TestWaitingWork>("waiting-test-work");
        while (!wm.allChildrenSuccessful())
        {
            clock.crank();
        }
        checkSuccess(*w);
        REQUIRE(w->mWaitingCount == w->mWakeUpCount);
        REQUIRE(w->mWaitingCount == w->mNumSteps - 1);
    }
    SECTION("new work added midway")
    {
        auto w1 = wm.scheduleWork<TestBasicWork>("test-work1");
        auto w2 = wm.scheduleWork<TestBasicWork>("test-work2");
        std::shared_ptr<TestBasicWork> w3;
        while (!wm.allChildrenSuccessful())
        {
            clock.crank();
            if (!w3)
            {
                w3 = wm.scheduleWork<TestBasicWork>("test-work3");
            };
        }
        checkSuccess(*w1);
        checkSuccess(*w2);
        checkSuccess(*w3);
    }

    SECTION("new work wakes up scheduler")
    {
                        while (wm.getState() != TestBasicWork::State::WORK_WAITING)
        {
            clock.crank();
        }

        auto w = wm.scheduleWork<TestBasicWork>("test-work");
        REQUIRE(wm.getState() == TestBasicWork::State::WORK_RUNNING);
        while (!wm.allChildrenSuccessful())
        {
            clock.crank();
        }
        checkSuccess(*w);
    }

    SECTION("work retries and fails")
    {
        auto w = wm.scheduleWork<TestBasicWork>("test-work", true);
        while (!wm.allChildrenDone())
        {
            clock.crank();
        }
        REQUIRE(w->getState() == TestBasicWork::State::WORK_FAILURE);

                REQUIRE(w->mRunningCount == w->mNumSteps * (BasicWork::RETRY_ONCE + 1));
        REQUIRE(w->mSuccessCount == 0);
        REQUIRE(w->mFailureCount == 1);
        REQUIRE(w->mRetryCount == BasicWork::RETRY_ONCE);
    }
    SECTION("work shutdown")
    {
                auto w = wm.scheduleWork<TestBasicWork>("test-work", false, 100);
        while (!wm.allChildrenDone())
        {
            clock.crank();
            w->shutdown();
        }

        REQUIRE(w->getState() == TestBasicWork::State::WORK_ABORTED);
                REQUIRE(w->mAbortCount == 1);
        REQUIRE(w->mCount == w->mNumSteps);

                REQUIRE(wm.getState() == TestBasicWork::State::WORK_WAITING);
    }
}

class TestWork : public Work
{
  public:
    int mRunningCount{0};
    int mSuccessCount{0};
    int mFailureCount{0};
    int mRetryCount{0};

    TestWork(Application& app, std::string name)
        : Work(app, std::move(name), RETRY_NEVER)
    {
    }

    BasicWork::State
    doWork() override
    {
        ++mRunningCount;
        return WorkUtils::checkChildrenStatus(*this);
    }

    template <typename T, typename... Args>
    std::shared_ptr<T>
    addTestWork(Args&&... args)
    {
        return addWork<T>(std::forward<Args>(args)...);
    }

    void
    onSuccess() override
    {
        mSuccessCount++;
    }

    void
    onFailureRaise() override
    {
        mFailureCount++;
    }

    void
    onFailureRetry() override
    {
        mRetryCount++;
    }
};

TEST_CASE("work with children", "[work]")
{
    VirtualClock clock;
    Config const& cfg = getTestConfig();
    Application::pointer appPtr = createTestApplication(clock, cfg);
    auto& wm = appPtr->getWorkScheduler();

    auto w1 = wm.scheduleWork<TestWork>("test-work1");
    auto w2 = wm.scheduleWork<TestWork>("test-work2");
    auto l1 = w1->addTestWork<TestBasicWork>("leaf-work");
    auto l2 = w1->addTestWork<TestBasicWork>("leaf-work-2");

    SECTION("success")
    {
        while (!wm.allChildrenSuccessful())
        {
            clock.crank();
        }
        REQUIRE(l1->getState() == TestBasicWork::State::WORK_SUCCESS);
        REQUIRE(l2->getState() == TestBasicWork::State::WORK_SUCCESS);
        REQUIRE(w1->getState() == TestBasicWork::State::WORK_SUCCESS);
        REQUIRE(w2->getState() == TestBasicWork::State::WORK_SUCCESS);
        REQUIRE(wm.getState() == TestBasicWork::State::WORK_WAITING);
    }
    SECTION("child failed")
    {
        auto l3 = w2->addTestWork<TestBasicWork>("leaf-work3", true);
        while (!wm.allChildrenDone())
        {
            clock.crank();
        }
        REQUIRE(l1->getState() == TestBasicWork::State::WORK_SUCCESS);
        REQUIRE(l2->getState() == TestBasicWork::State::WORK_SUCCESS);
        REQUIRE(w1->getState() == TestBasicWork::State::WORK_SUCCESS);
        REQUIRE(w2->getState() == TestBasicWork::State::WORK_FAILURE);
        REQUIRE(l3->getState() == TestBasicWork::State::WORK_FAILURE);
        REQUIRE(wm.getState() == TestBasicWork::State::WORK_WAITING);
    }
    SECTION("child failed so parent aborts other child")
    {
        auto l3 = w2->addTestWork<TestBasicWork>("leaf-work3", true, 3,
                                                 TestBasicWork::RETRY_NEVER);
        auto l4 = w2->addTestWork<TestBasicWork>("leaf-work4", false, 100);
        while (!wm.allChildrenDone())
        {
            clock.crank();
        }
        REQUIRE(l1->getState() == TestBasicWork::State::WORK_SUCCESS);
        REQUIRE(l2->getState() == TestBasicWork::State::WORK_SUCCESS);
        REQUIRE(w1->getState() == TestBasicWork::State::WORK_SUCCESS);
        REQUIRE(w2->getState() == TestBasicWork::State::WORK_FAILURE);
        REQUIRE(l3->getState() == TestBasicWork::State::WORK_FAILURE);
        REQUIRE(l4->getState() == TestBasicWork::State::WORK_ABORTED);
        REQUIRE(wm.getState() == TestBasicWork::State::WORK_WAITING);
    }
    SECTION("work scheduler shutdown")
    {
        wm.shutdown();

        REQUIRE(wm.isAborting());
        REQUIRE(w1->isAborting());
        REQUIRE(w2->isAborting());
        REQUIRE(l1->isAborting());
        REQUIRE(l2->isAborting());

                        REQUIRE_NOTHROW(l1->forceWakeUp());
        REQUIRE_NOTHROW(l2->forceWakeUp());
        REQUIRE(l1->isAborting());
        REQUIRE(l2->isAborting());
        REQUIRE_THROWS_AS(w1->addTestWork<TestBasicWork>("leaf-work-3"),
                          std::runtime_error);

        while (!wm.allChildrenDone())
        {
            clock.crank();
        }

        REQUIRE(wm.getState() == TestBasicWork::State::WORK_ABORTED);
        REQUIRE(w1->getState() == TestBasicWork::State::WORK_ABORTED);
        REQUIRE(w2->getState() == TestBasicWork::State::WORK_ABORTED);
        REQUIRE(l1->getState() == TestBasicWork::State::WORK_ABORTED);
        REQUIRE(l2->getState() == TestBasicWork::State::WORK_ABORTED);
    }
}

TEST_CASE("work scheduling and run count", "[work]")
{
    VirtualClock clock;
    Config const& cfg = getTestConfig();
    Application::pointer appPtr = createTestApplication(clock, cfg);

        auto& wm = appPtr->getWorkScheduler();
    auto mainWork = wm.scheduleWork<TestWork>("main-work");

    SECTION("multi-level tree")
    {
                auto w1 = mainWork->addTestWork<TestBasicWork>("3-step-work");
        auto w2 = mainWork->addTestWork<TestWork>("wait-for-children-work");

                auto c1 = w2->addTestWork<TestBasicWork>("child-3-step-work");
        auto c2 = w2->addTestWork<TestBasicWork>("other-child-3-step-work");

                                                        while (!wm.allChildrenSuccessful())
        {
            clock.crank();
        }
        REQUIRE(w1->mRunningCount == w1->mNumSteps);
        REQUIRE(w2->mRunningCount == c1->mNumSteps);
        REQUIRE(c1->mRunningCount == c1->mNumSteps);
        REQUIRE(c2->mRunningCount == c2->mNumSteps);

                REQUIRE(mainWork->mRunningCount ==
                (c1->mRunningCount * 2 + w2->mRunningCount));
    }
    SECTION("multi-level tree more leaves do not affect scheduling")
    {
                                                        auto w1 = mainWork->addTestWork<TestWork>("test-work-1");
        auto w2 = mainWork->addTestWork<TestWork>("test-work-2");

        auto c1 = w1->addTestWork<TestBasicWork>("child-3-step-work");
        auto c2 = w1->addTestWork<TestBasicWork>("other-child-3-step-work");
        auto c3 = w2->addTestWork<TestBasicWork>("2-child-3-step-work");
        auto c4 = w2->addTestWork<TestBasicWork>("2-other-child-3-step-work");

        while (!wm.allChildrenSuccessful())
        {
            clock.crank();
        }

        REQUIRE(w2->mRunningCount == c1->mNumSteps);
        REQUIRE(c1->mRunningCount == c1->mNumSteps);
        REQUIRE(c2->mRunningCount == c2->mNumSteps);
        REQUIRE(mainWork->mRunningCount ==
                (c2->mRunningCount * 2 + w2->mRunningCount));
    }
}

TEST_CASE("work scheduling compare trees", "[work]")
{
        
    VirtualClock clock;
    Config const& cfg = getTestConfig();
    Application::pointer appPtr = createTestApplication(clock, cfg);

    auto& wm = appPtr->getWorkScheduler();

    SECTION("linked list structure")
    {
                                                                        auto w1 = wm.scheduleWork<TestWork>("work-1");
        auto w2 = w1->addTestWork<TestWork>("work-2");
        auto w3 = w2->addTestWork<TestBasicWork>("work-3");

        while (!wm.allChildrenSuccessful())
        {
            clock.crank();
        }
        REQUIRE(w3->mRunningCount == w3->mNumSteps);
        REQUIRE(w2->mRunningCount == w3->mRunningCount);
        REQUIRE(w1->mRunningCount == w2->mRunningCount + w3->mRunningCount);
    }

    SECTION("flat work sequence")
    {
                                                        
        auto w1 = std::make_shared<TestWork>(*appPtr, "work-1");
        auto w2 = std::make_shared<TestWork>(*appPtr, "work-2");
        auto w3 = std::make_shared<TestBasicWork>(*appPtr, "work-3");

                std::vector<std::shared_ptr<BasicWork>> seq{w3, w2, w1};

        auto sw = wm.scheduleWork<WorkSequence>("test-work-sequence", seq);
        while (!wm.allChildrenSuccessful())
        {
            clock.crank();
        }

        REQUIRE(w3->mRunningCount == w3->mNumSteps);
        REQUIRE(w2->mRunningCount == 1);
        REQUIRE(w1->mRunningCount == 1);
    }
}

class TestRunCommandWork : public RunCommandWork
{
    std::string mCommand;

  public:
    TestRunCommandWork(Application& app, std::string name, std::string command)
        : RunCommandWork(app, std::move(name)), mCommand(std::move(command))
    {
    }
    ~TestRunCommandWork() override = default;

    CommandInfo
    getCommand() override
    {
        return CommandInfo{mCommand, std::string()};
    }

    BasicWork::State
    onRun() override
    {
        return RunCommandWork::onRun();
    }
};

TEST_CASE("RunCommandWork test", "[work]")
{
    VirtualClock clock;
    Config const& cfg = getTestConfig();
    Application::pointer appPtr = createTestApplication(clock, cfg);
    auto& wm = appPtr->getWorkScheduler();

    SECTION("one run command work")
    {
        wm.scheduleWork<TestRunCommandWork>("test-run-command", "date");
        while (!wm.allChildrenSuccessful())
        {
            clock.crank();
        }
    }
    SECTION("round robin with other work")
    {
        wm.scheduleWork<TestRunCommandWork>("test-run-command", "date");
        wm.scheduleWork<TestBasicWork>("test-work");
        while (!wm.allChildrenSuccessful())
        {
            clock.crank();
        }
    }
    SECTION("invalid run command")
    {
        auto w = wm.scheduleWork<TestRunCommandWork>("test-run-command",
                                                     "_invalid_");
        while (!wm.allChildrenDone())
        {
            clock.crank();
        }
        REQUIRE(w->getState() == TestBasicWork::State::WORK_FAILURE);
    }

    SECTION("run command aborted")
    {
#ifdef _WIN32
        std::string command = "waitfor /T 10 pause";
#else
        std::string command = "sleep 10";
#endif
        auto w =
            wm.scheduleWork<TestRunCommandWork>("test-run-command", command);
        while (w->getState() != TestBasicWork::State::WORK_WAITING)
        {
            clock.crank();
        }

        REQUIRE(appPtr->getProcessManager().getNumRunningProcesses());
        wm.shutdown();

        while (wm.getState() != TestBasicWork::State::WORK_ABORTED)
        {
            clock.crank();
        }

        REQUIRE(w->getState() == TestBasicWork::State::WORK_ABORTED);
        REQUIRE(appPtr->getProcessManager().getNumRunningProcesses() == 0);
    }
}

TEST_CASE("WorkSequence test", "[work]")
{
    VirtualClock clock;
    Config const& cfg = getTestConfig();
    Application::pointer appPtr = createTestApplication(clock, cfg);
    auto& wm = appPtr->getWorkScheduler();

    SECTION("basic")
    {
        auto w1 = std::make_shared<TestBasicWork>(*appPtr, "test-work-1");
        auto w2 = std::make_shared<TestBasicWork>(*appPtr, "test-work-2");
        auto w3 = std::make_shared<TestBasicWork>(*appPtr, "test-work-3");
        std::vector<std::shared_ptr<BasicWork>> seq{w1, w2, w3};

        auto work = wm.scheduleWork<WorkSequence>("test-work-sequence", seq);
        while (!wm.allChildrenSuccessful())
        {
            if (!w1->mSuccessCount)
            {
                REQUIRE(!w2->mRunningCount);
                REQUIRE(!w3->mRunningCount);
            }
            else if (!w2->mSuccessCount)
            {
                REQUIRE(w1->mSuccessCount);
                REQUIRE(!w3->mRunningCount);
            }
            else
            {
                REQUIRE(w1->mSuccessCount);
                REQUIRE(w2->mSuccessCount);
            }
            clock.crank();
        }
    }
    SECTION("WorkSequence is empty")
    {
        std::vector<std::shared_ptr<BasicWork>> seq{};
        auto work2 = wm.scheduleWork<WorkSequence>("test-work-sequence-2", seq);
        while (!wm.allChildrenSuccessful())
        {
            clock.crank();
        }
        REQUIRE(work2->getState() == TestBasicWork::State::WORK_SUCCESS);
    }
    SECTION("first work fails")
    {
        auto w1 = std::make_shared<TestBasicWork>(*appPtr, "test-work-1", true);
        auto w2 = std::make_shared<TestBasicWork>(*appPtr, "test-work-2");
        auto w3 = std::make_shared<TestBasicWork>(*appPtr, "test-work-3");
        std::vector<std::shared_ptr<BasicWork>> seq{w1, w2, w3};

        auto work = wm.executeWork<WorkSequence>("test-work-sequence", seq);

                auto w1RunCount = w1->mNumSteps * (BasicWork::RETRY_ONCE + 1);
        REQUIRE(w1->mRunningCount == w1RunCount * (BasicWork::RETRY_A_FEW + 1));
        REQUIRE(w1->getState() == TestBasicWork::State::WORK_FAILURE);

                REQUIRE_FALSE(w2->mRunningCount);
        REQUIRE_FALSE(w2->mSuccessCount);
        REQUIRE_FALSE(w2->mFailureCount);
        REQUIRE_FALSE(w2->mRetryCount);
        REQUIRE_FALSE(w3->mRunningCount);
        REQUIRE_FALSE(w3->mSuccessCount);
        REQUIRE_FALSE(w3->mFailureCount);
        REQUIRE_FALSE(w3->mRetryCount);
        REQUIRE(work->getState() == TestBasicWork::State::WORK_FAILURE);
    }
    SECTION("fails midway")
    {
        auto w1 = std::make_shared<TestBasicWork>(*appPtr, "test-work-1");
        auto w2 = std::make_shared<TestBasicWork>(*appPtr, "test-work-2", true);
        auto w3 = std::make_shared<TestBasicWork>(*appPtr, "test-work-3");
        std::vector<std::shared_ptr<BasicWork>> seq{w1, w2, w3};

        auto work = wm.executeWork<WorkSequence>("test-work-sequence", seq);

                auto w2RunCount = w2->mNumSteps * (BasicWork::RETRY_ONCE + 1);
        REQUIRE(w2->mRunningCount == w2RunCount * (BasicWork::RETRY_A_FEW + 1));
        REQUIRE(w2->getState() == BasicWork::State::WORK_FAILURE);

        REQUIRE(w1->getState() == TestBasicWork::State::WORK_SUCCESS);
        REQUIRE(w1->mRunningCount);
        REQUIRE(w1->mSuccessCount);
        REQUIRE_FALSE(w1->mFailureCount);
        REQUIRE_FALSE(w1->mRetryCount);
        REQUIRE_FALSE(w3->mRunningCount);
        REQUIRE_FALSE(w3->mSuccessCount);
        REQUIRE_FALSE(w3->mFailureCount);
        REQUIRE_FALSE(w3->mRetryCount);
        REQUIRE(work->getState() == TestBasicWork::State::WORK_FAILURE);
    }
    SECTION("work scheduler shutdown")
    {
                auto w1 =
            std::make_shared<TestBasicWork>(*appPtr, "test-work-1", false, 100);
        auto w2 =
            std::make_shared<TestBasicWork>(*appPtr, "test-work-2", false, 100);
        auto w3 =
            std::make_shared<TestBasicWork>(*appPtr, "test-work-3", false, 100);
        std::vector<std::shared_ptr<BasicWork>> seq{w1, w2, w3};

        auto work = wm.scheduleWork<WorkSequence>("test-work-sequence", seq);
        clock.crank();
        CHECK(!wm.allChildrenDone());
        wm.shutdown();

        while (!wm.allChildrenDone())
        {
            clock.crank();
        }

        REQUIRE(work->getState() == TestBasicWork::State::WORK_ABORTED);
        REQUIRE(wm.getState() == TestBasicWork::State::WORK_ABORTED);
    }
}

class TestBatchWork : public BatchWork
{
    bool mShouldFail;
    int mTotalWorks;

  public:
    int mCount{0};
    std::vector<std::shared_ptr<TestBasicWork>> mBatchedWorks;
    TestBatchWork(Application& app, std::string const& name, bool fail = false)
        : BatchWork(app, name)
        , mShouldFail(fail)
        , mTotalWorks(app.getConfig().MAX_CONCURRENT_SUBPROCESSES * 2)
    {
    }

  protected:
    bool
    hasNext() const override
    {
        return mCount < mTotalWorks;
    }

    void
    resetIter() override
    {
        mCount = 0;
    }

    std::shared_ptr<BasicWork>
    yieldMoreWork() override
    {
                bool fail = mCount == mTotalWorks - 1 && mShouldFail;
        auto w = std::make_shared<TestBasicWork>(
            mApp, fmt::format("child-{:d}", mCount++), fail);
        if (!fail)
        {
            mBatchedWorks.push_back(w);
        }
        return w;
    }
};

TEST_CASE("Work batching", "[batching][work]")
{
    VirtualClock clock;
    Application::pointer app = createTestApplication(clock, getTestConfig());
    auto& wm = app->getWorkScheduler();

    SECTION("success")
    {
        auto testBatch = wm.scheduleWork<TestBatchWork>("test-batch");
        while (!clock.getIOContext().stopped() && !wm.allChildrenDone())
        {
            clock.crank(true);
            REQUIRE(testBatch->getNumWorksInBatch() <=
                    app->getConfig().MAX_CONCURRENT_SUBPROCESSES);
        }
        REQUIRE(testBatch->getState() == TestBasicWork::State::WORK_SUCCESS);
    }
    SECTION("shutdown")
    {
        auto testBatch = wm.scheduleWork<TestBatchWork>("test-batch", true);
        while (!clock.getIOContext().stopped() && !wm.allChildrenDone())
        {
            clock.crank(true);
            wm.shutdown();
        }
        REQUIRE(testBatch->getState() == TestBasicWork::State::WORK_ABORTED);

                for (auto const& w : testBatch->mBatchedWorks)
        {
            auto validState =
                w->getState() == TestBasicWork::State::WORK_SUCCESS ||
                w->getState() == TestBasicWork::State::WORK_ABORTED;
            REQUIRE(validState);
        }
    }
}

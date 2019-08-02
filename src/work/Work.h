#pragma once


#include "util/Timer.h"
#include "work/BasicWork.h"
#include <list>
#include <map>
#include <memory>
#include <string>

namespace viichain
{

class Application;

class Work : public BasicWork
{
  public:
    virtual ~Work();

    std::string getStatus() const override;

    bool allChildrenSuccessful() const;
    bool allChildrenDone() const;
    bool anyChildRaiseFailure() const;
    bool anyChildRunning() const;
    bool hasChildren() const;

    void shutdown() override;

  protected:
    Work(Application& app, std::string name,
         size_t retries = BasicWork::RETRY_A_FEW);

                    template <typename T, typename... Args>
    std::shared_ptr<T>
    addWork(Args&&... args)
    {
        return addWorkWithCallback<T>(nullptr, std::forward<Args>(args)...);
    }

                    template <typename T, typename... Args>
    std::shared_ptr<T>
    addWorkWithCallback(std::function<void()> cb, Args&&... args)
    {
        if (isAborting())
        {
            throw std::runtime_error(getName() + " is being aborted!");
        }

        auto child = std::make_shared<T>(mApp, std::forward<Args>(args)...);
        addWork(cb, child);
        return child;
    }

    void
    addWork(std::function<void()> cb, std::shared_ptr<BasicWork> child)
    {
        addChild(child);
        auto wakeSelf = wakeSelfUpCallback(cb);
        child->startWork(wakeSelf);
        wakeSelf();
    }

    State onRun() final;
    bool onAbort() final;
        void onReset() final;
    void onFailureRaise() override;
    void onFailureRetry() override;

                                virtual BasicWork::State doWork() = 0;

        virtual void doReset();

  private:
    std::list<std::shared_ptr<BasicWork>> mChildren;
    std::list<std::shared_ptr<BasicWork>>::const_iterator mNextChild;

    size_t mDoneChildren{0};
    size_t mTotalChildren{0};

    std::shared_ptr<BasicWork> yieldNextRunningChild();
    void addChild(std::shared_ptr<BasicWork> child);
    void clearChildren();
    void shutdownChildren();

    bool mAbortChildrenButNotSelf{false};
};

namespace WorkUtils
{
BasicWork::State checkChildrenStatus(Work const& w);
}
}

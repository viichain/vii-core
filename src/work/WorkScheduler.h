#pragma once

#include "main/Application.h"
#include "work/Work.h"

namespace viichain
{

class WorkScheduler : public Work
{
    explicit WorkScheduler(Application& app);
    bool mScheduled{false};

  public:
    virtual ~WorkScheduler();
    static std::shared_ptr<WorkScheduler> create(Application& app);

    template <typename T, typename... Args>
    std::shared_ptr<T>
    executeWork(Args&&... args)
    {
        auto work = scheduleWork<T>(std::forward<Args>(args)...);
        auto& clock = mApp.getClock();
        while (!clock.getIOContext().stopped() && !allChildrenDone())
        {
            clock.crank(true);
        }
        return work;
    }

    template <typename T, typename... Args>
    std::shared_ptr<T>
    scheduleWork(Args&&... args)
    {
        std::weak_ptr<WorkScheduler> weak(
            std::static_pointer_cast<WorkScheduler>(shared_from_this()));
                        auto innerCallback = [weak]() { scheduleOne(weak); };
        return addWorkWithCallback<T>(innerCallback,
                                      std::forward<Args>(args)...);
    }

    void shutdown() override;

  protected:
    static void scheduleOne(std::weak_ptr<WorkScheduler> weak);
    State doWork() override;
};
}

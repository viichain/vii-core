#pragma once

#include "work/Work.h"

namespace viichain
{
class BatchWork : public Work
{
        std::map<std::string, std::shared_ptr<BasicWork>> mBatch;
    void addMoreWorkIfNeeded();

  public:
    BatchWork(Application& app, std::string name);
    ~BatchWork() = default;

    size_t
    getNumWorksInBatch() const
    {
        return mBatch.size();
    }

  protected:
    void doReset() final;
    State doWork() final;

            virtual bool hasNext() const = 0;
    virtual std::shared_ptr<BasicWork> yieldMoreWork() = 0;
    virtual void resetIter() = 0;
};
}

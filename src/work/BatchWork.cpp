
#include "work/BatchWork.h"
#include "catchup/CatchupManager.h"
#include "util/Logging.h"
#include "util/format.h"

namespace viichain
{

BatchWork::BatchWork(Application& app, std::string name)
    : Work(app, name, RETRY_NEVER)
{
}

void
BatchWork::doReset()
{
    mBatch.clear();
    resetIter();
}

BasicWork::State
BatchWork::doWork()
{
    if (anyChildRaiseFailure())
    {
        return State::WORK_FAILURE;
    }

        for (auto childIt = mBatch.begin(); childIt != mBatch.end();)
    {
        if (childIt->second->getState() == State::WORK_SUCCESS)
        {
            CLOG(DEBUG, "Work") << "Finished child work " << childIt->first;
            childIt = mBatch.erase(childIt);
        }
        else
        {
            ++childIt;
        }
    }

    addMoreWorkIfNeeded();
    mApp.getCatchupManager().logAndUpdateCatchupStatus(true);

    if (allChildrenSuccessful())
    {
        return State::WORK_SUCCESS;
    }

    if (!anyChildRunning())
    {
        return State::WORK_WAITING;
    }

    return State::WORK_RUNNING;
}

void
BatchWork::addMoreWorkIfNeeded()
{
    if (isAborting())
    {
        throw std::runtime_error(getName() + " is being aborted!");
    }

    int nChildren = mApp.getConfig().MAX_CONCURRENT_SUBPROCESSES;
    while (mBatch.size() < nChildren && hasNext())
    {
        auto w = yieldMoreWork();
        addWork(nullptr, w);
        if (mBatch.find(w->getName()) != mBatch.end())
        {
            throw std::runtime_error(
                "BatchWork error: inserting duplicate work!");
        }
        mBatch.insert(std::make_pair(w->getName(), w));
    }
}
}

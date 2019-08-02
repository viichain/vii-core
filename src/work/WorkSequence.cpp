
#include "WorkSequence.h"

namespace viichain
{

WorkSequence::WorkSequence(Application& app, std::string name,
                           std::vector<std::shared_ptr<BasicWork>> sequence,
                           size_t maxRetries)
    : BasicWork(app, std::move(name), maxRetries)
    , mSequenceOfWork(std::move(sequence))
    , mNextInSequence(mSequenceOfWork.begin())
{
}

BasicWork::State
WorkSequence::onRun()
{
    if (mNextInSequence == mSequenceOfWork.end())
    {
                return State::WORK_SUCCESS;
    }

    auto w = *mNextInSequence;
    assert(w);
    if (!mCurrentExecuting)
    {
        w->startWork(wakeSelfUpCallback());
        mCurrentExecuting = w;
    }
    else
    {
        auto state = w->getState();
        if (state == State::WORK_SUCCESS)
        {
            mCurrentExecuting = nullptr;
            ++mNextInSequence;
            return this->onRun();
        }
        else if (state != State::WORK_RUNNING)
        {
            return state;
        }
    }
    w->crankWork();
    return State::WORK_RUNNING;
}

bool
WorkSequence::onAbort()
{
    if (mCurrentExecuting && !mCurrentExecuting->isDone())
    {
                mCurrentExecuting->crankWork();
        return false;
    }
    return true;
}

void
WorkSequence::onReset()
{
    assert(std::all_of(
        mSequenceOfWork.cbegin(), mNextInSequence,
        [](std::shared_ptr<BasicWork> const& w) { return w->isDone(); }));
    mNextInSequence = mSequenceOfWork.begin();
    mCurrentExecuting.reset();
}

std::string
WorkSequence::getStatus() const
{
    if (!isDone() && mNextInSequence != mSequenceOfWork.end())
    {
        return (*mNextInSequence)->getStatus();
    }
    return BasicWork::getStatus();
}

void
WorkSequence::shutdown()
{
    if (mCurrentExecuting)
    {
        mCurrentExecuting->shutdown();
    }

    BasicWork::shutdown();
}
}

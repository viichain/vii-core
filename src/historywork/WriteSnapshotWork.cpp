
#include "historywork/WriteSnapshotWork.h"
#include "database/Database.h"
#include "history/StateSnapshot.h"
#include "historywork/Progress.h"
#include "main/Application.h"
#include "util/XDRStream.h"

namespace viichain
{

WriteSnapshotWork::WriteSnapshotWork(Application& app,
                                     std::shared_ptr<StateSnapshot> snapshot)
    : BasicWork(app, "write-snapshot", Work::RETRY_A_LOT), mSnapshot(snapshot)
{
}

BasicWork::State
WriteSnapshotWork::onRun()
{
    if (mDone)
    {
        return mSuccess ? State::WORK_SUCCESS : State::WORK_FAILURE;
    }

    std::weak_ptr<WriteSnapshotWork> weak(
        std::static_pointer_cast<WriteSnapshotWork>(shared_from_this()));

    auto work = [weak]() {
        auto self = weak.lock();
        if (!self)
        {
            return;
        }

        auto snap = self->mSnapshot;
        bool success = true;
        if (!snap->writeHistoryBlocks())
        {
            success = false;
        }

                                self->mApp.postOnMainThread(
            [weak, success]() {
                auto self = weak.lock();
                if (self)
                {
                    self->mDone = true;
                    self->mSuccess = success;
                    self->wakeUp();
                }
            },
            "WriteSnapshotWork: finish");
    };

            if (mApp.getDatabase().canUsePool())
    {
        mApp.postOnBackgroundThread(work, "WriteSnapshotWork: start");
        return State::WORK_WAITING;
    }
    else
    {
        work();
        return State::WORK_RUNNING;
    }
}
}


#include "historywork/PutSnapshotFilesWork.h"
#include "bucket/BucketManager.h"
#include "history/HistoryArchiveManager.h"
#include "history/StateSnapshot.h"
#include "historywork/GetHistoryArchiveStateWork.h"
#include "historywork/GzipAndPutFilesWork.h"
#include "historywork/PutHistoryArchiveStateWork.h"
#include "main/Application.h"
#include <util/format.h>

namespace viichain
{

PutSnapshotFilesWork::PutSnapshotFilesWork(
    Application& app, std::shared_ptr<StateSnapshot> snapshot)
    : Work(app, fmt::format("update-archives-{:08x}",
                            snapshot->mLocalState.currentLedger))
    , mSnapshot(snapshot)
{
}

BasicWork::State
PutSnapshotFilesWork::doWork()
{
    if (!mStarted)
    {
        for (auto& writableArchive :
             mApp.getHistoryArchiveManager().getWritableHistoryArchives())
        {
                        auto getState = std::make_shared<GetHistoryArchiveStateWork>(
                mApp, mRemoteState, 0, writableArchive);

                        auto putFiles = std::make_shared<GzipAndPutFilesWork>(
                mApp, writableArchive, mSnapshot, mRemoteState);

                        auto putState = std::make_shared<PutHistoryArchiveStateWork>(
                mApp, mSnapshot->mLocalState, writableArchive);

            std::vector<std::shared_ptr<BasicWork>> seq{getState, putFiles,
                                                        putState};

            addWork<WorkSequence>("put-snapshot-sequence", seq);
        }
        mStarted = true;
    }
    else
    {
        if (allChildrenDone())
        {
                                    return anyChildRaiseFailure() ? State::WORK_FAILURE
                                          : State::WORK_SUCCESS;
        }

        if (!anyChildRunning())
        {
            return State::WORK_WAITING;
        }
    }
    return State::WORK_RUNNING;
}

void
PutSnapshotFilesWork::doReset()
{
    mStarted = false;
}
}


#include "historywork/BatchDownloadWork.h"
#include "catchup/CatchupManager.h"
#include "history/HistoryManager.h"
#include "historywork/GetAndUnzipRemoteFileWork.h"
#include "historywork/Progress.h"
#include "lib/util/format.h"
#include "main/Application.h"

namespace viichain
{
BatchDownloadWork::BatchDownloadWork(Application& app, CheckpointRange range,
                                     std::string const& type,
                                     TmpDir const& downloadDir)
    : BatchWork(app, fmt::format("batch-download-{:s}-{:08x}-{:08x}", type,
                                 range.mFirst, range.mLast))
    , mRange(range)
    , mNext(range.mFirst)
    , mFileType(type)
    , mDownloadDir(downloadDir)
{
}

std::string
BatchDownloadWork::getStatus() const
{
    if (getState() == State::WORK_RUNNING)
    {
        auto task = fmt::format("downloading {:s} files", mFileType);
        return fmtProgress(mApp, task, mRange.mFirst, mRange.mLast, mNext);
    }
    return BatchWork::getStatus();
}

std::shared_ptr<BasicWork>
BatchDownloadWork::yieldMoreWork()
{
    if (!hasNext())
    {
        CLOG(WARNING, "Work")
            << getName() << " has no more children to iterate over! ";
        return nullptr;
    }

    FileTransferInfo ft(mDownloadDir, mFileType, mNext);
    CLOG(DEBUG, "History") << "Downloading and unzipping " << mFileType
                           << " for checkpoint " << mNext;
    auto getAndUnzip = std::make_shared<GetAndUnzipRemoteFileWork>(mApp, ft);
    mApp.getCatchupManager().logAndUpdateCatchupStatus(true);
    mNext += mApp.getHistoryManager().getCheckpointFrequency();

    return getAndUnzip;
}

bool
BatchDownloadWork::hasNext() const
{
    return mNext <= mRange.mLast;
}

void
BatchDownloadWork::resetIter()
{
    mNext = mRange.mFirst;
}
}

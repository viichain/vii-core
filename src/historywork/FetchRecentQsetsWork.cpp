
#include "historywork/FetchRecentQsetsWork.h"
#include "history/FileTransferInfo.h"
#include "history/HistoryManager.h"
#include "historywork/BatchDownloadWork.h"
#include "historywork/GetHistoryArchiveStateWork.h"
#include "main/Application.h"
#include "main/ErrorMessages.h"
#include "util/FileSystemException.h"
#include "util/TmpDir.h"
#include "util/XDRStream.h"

namespace viichain
{

FetchRecentQsetsWork::FetchRecentQsetsWork(Application& app,
                                           InferredQuorum& inferredQuorum,
                                           uint32_t ledgerNum)
    : Work(app, "fetch-recent-qsets")
    , mInferredQuorum(inferredQuorum)
    , mLedgerNum(ledgerNum)
{
}

void
FetchRecentQsetsWork::doReset()
{
    mGetHistoryArchiveStateWork.reset();
    mDownloadSCPMessagesWork.reset();
    mDownloadDir =
        std::make_unique<TmpDir>(mApp.getTmpDirManager().tmpDir(getName()));
    mRemoteState = {};
}

BasicWork::State
FetchRecentQsetsWork::doWork()
{
        if (!mGetHistoryArchiveStateWork)
    {
        mGetHistoryArchiveStateWork =
            addWork<GetHistoryArchiveStateWork>(mRemoteState, 0);
        return State::WORK_RUNNING;
    }
    else if (mGetHistoryArchiveStateWork->getState() != State::WORK_SUCCESS)
    {
        return mGetHistoryArchiveStateWork->getState();
    }

                    uint32_t numCheckpoints = 100;
    uint32_t step = mApp.getHistoryManager().getCheckpointFrequency();
    uint32_t window = numCheckpoints * step;
    uint32_t lastSeq = mLedgerNum;
    uint32_t firstSeq = lastSeq < window ? (step - 1) : (lastSeq - window);

    if (!mDownloadSCPMessagesWork)
    {
        CLOG(INFO, "History") << "Downloading historical SCP messages: ["
                              << firstSeq << ", " << lastSeq << "]";
        auto range = CheckpointRange{firstSeq, lastSeq, step};
        mDownloadSCPMessagesWork = addWork<BatchDownloadWork>(
            range, HISTORY_FILE_TYPE_SCP, *mDownloadDir);
        return State::WORK_RUNNING;
    }
    else if (mDownloadSCPMessagesWork->getState() != State::WORK_SUCCESS)
    {
        return mDownloadSCPMessagesWork->getState();
    }

        for (uint32_t i = firstSeq; i <= lastSeq; i += step)
    {
        CLOG(INFO, "History") << "Scanning for QSets in checkpoint: " << i;
        XDRInputFileStream in;
        FileTransferInfo fi(*mDownloadDir, HISTORY_FILE_TYPE_SCP, i);
        try
        {
            in.open(fi.localPath_nogz());
        }
        catch (FileSystemException&)
        {
            CLOG(ERROR, "History") << POSSIBLY_CORRUPTED_LOCAL_FS;
            return State::WORK_FAILURE;
        }

        SCPHistoryEntry tmp;
        while (in && in.readOne(tmp))
        {
            if (tmp.v0().ledgerMessages.ledgerSeq > mLedgerNum)
            {
                break;
            }
            mInferredQuorum.noteSCPHistory(tmp);
        }
    }

    return State::WORK_SUCCESS;
}
}

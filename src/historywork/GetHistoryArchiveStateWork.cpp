
#include "historywork/GetHistoryArchiveStateWork.h"
#include "history/HistoryArchive.h"
#include "historywork/GetRemoteFileWork.h"
#include "ledger/LedgerManager.h"
#include "lib/util/format.h"
#include "main/Application.h"
#include "main/ErrorMessages.h"
#include "util/Logging.h"
#include <medida/meter.h>
#include <medida/metrics_registry.h>

namespace viichain
{
GetHistoryArchiveStateWork::GetHistoryArchiveStateWork(
    Application& app, HistoryArchiveState& state, uint32_t seq,
    std::shared_ptr<HistoryArchive> archive, size_t maxRetries)
    : Work(app, "get-archive-state", maxRetries)
    , mState(state)
    , mSeq(seq)
    , mArchive(archive)
    , mRetries(maxRetries)
    , mLocalFilename(
          archive ? HistoryArchiveState::localName(app, archive->getName())
                  : app.getHistoryManager().localFilename(
                        HistoryArchiveState::baseName()))
    , mGetHistoryArchiveStateSuccess(app.getMetrics().NewMeter(
          {"history", "download-history-archive-state", "success"}, "event"))
{
}

BasicWork::State
GetHistoryArchiveStateWork::doWork()
{
    if (mGetRemoteFile)
    {
        auto state = mGetRemoteFile->getState();
        if (state == State::WORK_SUCCESS)
        {
            try
            {
                mState.load(mLocalFilename);
            }
            catch (std::runtime_error& e)
            {
                CLOG(ERROR, "History")
                    << "Error loading history state: " << e.what();
                CLOG(ERROR, "History") << POSSIBLY_CORRUPTED_LOCAL_FS;
                CLOG(ERROR, "History") << "OR";
                CLOG(ERROR, "History") << POSSIBLY_CORRUPTED_HISTORY;
                CLOG(ERROR, "History") << "OR";
                CLOG(ERROR, "History") << UPGRADE_STELLAR_CORE;
                return State::WORK_FAILURE;
            }
        }
        return state;
    }

    else
    {
        auto name = mSeq == 0 ? HistoryArchiveState::wellKnownRemoteName()
                              : HistoryArchiveState::remoteName(mSeq);
        CLOG(INFO, "History") << "Downloading history archive state: " << name;
        mGetRemoteFile = addWork<GetRemoteFileWork>(name, mLocalFilename,
                                                    mArchive, mRetries);
        return State::WORK_RUNNING;
    }
}

void
GetHistoryArchiveStateWork::doReset()
{
    mGetRemoteFile.reset();
    std::remove(mLocalFilename.c_str());
}

void
GetHistoryArchiveStateWork::onSuccess()
{
    mGetHistoryArchiveStateSuccess.Mark();
    Work::onSuccess();
}
}

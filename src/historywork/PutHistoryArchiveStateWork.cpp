
#include "historywork/PutHistoryArchiveStateWork.h"
#include "history/HistoryArchive.h"
#include "historywork/MakeRemoteDirWork.h"
#include "historywork/PutRemoteFileWork.h"
#include "main/ErrorMessages.h"
#include "util/Logging.h"

namespace viichain
{

PutHistoryArchiveStateWork::PutHistoryArchiveStateWork(
    Application& app, HistoryArchiveState const& state,
    std::shared_ptr<HistoryArchive> archive)
    : Work(app, "put-history-archive-state")
    , mState(state)
    , mArchive(archive)
    , mLocalFilename(HistoryArchiveState::localName(app, archive->getName()))
{
}

void
PutHistoryArchiveStateWork::doReset()
{
    mPutRemoteFileWork.reset();
    std::remove(mLocalFilename.c_str());
}

BasicWork::State
PutHistoryArchiveStateWork::doWork()
{
    if (!mPutRemoteFileWork)
    {
        try
        {
            mState.save(mLocalFilename);
            spawnPublishWork();
            return State::WORK_RUNNING;
        }
        catch (std::runtime_error& e)
        {
            CLOG(ERROR, "History")
                << "Error saving history state: " << e.what();
            CLOG(ERROR, "History") << POSSIBLY_CORRUPTED_LOCAL_FS;
            return State::WORK_FAILURE;
        }
    }
    else
    {
        return WorkUtils::checkChildrenStatus(*this);
    }
}

void
PutHistoryArchiveStateWork::spawnPublishWork()
{
        auto seqName = HistoryArchiveState::remoteName(mState.currentLedger);
    auto seqDir = HistoryArchiveState::remoteDir(mState.currentLedger);

    auto w1 = std::make_shared<MakeRemoteDirWork>(mApp, seqDir, mArchive);
    auto w2 = std::make_shared<PutRemoteFileWork>(mApp, mLocalFilename, seqName,
                                                  mArchive);

    std::vector<std::shared_ptr<BasicWork>> seq{w1, w2};
    mPutRemoteFileWork =
        addWork<WorkSequence>("put-history-file-sequence", seq);

        auto wkName = HistoryArchiveState::wellKnownRemoteName();
    auto wkDir = HistoryArchiveState::wellKnownRemoteDir();

    auto w3 = std::make_shared<MakeRemoteDirWork>(mApp, wkDir, mArchive);
    auto w4 = std::make_shared<PutRemoteFileWork>(mApp, mLocalFilename, wkName,
                                                  mArchive);
    std::vector<std::shared_ptr<BasicWork>> seqWk{w3, w4};
    auto wellKnownPut =
        addWork<WorkSequence>("put-history-well-known-sequence", seqWk);
}
}


#include "GzipAndPutFilesWork.h"
#include "bucket/BucketManager.h"
#include "historywork/GzipFileWork.h"
#include "historywork/MakeRemoteDirWork.h"
#include "historywork/PutRemoteFileWork.h"
#include "work/WorkSequence.h"

namespace viichain
{

GzipAndPutFilesWork::GzipAndPutFilesWork(
    Application& app, std::shared_ptr<HistoryArchive> archive,
    std::shared_ptr<StateSnapshot> snapshot,
    HistoryArchiveState const& remoteState)
    : Work(app, "helper-put-files-" + archive->getName())
    , mArchive(archive)
    , mSnapshot(snapshot)
    , mRemoteState(remoteState)
{
}

BasicWork::State
GzipAndPutFilesWork::doWork()
{
    if (!mChildrenSpawned)
    {
        std::vector<std::shared_ptr<FileTransferInfo>> files = {
            mSnapshot->mLedgerSnapFile, mSnapshot->mTransactionSnapFile,
            mSnapshot->mTransactionResultSnapFile,
            mSnapshot->mSCPHistorySnapFile};

        std::vector<std::string> bucketsToSend =
            mSnapshot->mLocalState.differingBuckets(mRemoteState);

        for (auto const& hash : bucketsToSend)
        {
            auto b = mApp.getBucketManager().getBucketByHash(hexToBin256(hash));
            assert(b);
            files.push_back(std::make_shared<FileTransferInfo>(*b));
        }
        for (auto f : files)
        {
                        if (f && fs::exists(f->localPath_nogz()))
            {
                auto gzipFile = std::make_shared<GzipFileWork>(
                    mApp, f->localPath_nogz(), true);
                auto mkdir = std::make_shared<MakeRemoteDirWork>(
                    mApp, f->remoteDir(), mArchive);
                auto putFile = std::make_shared<PutRemoteFileWork>(
                    mApp, f->localPath_gz(), f->remoteName(), mArchive);

                std::vector<std::shared_ptr<BasicWork>> seq{gzipFile, mkdir,
                                                            putFile};

                addWork<WorkSequence>("gzip-and-put-file-" + f->localPath_gz(),
                                      seq);
            }
        }
        mChildrenSpawned = true;
    }
    else
    {
        return WorkUtils::checkChildrenStatus(*this);
    }
    return State::WORK_RUNNING;
}

void
GzipAndPutFilesWork::doReset()
{
    mChildrenSpawned = false;
}
}

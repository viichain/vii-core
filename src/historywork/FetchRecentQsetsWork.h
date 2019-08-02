
#pragma once

#include "history/HistoryArchive.h"
#include "work/Work.h"

namespace viichain
{

class TmpDir;
struct InferredQuorum;

class FetchRecentQsetsWork : public Work
{
    std::unique_ptr<TmpDir> mDownloadDir;
    InferredQuorum& mInferredQuorum;
    uint32_t mLedgerNum;
    HistoryArchiveState mRemoteState;
    std::shared_ptr<BasicWork> mGetHistoryArchiveStateWork;
    std::shared_ptr<BasicWork> mDownloadSCPMessagesWork;

  public:
    FetchRecentQsetsWork(Application& app, InferredQuorum& iq,
                         uint32_t ledgerNum);
    ~FetchRecentQsetsWork() = default;
    void doReset() override;
    BasicWork::State doWork() override;
};
}

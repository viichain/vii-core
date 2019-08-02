
#pragma once

#include "history/HistoryArchive.h"
#include "work/Work.h"

namespace viichain
{

class Bucket;
class TmpDir;

struct HistoryArchiveState;

class RepairMissingBucketsWork : public Work
{
    using Handler = std::function<void(asio::error_code const& ec)>;
    Handler mEndHandler;
    bool mChildrenStarted{false};

    HistoryArchiveState mLocalState;
    std::unique_ptr<TmpDir> mDownloadDir;
    std::map<std::string, std::shared_ptr<Bucket>> mBuckets;

  public:
    RepairMissingBucketsWork(Application& app,
                             HistoryArchiveState const& localState,
                             Handler endHandler);
    ~RepairMissingBucketsWork() = default;

  protected:
    void doReset() override;
    State doWork() override;

    void onFailureRaise() override;
    void onSuccess() override;
};
}

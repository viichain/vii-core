#pragma once


#include "bucket/PublishQueueBuckets.h"
#include "history/HistoryManager.h"
#include "util/TmpDir.h"
#include "work/Work.h"
#include <memory>

namespace medida
{
class Meter;
}

namespace viichain
{

class Application;
class Work;

class HistoryManagerImpl : public HistoryManager
{
    Application& mApp;
    std::unique_ptr<TmpDir> mWorkDir;
    std::shared_ptr<BasicWork> mPublishWork;

    PublishQueueBuckets mPublishQueueBuckets;
    bool mPublishQueueBucketsFilled{false};

    int mPublishQueued{0};
    medida::Meter& mPublishSuccess;
    medida::Meter& mPublishFailure;

    PublishQueueBuckets::BucketCount loadBucketsReferencedByPublishQueue();

  public:
    HistoryManagerImpl(Application& app);
    ~HistoryManagerImpl() override;

    uint32_t getCheckpointFrequency() const override;
    uint32_t checkpointContainingLedger(uint32_t ledger) const override;
    uint32_t prevCheckpointLedger(uint32_t ledger) const override;
    uint32_t nextCheckpointLedger(uint32_t ledger) const override;

    void logAndUpdatePublishStatus() override;

    size_t publishQueueLength() const override;

    bool maybeQueueHistoryCheckpoint() override;

    void queueCurrentHistory() override;

    void takeSnapshotAndPublish(HistoryArchiveState const& has);

    uint32_t getMinLedgerQueuedToPublish() override;

    uint32_t getMaxLedgerQueuedToPublish() override;

    size_t publishQueuedHistory() override;

    std::vector<std::string>
    getMissingBucketsReferencedByPublishQueue() override;

    std::vector<std::string> getBucketsReferencedByPublishQueue() override;

    std::vector<HistoryArchiveState> getPublishQueueStates();

    void historyPublished(uint32_t ledgerSeq,
                          std::vector<std::string> const& originalBuckets,
                          bool success) override;

    void downloadMissingBuckets(
        HistoryArchiveState desiredState,
        std::function<void(asio::error_code const& ec)> handler) override;

    HistoryArchiveState getLastClosedHistoryArchiveState() const override;

    InferredQuorum inferQuorum(uint32_t ledgerNum) override;

    std::string const& getTmpDir() override;

    std::string localFilename(std::string const& basename) override;

    uint64_t getPublishQueueCount() override;
    uint64_t getPublishSuccessCount() override;
    uint64_t getPublishFailureCount() override;
};
}

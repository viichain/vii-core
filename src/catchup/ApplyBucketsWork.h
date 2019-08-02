
#pragma once

#include "bucket/BucketApplicator.h"
#include "work/Work.h"

namespace medida
{
class Meter;
}

namespace viichain
{

class BucketLevel;
class BucketList;
class Bucket;
struct HistoryArchiveState;
struct LedgerHeaderHistoryEntry;

class ApplyBucketsWork : public BasicWork
{
    std::map<std::string, std::shared_ptr<Bucket>> const& mBuckets;
    HistoryArchiveState const& mApplyState;

    bool mApplying;
    size_t mTotalBuckets;
    size_t mAppliedBuckets;
    size_t mAppliedEntries;
    size_t mTotalSize;
    size_t mAppliedSize;
    size_t mLastAppliedSizeMb;
    size_t mLastPos;
    uint32_t mLevel;
    uint32_t mMaxProtocolVersion;
    std::shared_ptr<Bucket const> mSnapBucket;
    std::shared_ptr<Bucket const> mCurrBucket;
    std::unique_ptr<BucketApplicator> mSnapApplicator;
    std::unique_ptr<BucketApplicator> mCurrApplicator;

    medida::Meter& mBucketApplyStart;
    medida::Meter& mBucketApplySuccess;
    medida::Meter& mBucketApplyFailure;
    BucketApplicator::Counters mCounters;

    void advance(std::string const& name, BucketApplicator& applicator);
    std::shared_ptr<Bucket const> getBucket(std::string const& bucketHash);
    BucketLevel& getBucketLevel(uint32_t level);
    void startLevel();
    bool isLevelComplete();

  public:
    ApplyBucketsWork(
        Application& app,
        std::map<std::string, std::shared_ptr<Bucket>> const& buckets,
        HistoryArchiveState const& applyState, uint32_t maxProtocolVersion);
    ~ApplyBucketsWork() = default;

  protected:
    void onReset() override;
    BasicWork::State onRun() override;
    bool
    onAbort() override
    {
        return true;
    };
    void onFailureRaise() override;
    void onFailureRetry() override;
};
}

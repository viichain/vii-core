#pragma once

#include "bucket/BucketList.h"
#include "bucket/BucketManager.h"
#include "overlay/VIIXDR.h"

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>


namespace medida
{
class Timer;
class Meter;
class Counter;
}

namespace viichain
{

class TmpDir;
class Application;
class Bucket;
class BucketList;
struct HistoryArchiveState;

class BucketManagerImpl : public BucketManager
{
    static std::string const kLockFilename;

    Application& mApp;
    BucketList mBucketList;
    std::unique_ptr<TmpDirManager> mTmpDirManager;
    std::unique_ptr<TmpDir> mWorkDir;
    std::map<Hash, std::shared_ptr<Bucket>> mSharedBuckets;
    mutable std::recursive_mutex mBucketMutex;
    std::unique_ptr<std::string> mLockedBucketDir;
    medida::Meter& mBucketObjectInsertBatch;
    medida::Timer& mBucketAddBatch;
    medida::Timer& mBucketSnapMerge;
    medida::Counter& mSharedBucketsSize;
    MergeCounters mMergeCounters;

    std::set<Hash> getReferencedBuckets() const;
    void cleanupStaleFiles();
    void cleanDir();

  protected:
    void calculateSkipValues(LedgerHeader& currentHeader);
    std::string bucketFilename(std::string const& bucketHexHash);
    std::string bucketFilename(Hash const& hash);

  public:
    BucketManagerImpl(Application& app);
    ~BucketManagerImpl() override;
    void initialize() override;
    void dropAll() override;
    std::string const& getTmpDir() override;
    std::string const& getBucketDir() override;
    BucketList& getBucketList() override;
    medida::Timer& getMergeTimer() override;
    MergeCounters readMergeCounters() override;
    void incrMergeCounters(MergeCounters const&) override;
    TmpDirManager& getTmpDirManager() override;
    std::shared_ptr<Bucket> adoptFileAsBucket(std::string const& filename,
                                              uint256 const& hash,
                                              size_t nObjects,
                                              size_t nBytes) override;
    std::shared_ptr<Bucket> getBucketByHash(uint256 const& hash) override;

    void forgetUnreferencedBuckets() override;
    void addBatch(Application& app, uint32_t currLedger,
                  uint32_t currLedgerProtocol,
                  std::vector<LedgerEntry> const& initEntries,
                  std::vector<LedgerEntry> const& liveEntries,
                  std::vector<LedgerKey> const& deadEntries) override;
    void snapshotLedger(LedgerHeader& currentHeader) override;

    std::vector<std::string>
    checkForMissingBucketsFiles(HistoryArchiveState const& has) override;
    void assumeState(HistoryArchiveState const& has,
                     uint32_t maxProtocolVersion) override;
    void shutdown() override;
};

#define SKIP_1 50
#define SKIP_2 5000
#define SKIP_3 50000
#define SKIP_4 500000
}

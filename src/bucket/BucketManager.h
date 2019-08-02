#pragma once


#include "bucket/Bucket.h"
#include "overlay/StellarXDR.h"
#include "util/NonCopyable.h"
#include <memory>

#include "medida/timer_context.h"

namespace viichain
{

class Application;
class BucketList;
class TmpDirManager;
struct LedgerHeader;
struct HistoryArchiveState;

struct MergeCounters
{
    uint64_t mPreInitEntryProtocolMerges{0};
    uint64_t mPostInitEntryProtocolMerges{0};

    uint64_t mNewMetaEntries{0};
    uint64_t mNewInitEntries{0};
    uint64_t mNewLiveEntries{0};
    uint64_t mNewDeadEntries{0};
    uint64_t mOldMetaEntries{0};
    uint64_t mOldInitEntries{0};
    uint64_t mOldLiveEntries{0};
    uint64_t mOldDeadEntries{0};

    uint64_t mOldEntriesDefaultAccepted{0};
    uint64_t mNewEntriesDefaultAccepted{0};
    uint64_t mNewInitEntriesMergedWithOldDead{0};
    uint64_t mOldInitEntriesMergedWithNewLive{0};
    uint64_t mOldInitEntriesMergedWithNewDead{0};
    uint64_t mNewEntriesMergedWithOldNeitherInit{0};

    uint64_t mShadowScanSteps{0};
    uint64_t mMetaEntryShadowElisions{0};
    uint64_t mLiveEntryShadowElisions{0};
    uint64_t mInitEntryShadowElisions{0};
    uint64_t mDeadEntryShadowElisions{0};

    uint64_t mOutputIteratorTombstoneElisions{0};
    uint64_t mOutputIteratorBufferUpdates{0};
    uint64_t mOutputIteratorActualWrites{0};
    MergeCounters& operator+=(MergeCounters const& delta);
};

class BucketManager : NonMovableOrCopyable
{

  public:
    static std::unique_ptr<BucketManager> create(Application&);

    virtual ~BucketManager()
    {
    }
    virtual void initialize() = 0;
    virtual void dropAll() = 0;
    virtual std::string const& getTmpDir() = 0;
    virtual TmpDirManager& getTmpDirManager() = 0;
    virtual std::string const& getBucketDir() = 0;
    virtual BucketList& getBucketList() = 0;

    virtual medida::Timer& getMergeTimer() = 0;

            virtual MergeCounters readMergeCounters() = 0;
    virtual void incrMergeCounters(MergeCounters const& delta) = 0;

                                                virtual std::shared_ptr<Bucket>
    adoptFileAsBucket(std::string const& filename, uint256 const& hash,
                      size_t nObjects, size_t nBytes) = 0;

        virtual std::shared_ptr<Bucket> getBucketByHash(uint256 const& hash) = 0;

                    virtual void forgetUnreferencedBuckets() = 0;

                    virtual void addBatch(Application& app, uint32_t currLedger,
                          uint32_t currLedgerProtocol,
                          std::vector<LedgerEntry> const& initEntries,
                          std::vector<LedgerEntry> const& liveEntries,
                          std::vector<LedgerKey> const& deadEntries) = 0;

            virtual void snapshotLedger(LedgerHeader& currentHeader) = 0;

            virtual std::vector<std::string>
    checkForMissingBucketsFiles(HistoryArchiveState const& has) = 0;

            virtual void assumeState(HistoryArchiveState const& has,
                             uint32_t maxProtocolVersion) = 0;

        virtual void shutdown() = 0;
};
}

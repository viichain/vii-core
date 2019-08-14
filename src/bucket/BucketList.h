#pragma once


#include "bucket/FutureBucket.h"
#include "overlay/VIIXDR.h"
#include "xdrpp/message.h"
#include <future>

namespace viichain
{

class Application;
class Bucket;

namespace testutil
{
class BucketListDepthModifier;
}

class BucketLevel
{
    uint32_t mLevel;
    FutureBucket mNextCurr;
    std::shared_ptr<Bucket> mCurr;
    std::shared_ptr<Bucket> mSnap;

  public:
    BucketLevel(uint32_t i);
    uint256 getHash() const;
    FutureBucket const& getNext() const;
    FutureBucket& getNext();
    std::shared_ptr<Bucket> getCurr() const;
    std::shared_ptr<Bucket> getSnap() const;
    void setNext(FutureBucket const& fb);
    void setCurr(std::shared_ptr<Bucket>);
    void setSnap(std::shared_ptr<Bucket>);
    void commit();
    void prepare(Application& app, uint32_t currLedger,
                 uint32_t currLedgerProtocol, std::shared_ptr<Bucket> snap,
                 std::vector<std::shared_ptr<Bucket>> const& shadows,
                 bool countMergeEvents);
    std::shared_ptr<Bucket> snap();
};

class BucketListDepth
{
    uint32_t mNumLevels;

    BucketListDepth& operator=(uint32_t numLevels);

  public:
    BucketListDepth(uint32_t numLevels);

    operator uint32_t() const;

    friend class testutil::BucketListDepthModifier;
};

class BucketList
{
        static uint32_t mask(uint32_t v, uint32_t m);
    std::vector<BucketLevel> mLevels;

  public:
                static BucketListDepth kNumLevels;

        static uint32_t levelSize(uint32_t level);

        static uint32_t levelHalf(uint32_t level);

            static uint32_t sizeOfCurr(uint32_t ledger, uint32_t level);

            static uint32_t sizeOfSnap(uint32_t ledger, uint32_t level);

            static uint32_t oldestLedgerInCurr(uint32_t ledger, uint32_t level);

            static uint32_t oldestLedgerInSnap(uint32_t ledger, uint32_t level);

            static bool levelShouldSpill(uint32_t ledger, uint32_t level);

        static bool keepDeadEntries(uint32_t level);

            BucketList();

        BucketLevel const& getLevel(uint32_t i) const;

        BucketLevel& getLevel(uint32_t i);

                Hash getHash() const;

                    void restartMerges(Application& app, uint32_t maxProtocolVersion);

                                void addBatch(Application& app, uint32_t currLedger,
                  uint32_t currLedgerProtocol,
                  std::vector<LedgerEntry> const& initEntries,
                  std::vector<LedgerEntry> const& liveEntries,
                  std::vector<LedgerKey> const& deadEntries);
};
}


#include "BucketList.h"
#include "bucket/Bucket.h"
#include "bucket/BucketManager.h"
#include "bucket/LedgerCmp.h"
#include "crypto/Hex.h"
#include "crypto/Random.h"
#include "crypto/SHA.h"
#include "main/Application.h"
#include "util/Logging.h"
#include "util/XDRStream.h"
#include "util/types.h"
#include <cassert>

namespace viichain
{

BucketLevel::BucketLevel(uint32_t i)
    : mLevel(i)
    , mCurr(std::make_shared<Bucket>())
    , mSnap(std::make_shared<Bucket>())
{
}

uint256
BucketLevel::getHash() const
{
    auto hsh = SHA256::create();
    hsh->add(mCurr->getHash());
    hsh->add(mSnap->getHash());
    return hsh->finish();
}

FutureBucket const&
BucketLevel::getNext() const
{
    return mNextCurr;
}

FutureBucket&
BucketLevel::getNext()
{
    return mNextCurr;
}

void
BucketLevel::setNext(FutureBucket const& fb)
{
    mNextCurr = fb;
}

std::shared_ptr<Bucket>
BucketLevel::getCurr() const
{
    return mCurr;
}

std::shared_ptr<Bucket>
BucketLevel::getSnap() const
{
    return mSnap;
}

void
BucketLevel::setCurr(std::shared_ptr<Bucket> b)
{
    mNextCurr.clear();
    mCurr = b;
}

void
BucketLevel::setSnap(std::shared_ptr<Bucket> b)
{
    mSnap = b;
}

void
BucketLevel::commit()
{
    if (mNextCurr.isLive())
    {
        setCurr(mNextCurr.resolve());
                    }
    assert(!mNextCurr.isMerging());
}

void
BucketLevel::prepare(Application& app, uint32_t currLedger,
                     uint32_t currLedgerProtocol, std::shared_ptr<Bucket> snap,
                     std::vector<std::shared_ptr<Bucket>> const& shadows,
                     bool countMergeEvents)
{
            assert(!mNextCurr.isMerging());

    auto curr = mCurr;

                                if (mLevel != 0)
    {
        uint32_t nextChangeLedger =
            currLedger + BucketList::levelHalf(mLevel - 1);
        if (BucketList::levelShouldSpill(nextChangeLedger, mLevel))
        {
                                    curr = std::make_shared<Bucket>();
        }
    }

    mNextCurr =
        FutureBucket(app, curr, snap, shadows, currLedgerProtocol,
                     BucketList::keepDeadEntries(mLevel), countMergeEvents);
    assert(mNextCurr.isMerging());
}

std::shared_ptr<Bucket>
BucketLevel::snap()
{
    mSnap = mCurr;
    mCurr = std::make_shared<Bucket>();
                    return mSnap;
}

BucketListDepth::BucketListDepth(uint32_t numLevels) : mNumLevels(numLevels)
{
}

BucketListDepth&
BucketListDepth::operator=(uint32_t numLevels)
{
    mNumLevels = numLevels;
    return *this;
}

BucketListDepth::operator uint32_t() const
{
    return mNumLevels;
}

uint32_t
BucketList::levelSize(uint32_t level)
{
    assert(level < kNumLevels);
    return 1UL << (2 * (level + 1));
}

uint32_t
BucketList::levelHalf(uint32_t level)
{
    return levelSize(level) >> 1;
}

uint32_t
BucketList::mask(uint32_t v, uint32_t m)
{
    return v & ~(m - 1);
}

uint32_t
BucketList::sizeOfCurr(uint32_t ledger, uint32_t level)
{
    assert(ledger != 0);
    assert(level < kNumLevels);
    if (level == 0)
    {
        return (ledger == 1) ? 1 : (1 + ledger % 2);
    }

    auto const size = levelSize(level);
    auto const half = levelHalf(level);
    if (level != BucketList::kNumLevels - 1 && mask(ledger, half) != 0)
    {
        uint32_t const sizeDelta = 1UL << (2 * level - 1);
        if (mask(ledger, half) == ledger || mask(ledger, size) == ledger)
        {
            return sizeDelta;
        }

        auto const prevSize = levelSize(level - 1);
        auto const prevHalf = levelHalf(level - 1);
        uint32_t previousRelevantLedger =
            std::max({mask(ledger - 1, prevHalf), mask(ledger - 1, prevSize),
                      mask(ledger - 1, half), mask(ledger - 1, size)});
        if (mask(ledger, prevHalf) == ledger ||
            mask(ledger, prevSize) == ledger)
        {
            return sizeOfCurr(previousRelevantLedger, level) + sizeDelta;
        }
        else
        {
            return sizeOfCurr(previousRelevantLedger, level);
        }
    }
    else
    {
        uint32_t blsize = 0;
        for (uint32_t l = 0; l < level; l++)
        {
            blsize += sizeOfCurr(ledger, l);
            blsize += sizeOfSnap(ledger, l);
        }
        return ledger - blsize;
    }
}

uint32_t
BucketList::sizeOfSnap(uint32_t ledger, uint32_t level)
{
    assert(ledger != 0);
    assert(level < kNumLevels);
    if (level == BucketList::kNumLevels - 1)
    {
        return 0;
    }
    else if (mask(ledger, levelSize(level)) != 0)
    {
        return levelHalf(level);
    }
    else
    {
        uint32_t size = 0;
        for (uint32_t l = 0; l < level; l++)
        {
            size += sizeOfCurr(ledger, l);
            size += sizeOfSnap(ledger, l);
        }
        size += sizeOfCurr(ledger, level);
        return ledger - size;
    }
}

uint32_t
BucketList::oldestLedgerInCurr(uint32_t ledger, uint32_t level)
{
    assert(ledger != 0);
    assert(level < kNumLevels);
    if (sizeOfCurr(ledger, level) == 0)
    {
        return std::numeric_limits<uint32_t>::max();
    }

    uint32_t count = ledger;
    for (uint32_t l = 0; l < level; l++)
    {
        count -= sizeOfCurr(ledger, l);
        count -= sizeOfSnap(ledger, l);
    }
    count -= sizeOfCurr(ledger, level);
    return count + 1;
}

uint32_t
BucketList::oldestLedgerInSnap(uint32_t ledger, uint32_t level)
{
    assert(ledger != 0);
    assert(level < kNumLevels);
    if (sizeOfSnap(ledger, level) == 0)
    {
        return std::numeric_limits<uint32_t>::max();
    }

    uint32_t count = ledger;
    for (uint32_t l = 0; l <= level; l++)
    {
        count -= sizeOfCurr(ledger, l);
        count -= sizeOfSnap(ledger, l);
    }
    return count + 1;
}

uint256
BucketList::getHash() const
{
    auto hsh = SHA256::create();
    for (auto const& lev : mLevels)
    {
        hsh->add(lev.getHash());
    }
    return hsh->finish();
}


bool
BucketList::levelShouldSpill(uint32_t ledger, uint32_t level)
{
    if (level == kNumLevels - 1)
    {
                return false;
    }

    return (ledger == mask(ledger, levelHalf(level)) ||
            ledger == mask(ledger, levelSize(level)));
}

bool
BucketList::keepDeadEntries(uint32_t level)
{
    return level < BucketList::kNumLevels - 1;
}

BucketLevel const&
BucketList::getLevel(uint32_t i) const
{
    return mLevels.at(i);
}

BucketLevel&
BucketList::getLevel(uint32_t i)
{
    return mLevels.at(i);
}

void
BucketList::addBatch(Application& app, uint32_t currLedger,
                     uint32_t currLedgerProtocol,
                     std::vector<LedgerEntry> const& initEntries,
                     std::vector<LedgerEntry> const& liveEntries,
                     std::vector<LedgerKey> const& deadEntries)
{
    assert(currLedger > 0);

    std::vector<std::shared_ptr<Bucket>> shadows;
    for (auto& level : mLevels)
    {
        shadows.push_back(level.getCurr());
        shadows.push_back(level.getSnap());
    }

                                                                                                    
    assert(shadows.size() >= 2);
    shadows.pop_back();
    shadows.pop_back();

    for (uint32_t i = static_cast<uint32>(mLevels.size()) - 1; i != 0; --i)
    {
        assert(shadows.size() >= 2);
        shadows.pop_back();
        shadows.pop_back();

                if (levelShouldSpill(currLedger, i - 1))
        {
                        auto snap = mLevels[i - 1].snap();

                                                                        
            mLevels[i].commit();
            mLevels[i].prepare(app, currLedger, currLedgerProtocol, snap,
                               shadows, /*countMergeEvents=*/true);
        }
    }

                bool countMergeEvents =
        !app.getConfig().ARTIFICIALLY_REDUCE_MERGE_COUNTS_FOR_TESTING;
    assert(shadows.size() == 0);
    mLevels[0].prepare(app, currLedger, currLedgerProtocol,
                       Bucket::fresh(app.getBucketManager(), currLedgerProtocol,
                                     initEntries, liveEntries, deadEntries,
                                     countMergeEvents),
                       shadows, countMergeEvents);
    mLevels[0].commit();
}

void
BucketList::restartMerges(Application& app, uint32_t maxProtocolVersion)
{
    for (uint32_t i = 0; i < static_cast<uint32>(mLevels.size()); i++)
    {
        auto& level = mLevels[i];
        auto& next = level.getNext();
        if (next.hasHashes() && !next.isLive())
        {
            next.makeLive(app, maxProtocolVersion, keepDeadEntries(i));
            if (next.isMerging())
            {
                CLOG(INFO, "Bucket")
                    << "Restarted merge on BucketList level " << i;
            }
        }
    }
}

BucketListDepth BucketList::kNumLevels = 11;

BucketList::BucketList()
{
    for (uint32_t i = 0; i < kNumLevels; ++i)
    {
        mLevels.push_back(BucketLevel(i));
    }
}
}


#include "util/asio.h"

#include "bucket/Bucket.h"
#include "bucket/BucketManager.h"
#include "bucket/FutureBucket.h"
#include "crypto/Hex.h"
#include "main/Application.h"
#include "main/ErrorMessages.h"
#include "util/LogSlowExecution.h"
#include "util/Logging.h"
#include "util/format.h"

#include <chrono>

namespace viichain
{

FutureBucket::FutureBucket(Application& app,
                           std::shared_ptr<Bucket> const& curr,
                           std::shared_ptr<Bucket> const& snap,
                           std::vector<std::shared_ptr<Bucket>> const& shadows,
                           uint32_t maxProtocolVersion, bool keepDeadEntries,
                           bool countMergeEvents)
    : mState(FB_LIVE_INPUTS)
    , mInputCurrBucket(curr)
    , mInputSnapBucket(snap)
    , mInputShadowBuckets(shadows)
{
                assert(curr);
    assert(snap);
    mInputCurrBucketHash = binToHex(curr->getHash());
    mInputSnapBucketHash = binToHex(snap->getHash());
    for (auto const& b : mInputShadowBuckets)
    {
        mInputShadowBucketHashes.push_back(binToHex(b->getHash()));
    }
    startMerge(app, maxProtocolVersion, keepDeadEntries, countMergeEvents);
}

void
FutureBucket::setLiveOutput(std::shared_ptr<Bucket> output)
{
    mState = FB_LIVE_OUTPUT;
    mOutputBucketHash = binToHex(output->getHash());
            std::promise<std::shared_ptr<Bucket>> promise;
    mOutputBucket = promise.get_future().share();
    promise.set_value(output);
    checkState();
}

static void
checkHashEq(std::shared_ptr<Bucket> b, std::string const& h)
{
    assert(b->getHash() == hexToBin256(h));
}

void
FutureBucket::checkHashesMatch() const
{
    if (!mInputShadowBuckets.empty())
    {
        assert(mInputShadowBuckets.size() == mInputShadowBucketHashes.size());
        for (size_t i = 0; i < mInputShadowBuckets.size(); ++i)
        {
            checkHashEq(mInputShadowBuckets.at(i),
                        mInputShadowBucketHashes.at(i));
        }
    }
    if (mInputSnapBucket)
    {
        checkHashEq(mInputSnapBucket, mInputSnapBucketHash);
    }
    if (mInputCurrBucket)
    {
        checkHashEq(mInputCurrBucket, mInputCurrBucketHash);
    }
    if (mOutputBucket.valid() && mergeComplete() && !mOutputBucketHash.empty())
    {
        checkHashEq(mOutputBucket.get(), mOutputBucketHash);
    }
}

void
FutureBucket::checkState() const
{
    switch (mState)
    {
    case FB_CLEAR:
        assert(mInputShadowBuckets.empty());
        assert(!mInputSnapBucket);
        assert(!mInputCurrBucket);
        assert(!mOutputBucket.valid());
        assert(mInputShadowBucketHashes.empty());
        assert(mInputSnapBucketHash.empty());
        assert(mInputCurrBucketHash.empty());
        assert(mOutputBucketHash.empty());
        break;

    case FB_LIVE_INPUTS:
        assert(mInputSnapBucket);
        assert(mInputCurrBucket);
        assert(mOutputBucket.valid());
        assert(mOutputBucketHash.empty());
        checkHashesMatch();
        break;

    case FB_LIVE_OUTPUT:
        assert(mergeComplete());
        assert(mOutputBucket.valid());
        assert(!mOutputBucketHash.empty());
        checkHashesMatch();
        break;

    case FB_HASH_INPUTS:
        assert(!mInputSnapBucket);
        assert(!mInputCurrBucket);
        assert(!mOutputBucket.valid());
        assert(!mInputSnapBucketHash.empty());
        assert(!mInputCurrBucketHash.empty());
        assert(mOutputBucketHash.empty());
        break;

    case FB_HASH_OUTPUT:
        assert(!mInputSnapBucket);
        assert(!mInputCurrBucket);
        assert(!mOutputBucket.valid());
        assert(mInputSnapBucketHash.empty());
        assert(mInputCurrBucketHash.empty());
        assert(!mOutputBucketHash.empty());
        break;

    default:
        assert(false);
        break;
    }
}

void
FutureBucket::clearInputs()
{
    mInputShadowBuckets.clear();
    mInputSnapBucket.reset();
    mInputCurrBucket.reset();

    mInputShadowBucketHashes.clear();
    mInputSnapBucketHash.clear();
    mInputCurrBucketHash.clear();
}

void
FutureBucket::clearOutput()
{
            mOutputBucket = std::shared_future<std::shared_ptr<Bucket>>();
    mOutputBucketHash.clear();
}

void
FutureBucket::clear()
{
    mState = FB_CLEAR;
    clearInputs();
    clearOutput();
}

bool
FutureBucket::isLive() const
{
    return (mState == FB_LIVE_INPUTS || mState == FB_LIVE_OUTPUT);
}

bool
FutureBucket::isMerging() const
{
    return mState == FB_LIVE_INPUTS;
}

bool
FutureBucket::hasHashes() const
{
    return (mState == FB_HASH_INPUTS || mState == FB_HASH_OUTPUT);
}

bool
FutureBucket::mergeComplete() const
{
    assert(isLive());
    auto status = mOutputBucket.wait_for(std::chrono::nanoseconds(1));
    return status == std::future_status::ready;
}

std::shared_ptr<Bucket>
FutureBucket::resolve()
{
    checkState();
    assert(isLive());
    clearInputs();
    std::shared_ptr<Bucket> bucket;

    {
        auto timer = LogSlowExecution("Resolving bucket");
        bucket = mOutputBucket.get();
    }

    if (mOutputBucketHash.empty())
    {
        mOutputBucketHash = binToHex(bucket->getHash());
    }
    else
    {
        checkHashEq(bucket, mOutputBucketHash);
    }
    mState = FB_LIVE_OUTPUT;
    checkState();
    return bucket;
}

bool
FutureBucket::hasOutputHash() const
{
    if (mState == FB_LIVE_OUTPUT || mState == FB_HASH_OUTPUT)
    {
        assert(!mOutputBucketHash.empty());
        return true;
    }
    return false;
}

std::string const&
FutureBucket::getOutputHash() const
{
    assert(mState == FB_LIVE_OUTPUT || mState == FB_HASH_OUTPUT);
    assert(!mOutputBucketHash.empty());
    return mOutputBucketHash;
}

void
FutureBucket::startMerge(Application& app, uint32_t maxProtocolVersion,
                         bool keepDeadEntries, bool countMergeEvents)
{
            
    assert(mState == FB_LIVE_INPUTS);

    std::shared_ptr<Bucket> curr = mInputCurrBucket;
    std::shared_ptr<Bucket> snap = mInputSnapBucket;
    std::vector<std::shared_ptr<Bucket>> shadows = mInputShadowBuckets;

    assert(curr);
    assert(snap);
    assert(!mOutputBucket.valid());

    CLOG(TRACE, "Bucket") << "Preparing merge of curr="
                          << hexAbbrev(curr->getHash())
                          << " with snap=" << hexAbbrev(snap->getHash());

    BucketManager& bm = app.getBucketManager();

    using task_t = std::packaged_task<std::shared_ptr<Bucket>()>;
    std::shared_ptr<task_t> task =
        std::make_shared<task_t>([curr, snap, &bm, shadows, maxProtocolVersion,
                                  keepDeadEntries, countMergeEvents]() {
            CLOG(TRACE, "Bucket")
                << "Worker merging curr=" << hexAbbrev(curr->getHash())
                << " with snap=" << hexAbbrev(snap->getHash());

            try
            {
                auto res =
                    Bucket::merge(bm, maxProtocolVersion, curr, snap, shadows,
                                  keepDeadEntries, countMergeEvents);

                CLOG(TRACE, "Bucket")
                    << "Worker finished merging curr="
                    << hexAbbrev(curr->getHash())
                    << " with snap=" << hexAbbrev(snap->getHash());

                return res;
            }
            catch (std::exception const& e)
            {
                throw std::runtime_error(fmt::format(
                    "Error merging bucket curr={} with snap={}: "
                    "{}. {}",
                    hexAbbrev(curr->getHash()), hexAbbrev(snap->getHash()),
                    e.what(), POSSIBLY_CORRUPTED_LOCAL_FS));
            };
        });

    mOutputBucket = task->get_future().share();
    app.postOnBackgroundThread(bind(&task_t::operator(), task),
                               "FutureBucket: merge");
    checkState();
}

void
FutureBucket::makeLive(Application& app, uint32_t maxProtocolVersion,
                       bool keepDeadEntries)
{
    checkState();
    assert(!isLive());
    assert(hasHashes());
    auto& bm = app.getBucketManager();
    if (hasOutputHash())
    {
        setLiveOutput(bm.getBucketByHash(hexToBin256(getOutputHash())));
    }
    else
    {
        assert(mState == FB_HASH_INPUTS);
        mInputCurrBucket =
            bm.getBucketByHash(hexToBin256(mInputCurrBucketHash));
        mInputSnapBucket =
            bm.getBucketByHash(hexToBin256(mInputSnapBucketHash));
        assert(mInputShadowBuckets.empty());
        for (auto const& h : mInputShadowBucketHashes)
        {
            auto b = bm.getBucketByHash(hexToBin256(h));
            assert(b);
            CLOG(DEBUG, "Bucket") << "Reconstituting shadow " << h;
            mInputShadowBuckets.push_back(b);
        }
        mState = FB_LIVE_INPUTS;
        startMerge(app, maxProtocolVersion, keepDeadEntries,
                   true);
        assert(isLive());
    }
}

std::vector<std::string>
FutureBucket::getHashes() const
{
    std::vector<std::string> hashes;
    if (!mInputCurrBucketHash.empty())
    {
        hashes.push_back(mInputCurrBucketHash);
    }
    if (!mInputSnapBucketHash.empty())
    {
        hashes.push_back(mInputSnapBucketHash);
    }
    for (auto h : mInputShadowBucketHashes)
    {
        hashes.push_back(h);
    }
    if (!mOutputBucketHash.empty())
    {
        hashes.push_back(mOutputBucketHash);
    }
    return hashes;
}
}

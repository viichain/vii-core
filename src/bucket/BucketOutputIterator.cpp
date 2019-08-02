
#include "bucket/BucketOutputIterator.h"
#include "bucket/Bucket.h"
#include "bucket/BucketManager.h"
#include "crypto/Random.h"

namespace viichain
{

namespace
{
std::string
randomBucketName(std::string const& tmpDir)
{
    for (;;)
    {
        std::string name =
            tmpDir + "/tmp-bucket-" + binToHex(randomBytes(8)) + ".xdr";
        std::ifstream ifile(name);
        if (!ifile)
        {
            return name;
        }
    }
}
}

BucketOutputIterator::BucketOutputIterator(std::string const& tmpDir,
                                           bool keepDeadEntries,
                                           BucketMetadata const& meta,
                                           MergeCounters& mc)
    : mFilename(randomBucketName(tmpDir))
    , mBuf(nullptr)
    , mHasher(SHA256::create())
    , mKeepDeadEntries(keepDeadEntries)
    , mMeta(meta)
    , mMergeCounters(mc)
{
    CLOG(TRACE, "Bucket") << "BucketOutputIterator opening file to write: "
                          << mFilename;
        mOut.open(mFilename);

    if (meta.ledgerVersion >=
        Bucket::FIRST_PROTOCOL_SUPPORTING_INITENTRY_AND_METAENTRY)
    {
        BucketEntry bme;
        bme.type(METAENTRY);
        bme.metaEntry() = mMeta;
        put(bme);
        mPutMeta = true;
    }
}

void
BucketOutputIterator::put(BucketEntry const& e)
{
    Bucket::checkProtocolLegality(e, mMeta.ledgerVersion);
    if (e.type() == METAENTRY)
    {
        if (mPutMeta)
        {
            throw std::runtime_error(
                "putting META entry in bucket after initial entry");
        }
    }

    if (!mKeepDeadEntries && e.type() == DEADENTRY)
    {
        ++mMergeCounters.mOutputIteratorTombstoneElisions;
        return;
    }

        if (mBuf)
    {
                        assert(!mCmp(e, *mBuf));

                        if (mCmp(*mBuf, e))
        {
            ++mMergeCounters.mOutputIteratorActualWrites;
            mOut.writeOne(*mBuf, mHasher.get(), &mBytesPut);
            mObjectsPut++;
        }
    }
    else
    {
        mBuf = std::make_unique<BucketEntry>();
    }

        ++mMergeCounters.mOutputIteratorBufferUpdates;
    *mBuf = e;
}

std::shared_ptr<Bucket>
BucketOutputIterator::getBucket(BucketManager& bucketManager)
{
    if (mBuf)
    {
        mOut.writeOne(*mBuf, mHasher.get(), &mBytesPut);
        mObjectsPut++;
        mBuf.reset();
    }

    mOut.close();
    if (mObjectsPut == 0 || mBytesPut == 0)
    {
        assert(mObjectsPut == 0);
        assert(mBytesPut == 0);
        CLOG(DEBUG, "Bucket") << "Deleting empty bucket file " << mFilename;
        std::remove(mFilename.c_str());
        return std::make_shared<Bucket>();
    }
    return bucketManager.adoptFileAsBucket(mFilename, mHasher->finish(),
                                           mObjectsPut, mBytesPut);
}
}

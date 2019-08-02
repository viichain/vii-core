
#include "bucket/BucketInputIterator.h"
#include "bucket/Bucket.h"

namespace viichain
{
void
BucketInputIterator::loadEntry()
{
    if (mIn.readOne(mEntry))
    {
        mEntryPtr = &mEntry;
        if (mEntry.type() == METAENTRY)
        {
                                    if (mSeenMetadata)
            {
                throw std::runtime_error(
                    "Malformed bucket: multiple META entries.");
            }
            if (mSeenOtherEntries)
            {
                throw std::runtime_error(
                    "Malformed bucket: META after other entries.");
            }
            mMetadata = mEntry.metaEntry();
            mSeenMetadata = true;
            loadEntry();
        }
        else
        {
            mSeenOtherEntries = true;
            if (mSeenMetadata)
            {
                Bucket::checkProtocolLegality(mEntry, mMetadata.ledgerVersion);
            }
        }
    }
    else
    {
        mEntryPtr = nullptr;
    }
}

size_t
BucketInputIterator::pos()
{
    return mIn.pos();
}

size_t
BucketInputIterator::size() const
{
    return mIn.size();
}

BucketInputIterator::operator bool() const
{
    return mEntryPtr != nullptr;
}

BucketEntry const& BucketInputIterator::operator*()
{
    return *mEntryPtr;
}

bool
BucketInputIterator::seenMetadata() const
{
    return mSeenMetadata;
}

BucketMetadata const&
BucketInputIterator::getMetadata() const
{
    return mMetadata;
}

BucketInputIterator::BucketInputIterator(std::shared_ptr<Bucket const> bucket)
    : mBucket(bucket), mEntryPtr(nullptr), mSeenMetadata(false)
{
                            mMetadata.ledgerVersion = 0;
    if (!mBucket->getFilename().empty())
    {
        CLOG(TRACE, "Bucket") << "BucketInputIterator opening file to read: "
                              << mBucket->getFilename();
        mIn.open(mBucket->getFilename());
        loadEntry();
    }
}

BucketInputIterator::~BucketInputIterator()
{
    mIn.close();
}

BucketInputIterator& BucketInputIterator::operator++()
{
    if (mIn)
    {
        loadEntry();
    }
    else
    {
        mEntryPtr = nullptr;
    }
    return *this;
}
}

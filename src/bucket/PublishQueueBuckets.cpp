
#include "bucket/PublishQueueBuckets.h"

namespace viichain
{

void
PublishQueueBuckets::setBuckets(BucketCount const& buckets)
{
    mBucketUsage = buckets;
}

void
PublishQueueBuckets::addBuckets(std::vector<std::string> const& buckets)
{
    for (auto const& bucket : buckets)
    {
        addBucket(bucket);
    }
}

void
PublishQueueBuckets::addBucket(std::string const& bucket)
{
    mBucketUsage[bucket]++;
}

void
PublishQueueBuckets::removeBuckets(std::vector<std::string> const& buckets)
{
    for (auto const& bucket : buckets)
    {
        removeBucket(bucket);
    }
}

void
PublishQueueBuckets::removeBucket(std::string const& bucket)
{
    auto it = mBucketUsage.find(bucket);
    if (it == std::end(mBucketUsage))
    {
        return;
    }

    it->second--;
    if (it->second == 0)
    {
        mBucketUsage.erase(it);
    }
}
}

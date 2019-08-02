#pragma once


#include <map>
#include <string>
#include <vector>

namespace viichain
{

class PublishQueueBuckets
{
  public:
    using BucketCount = std::map<std::string, int>;

    void setBuckets(BucketCount const& buckets);

    void addBuckets(std::vector<std::string> const& buckets);
    void addBucket(std::string const& bucket);

    void removeBuckets(std::vector<std::string> const& buckets);
    void removeBucket(std::string const& bucket);

    BucketCount const&
    map() const
    {
        return mBucketUsage;
    }

  private:
    BucketCount mBucketUsage;
};
}

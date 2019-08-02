
#pragma once

#include "work/Work.h"
#include "xdr/vii-types.h"

namespace medida
{
class Meter;
}

namespace viichain
{

class Bucket;

class VerifyBucketWork : public BasicWork
{
    std::map<std::string, std::shared_ptr<Bucket>>& mBuckets;
    std::string mBucketFile;
    uint256 mHash;
    bool mDone{false};
    std::error_code mEc;

    void adoptBucket();
    void spawnVerifier();

    medida::Meter& mVerifyBucketSuccess;
    medida::Meter& mVerifyBucketFailure;

  public:
    VerifyBucketWork(Application& app,
                     std::map<std::string, std::shared_ptr<Bucket>>& buckets,
                     std::string const& bucketFile, uint256 const& hash);
    ~VerifyBucketWork() = default;

  protected:
    BasicWork::State onRun() override;
    bool
    onAbort() override
    {
        return true;
    };
};
}

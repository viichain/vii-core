#pragma once


#include "bucket/BucketManager.h"
#include "bucket/LedgerCmp.h"
#include "util/XDRStream.h"
#include "xdr/vii-ledger.h"

#include <memory>
#include <string>

namespace viichain
{

class Bucket;
class BucketManager;

class BucketOutputIterator
{
  protected:
    std::string mFilename;
    XDROutputFileStream mOut;
    BucketEntryIdCmp mCmp;
    std::unique_ptr<BucketEntry> mBuf;
    std::unique_ptr<SHA256> mHasher;
    size_t mBytesPut{0};
    size_t mObjectsPut{0};
    bool mKeepDeadEntries{true};
    BucketMetadata mMeta;
    bool mPutMeta{false};
    MergeCounters& mMergeCounters;

  public:
                                BucketOutputIterator(std::string const& tmpDir, bool keepDeadEntries,
                         BucketMetadata const& meta, MergeCounters& mc);

    void put(BucketEntry const& e);

    std::shared_ptr<Bucket> getBucket(BucketManager& bucketManager);
};
}

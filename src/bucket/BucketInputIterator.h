#pragma once


#include "bucket/LedgerCmp.h"
#include "util/XDRStream.h"
#include "xdr/vii-ledger.h"

#include <memory>

namespace viichain
{

class Bucket;

class BucketInputIterator
{
    std::shared_ptr<Bucket const> mBucket;

                BucketEntry const* mEntryPtr{nullptr};
    XDRInputFileStream mIn;
    BucketEntry mEntry;
    bool mSeenMetadata{false};
    bool mSeenOtherEntries{false};
    BucketMetadata mMetadata;
    void loadEntry();

  public:
    operator bool() const;

                                bool seenMetadata() const;
    BucketMetadata const& getMetadata() const;

    BucketEntry const& operator*();

    BucketInputIterator(std::shared_ptr<Bucket const> bucket);

    ~BucketInputIterator();

    BucketInputIterator& operator++();

    size_t pos();
    size_t size() const;
};
}

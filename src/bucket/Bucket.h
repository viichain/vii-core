#pragma once


#include "bucket/LedgerCmp.h"
#include "crypto/Hex.h"
#include "overlay/VIIXDR.h"
#include "util/NonCopyable.h"
#include "util/XDRStream.h"
#include <string>

namespace viichain
{

class Application;
class BucketManager;
class BucketList;
class Database;

class Bucket : public std::enable_shared_from_this<Bucket>,
               public NonMovableOrCopyable
{

    std::string const mFilename;
    Hash const mHash;
    size_t mSize{0};

  public:
            Bucket();

                Bucket(std::string const& filename, Hash const& hash);

    Hash const& getHash() const;
    std::string const& getFilename() const;
    size_t getSize() const;

            bool containsBucketIdentity(BucketEntry const& id) const;

            static constexpr uint32_t
        FIRST_PROTOCOL_SUPPORTING_INITENTRY_AND_METAENTRY = 11;

    static void checkProtocolLegality(BucketEntry const& entry,
                                      uint32_t protocolVersion);

    static std::vector<BucketEntry>
    convertToBucketEntry(bool useInit,
                         std::vector<LedgerEntry> const& initEntries,
                         std::vector<LedgerEntry> const& liveEntries,
                         std::vector<LedgerKey> const& deadEntries);

                    void apply(Application& app) const;

                static std::shared_ptr<Bucket>
    fresh(BucketManager& bucketManager, uint32_t protocolVersion,
          std::vector<LedgerEntry> const& initEntries,
          std::vector<LedgerEntry> const& liveEntries,
          std::vector<LedgerKey> const& deadEntries, bool countMergeEvents);

                                                static std::shared_ptr<Bucket>
    merge(BucketManager& bucketManager, uint32_t maxProtocolVersion,
          std::shared_ptr<Bucket> const& oldBucket,
          std::shared_ptr<Bucket> const& newBucket,
          std::vector<std::shared_ptr<Bucket>> const& shadows,
          bool keepDeadEntries, bool countMergeEvents);
};
}

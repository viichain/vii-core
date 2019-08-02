
#include "util/asio.h"
#include "bucket/Bucket.h"
#include "bucket/BucketApplicator.h"
#include "bucket/BucketList.h"
#include "bucket/BucketManager.h"
#include "bucket/BucketOutputIterator.h"
#include "bucket/LedgerCmp.h"
#include "crypto/Hex.h"
#include "crypto/Random.h"
#include "crypto/SHA.h"
#include "database/Database.h"
#include "lib/util/format.h"
#include "main/Application.h"
#include "medida/timer.h"
#include "util/Fs.h"
#include "util/Logging.h"
#include "util/TmpDir.h"
#include "util/XDRStream.h"
#include "xdrpp/message.h"
#include <cassert>
#include <future>

namespace viichain
{

Bucket::Bucket(std::string const& filename, Hash const& hash)
    : mFilename(filename), mHash(hash)
{
    assert(filename.empty() || fs::exists(filename));
    if (!filename.empty())
    {
        CLOG(TRACE, "Bucket")
            << "Bucket::Bucket() created, file exists : " << mFilename;
        mSize = fs::size(filename);
    }
}

Bucket::Bucket()
{
}

Hash const&
Bucket::getHash() const
{
    return mHash;
}

std::string const&
Bucket::getFilename() const
{
    return mFilename;
}

size_t
Bucket::getSize() const
{
    return mSize;
}

bool
Bucket::containsBucketIdentity(BucketEntry const& id) const
{
    BucketEntryIdCmp cmp;
    BucketInputIterator iter(shared_from_this());
    while (iter)
    {
        if (!(cmp(*iter, id) || cmp(id, *iter)))
        {
            return true;
        }
        ++iter;
    }
    return false;
}

void
Bucket::apply(Application& app) const
{
    BucketApplicator applicator(app, app.getConfig().LEDGER_PROTOCOL_VERSION,
                                shared_from_this());
    BucketApplicator::Counters counters(std::chrono::system_clock::now());
    while (applicator)
    {
        applicator.advance(counters);
    }
    counters.logInfo("direct", 0, std::chrono::system_clock::now());
}

std::vector<BucketEntry>
Bucket::convertToBucketEntry(bool useInit,
                             std::vector<LedgerEntry> const& initEntries,
                             std::vector<LedgerEntry> const& liveEntries,
                             std::vector<LedgerKey> const& deadEntries)
{
    std::vector<BucketEntry> bucket;
    for (auto const& e : initEntries)
    {
        BucketEntry ce;
        ce.type(useInit ? INITENTRY : LIVEENTRY);
        ce.liveEntry() = e;
        bucket.push_back(ce);
    }
    for (auto const& e : liveEntries)
    {
        BucketEntry ce;
        ce.type(LIVEENTRY);
        ce.liveEntry() = e;
        bucket.push_back(ce);
    }
    for (auto const& e : deadEntries)
    {
        BucketEntry ce;
        ce.type(DEADENTRY);
        ce.deadEntry() = e;
        bucket.push_back(ce);
    }

    BucketEntryIdCmp cmp;
    std::sort(bucket.begin(), bucket.end(), cmp);
    assert(std::adjacent_find(
               bucket.begin(), bucket.end(),
               [&cmp](BucketEntry const& lhs, BucketEntry const& rhs) {
                   return !cmp(lhs, rhs);
               }) == bucket.end());
    return bucket;
}

std::shared_ptr<Bucket>
Bucket::fresh(BucketManager& bucketManager, uint32_t protocolVersion,
              std::vector<LedgerEntry> const& initEntries,
              std::vector<LedgerEntry> const& liveEntries,
              std::vector<LedgerKey> const& deadEntries, bool countMergeEvents)
{
                bool useInit =
        (protocolVersion >= FIRST_PROTOCOL_SUPPORTING_INITENTRY_AND_METAENTRY);

    BucketMetadata meta;
    meta.ledgerVersion = protocolVersion;
    auto entries =
        convertToBucketEntry(useInit, initEntries, liveEntries, deadEntries);

    MergeCounters mc;
    BucketOutputIterator out(bucketManager.getTmpDir(), true, meta, mc);
    for (auto const& e : entries)
    {
        out.put(e);
    }

    if (countMergeEvents)
    {
        bucketManager.incrMergeCounters(mc);
    }
    return out.getBucket(bucketManager);
}

static void
countShadowedEntryType(MergeCounters& mc, BucketEntry const& e)
{
    switch (e.type())
    {
    case METAENTRY:
        ++mc.mMetaEntryShadowElisions;
        break;
    case INITENTRY:
        ++mc.mInitEntryShadowElisions;
        break;
    case LIVEENTRY:
        ++mc.mLiveEntryShadowElisions;
        break;
    case DEADENTRY:
        ++mc.mDeadEntryShadowElisions;
        break;
    }
}

inline void
Bucket::checkProtocolLegality(BucketEntry const& entry,
                              uint32_t protocolVersion)
{
    if (protocolVersion < FIRST_PROTOCOL_SUPPORTING_INITENTRY_AND_METAENTRY &&
        (entry.type() == INITENTRY || entry.type() == METAENTRY))
    {
        throw std::runtime_error(fmt::format(
            "unsupported entry type {} in protocol {} bucket",
            (entry.type() == INITENTRY ? "INIT" : "META"), protocolVersion));
    }
}

inline void
maybePut(BucketOutputIterator& out, BucketEntry const& entry,
         std::vector<BucketInputIterator>& shadowIterators,
         bool keepShadowedLifecycleEntries, MergeCounters& mc)
{
                                                                                                                                                    
    if (keepShadowedLifecycleEntries &&
        (entry.type() == INITENTRY || entry.type() == DEADENTRY))
    {
                out.put(entry);
        return;
    }

    BucketEntryIdCmp cmp;
    for (auto& si : shadowIterators)
    {
                while (si && cmp(*si, entry))
        {
            ++mc.mShadowScanSteps;
            ++si;
        }
                                if (si && !cmp(entry, *si))
        {
                        countShadowedEntryType(mc, entry);
            return;
        }
    }
        out.put(entry);
}

static void
countOldEntryType(MergeCounters& mc, BucketEntry const& e)
{
    switch (e.type())
    {
    case METAENTRY:
        ++mc.mOldMetaEntries;
        break;
    case INITENTRY:
        ++mc.mOldInitEntries;
        break;
    case LIVEENTRY:
        ++mc.mOldLiveEntries;
        break;
    case DEADENTRY:
        ++mc.mOldDeadEntries;
        break;
    }
}

static void
countNewEntryType(MergeCounters& mc, BucketEntry const& e)
{
    switch (e.type())
    {
    case METAENTRY:
        ++mc.mNewMetaEntries;
        break;
    case INITENTRY:
        ++mc.mNewInitEntries;
        break;
    case LIVEENTRY:
        ++mc.mNewLiveEntries;
        break;
    case DEADENTRY:
        ++mc.mNewDeadEntries;
        break;
    }
}

static void
calculateMergeProtocolVersion(
    MergeCounters& mc, uint32_t maxProtocolVersion,
    BucketInputIterator const& oi, BucketInputIterator const& ni,
    std::vector<BucketInputIterator> const& shadowIterators,
    uint32& protocolVersion, bool& keepShadowedLifecycleEntries)
{
    protocolVersion = std::max(oi.getMetadata().ledgerVersion,
                               ni.getMetadata().ledgerVersion);

    for (auto const& si : shadowIterators)
    {
        protocolVersion =
            std::max(si.getMetadata().ledgerVersion, protocolVersion);
    }

    CLOG(TRACE, "Bucket") << "Bucket merge protocolVersion=" << protocolVersion
                          << ", maxProtocolVersion=" << maxProtocolVersion;

    if (protocolVersion > maxProtocolVersion)
    {
        throw std::runtime_error(fmt::format(
            "bucket protocol version {} exceeds maxProtocolVersion {}",
            protocolVersion, maxProtocolVersion));
    }

                    keepShadowedLifecycleEntries = true;
    if (protocolVersion <
        Bucket::FIRST_PROTOCOL_SUPPORTING_INITENTRY_AND_METAENTRY)
    {
        ++mc.mPreInitEntryProtocolMerges;
        keepShadowedLifecycleEntries = false;
    }
    else
    {
        ++mc.mPostInitEntryProtocolMerges;
    }
}

static bool
mergeCasesWithDefaultAcceptance(
    BucketEntryIdCmp const& cmp, MergeCounters& mc, BucketInputIterator& oi,
    BucketInputIterator& ni, BucketOutputIterator& out,
    std::vector<BucketInputIterator>& shadowIterators, uint32_t protocolVersion,
    bool keepShadowedLifecycleEntries)
{
    if (!ni || (oi && ni && cmp(*oi, *ni)))
    {
                                                        ++mc.mOldEntriesDefaultAccepted;
        Bucket::checkProtocolLegality(*oi, protocolVersion);
        countOldEntryType(mc, *oi);
        maybePut(out, *oi, shadowIterators, keepShadowedLifecycleEntries, mc);
        ++oi;
        return true;
    }
    else if (!oi || (oi && ni && cmp(*ni, *oi)))
    {
                                                        ++mc.mNewEntriesDefaultAccepted;
        Bucket::checkProtocolLegality(*ni, protocolVersion);
        countNewEntryType(mc, *ni);
        maybePut(out, *ni, shadowIterators, keepShadowedLifecycleEntries, mc);
        ++ni;
        return true;
    }
    return false;
}

static void
mergeCasesWithEqualKeys(MergeCounters& mc, BucketInputIterator& oi,
                        BucketInputIterator& ni, BucketOutputIterator& out,
                        std::vector<BucketInputIterator>& shadowIterators,
                        uint32_t protocolVersion,
                        bool keepShadowedLifecycleEntries)
{
                                                                                                                                                                                                                                                        
    BucketEntry const& oldEntry = *oi;
    BucketEntry const& newEntry = *ni;
    Bucket::checkProtocolLegality(oldEntry, protocolVersion);
    Bucket::checkProtocolLegality(newEntry, protocolVersion);
    countOldEntryType(mc, oldEntry);
    countNewEntryType(mc, newEntry);

    if (newEntry.type() == INITENTRY)
    {
                        if (oldEntry.type() != DEADENTRY)
        {
            throw std::runtime_error(
                "Malformed bucket: old non-DEAD + new INIT.");
        }
        BucketEntry newLive;
        newLive.type(LIVEENTRY);
        newLive.liveEntry() = newEntry.liveEntry();
        ++mc.mNewInitEntriesMergedWithOldDead;
        maybePut(out, newLive, shadowIterators, keepShadowedLifecycleEntries,
                 mc);
    }
    else if (oldEntry.type() == INITENTRY)
    {
                if (newEntry.type() == LIVEENTRY)
        {
                        BucketEntry newInit;
            newInit.type(INITENTRY);
            newInit.liveEntry() = newEntry.liveEntry();
            ++mc.mOldInitEntriesMergedWithNewLive;
            maybePut(out, newInit, shadowIterators,
                     keepShadowedLifecycleEntries, mc);
        }
        else
        {
                        if (newEntry.type() != DEADENTRY)
            {
                throw std::runtime_error(
                    "Malformed bucket: old INIT + new non-DEAD.");
            }
            ++mc.mOldInitEntriesMergedWithNewDead;
        }
    }
    else
    {
                ++mc.mNewEntriesMergedWithOldNeitherInit;
        maybePut(out, newEntry, shadowIterators, keepShadowedLifecycleEntries,
                 mc);
    }
    ++oi;
    ++ni;
}

std::shared_ptr<Bucket>
Bucket::merge(BucketManager& bucketManager, uint32_t maxProtocolVersion,
              std::shared_ptr<Bucket> const& oldBucket,
              std::shared_ptr<Bucket> const& newBucket,
              std::vector<std::shared_ptr<Bucket>> const& shadows,
              bool keepDeadEntries, bool countMergeEvents)
{
            
    assert(oldBucket);
    assert(newBucket);

    MergeCounters mc;
    BucketInputIterator oi(oldBucket);
    BucketInputIterator ni(newBucket);
    std::vector<BucketInputIterator> shadowIterators(shadows.begin(),
                                                     shadows.end());

    uint32_t protocolVersion;
    bool keepShadowedLifecycleEntries;
    calculateMergeProtocolVersion(mc, maxProtocolVersion, oi, ni,
                                  shadowIterators, protocolVersion,
                                  keepShadowedLifecycleEntries);

    auto timer = bucketManager.getMergeTimer().TimeScope();
    BucketMetadata meta;
    meta.ledgerVersion = protocolVersion;
    BucketOutputIterator out(bucketManager.getTmpDir(), keepDeadEntries, meta,
                             mc);

    BucketEntryIdCmp cmp;
    while (oi || ni)
    {
        if (!mergeCasesWithDefaultAcceptance(cmp, mc, oi, ni, out,
                                             shadowIterators, protocolVersion,
                                             keepShadowedLifecycleEntries))
        {
            mergeCasesWithEqualKeys(mc, oi, ni, out, shadowIterators,
                                    protocolVersion,
                                    keepShadowedLifecycleEntries);
        }
    }
    if (countMergeEvents)
    {
        bucketManager.incrMergeCounters(mc);
    }
    return out.getBucket(bucketManager);
}
}

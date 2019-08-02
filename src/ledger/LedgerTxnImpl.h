
#include "database/Database.h"
#include "ledger/LedgerTxn.h"
#include "util/RandomEvictionCache.h"
#include <list>
#ifdef USE_POSTGRES
#include <iomanip>
#include <libpq-fe.h>
#include <limits>
#include <sstream>
#endif

namespace viichain
{

std::unordered_map<LedgerKey, std::shared_ptr<LedgerEntry const>>
populateLoadedEntries(std::unordered_set<LedgerKey> const& keys,
                      std::vector<LedgerEntry> const& entries);

static const double ENTRY_CACHE_FILL_RATIO = 0.5;

class EntryIterator::AbstractImpl
{
  public:
    virtual ~AbstractImpl()
    {
    }

    virtual void advance() = 0;

    virtual bool atEnd() const = 0;

    virtual LedgerEntry const& entry() const = 0;

    virtual bool entryExists() const = 0;

    virtual LedgerKey const& key() const = 0;

    virtual std::unique_ptr<AbstractImpl> clone() const = 0;
};

class BulkLedgerEntryChangeAccumulator
{

    std::vector<EntryIterator> mAccountsToUpsert;
    std::vector<EntryIterator> mAccountsToDelete;
    std::vector<EntryIterator> mAccountDataToUpsert;
    std::vector<EntryIterator> mAccountDataToDelete;
    std::vector<EntryIterator> mOffersToUpsert;
    std::vector<EntryIterator> mOffersToDelete;
    std::vector<EntryIterator> mTrustLinesToUpsert;
    std::vector<EntryIterator> mTrustLinesToDelete;

  public:
    std::vector<EntryIterator>&
    getAccountsToUpsert()
    {
        return mAccountsToUpsert;
    }

    std::vector<EntryIterator>&
    getAccountsToDelete()
    {
        return mAccountsToDelete;
    }

    std::vector<EntryIterator>&
    getTrustLinesToUpsert()
    {
        return mTrustLinesToUpsert;
    }

    std::vector<EntryIterator>&
    getTrustLinesToDelete()
    {
        return mTrustLinesToDelete;
    }

    std::vector<EntryIterator>&
    getOffersToUpsert()
    {
        return mOffersToUpsert;
    }

    std::vector<EntryIterator>&
    getOffersToDelete()
    {
        return mOffersToDelete;
    }

    std::vector<EntryIterator>&
    getAccountDataToUpsert()
    {
        return mAccountDataToUpsert;
    }

    std::vector<EntryIterator>&
    getAccountDataToDelete()
    {
        return mAccountDataToDelete;
    }

    void accumulate(EntryIterator const& iter);
};

class LedgerTxn::Impl
{
    class EntryIteratorImpl;

    typedef std::unordered_map<LedgerKey, std::shared_ptr<LedgerEntry>>
        EntryMap;

    AbstractLedgerTxnParent& mParent;
    AbstractLedgerTxn* mChild;
    std::unique_ptr<LedgerHeader> mHeader;
    std::shared_ptr<LedgerTxnHeader::Impl> mActiveHeader;
    EntryMap mEntry;
    std::unordered_map<LedgerKey, std::shared_ptr<EntryImplBase>> mActive;
    bool const mShouldUpdateLastModified;
    bool mIsSealed;
    LedgerTxnConsistency mConsistency;

    void throwIfChild() const;
    void throwIfSealed() const;
    void throwIfNotExactConsistency() const;

                        std::map<AccountID, int64_t> getDeltaVotes() const;

        std::map<AccountID, int64_t>
    getTotalVotes(std::vector<InflationWinner> const& parentWinners,
                  std::map<AccountID, int64_t> const& deltaVotes,
                  int64_t minVotes) const;

        std::vector<InflationWinner>
    enumerateInflationWinners(std::map<AccountID, int64_t> const& totalVotes,
                              size_t maxWinners, int64_t minVotes) const;

        EntryIterator getEntryIterator(EntryMap const& entries) const;

        EntryMap maybeUpdateLastModified() const;

            void maybeUpdateLastModifiedThenInvokeThenSeal(
        std::function<void(EntryMap const&)> f);

  public:
        Impl(LedgerTxn& self, AbstractLedgerTxnParent& parent,
         bool shouldUpdateLastModified);

        void addChild(AbstractLedgerTxn& child);

        void commit();

        void commitChild(EntryIterator iter, LedgerTxnConsistency cons);

                        LedgerTxnEntry create(LedgerTxn& self, LedgerEntry const& entry);

        void deactivate(LedgerKey const& key);

        void deactivateHeader();

                        void erase(LedgerKey const& key);

                    std::unordered_map<LedgerKey, LedgerEntry> getAllOffers();

                                    std::shared_ptr<LedgerEntry const>
    getBestOffer(Asset const& buying, Asset const& selling,
                 std::unordered_set<LedgerKey>& exclude);

                        LedgerEntryChanges getChanges();

                        LedgerTxnDelta getDelta();

                    std::unordered_map<LedgerKey, LedgerEntry>
    getOffersByAccountAndAsset(AccountID const& account, Asset const& asset);

        LedgerHeader const& getHeader() const;

                    std::vector<InflationWinner> getInflationWinners(size_t maxWinners,
                                                     int64_t minBalance);

                    std::vector<InflationWinner> queryInflationWinners(size_t maxWinners,
                                                       int64_t minBalance);

        void getAllEntries(std::vector<LedgerEntry>& initEntries,
                       std::vector<LedgerEntry>& liveEntries,
                       std::vector<LedgerKey>& deadEntries);

                        std::shared_ptr<LedgerEntry const>
    getNewestVersion(LedgerKey const& key) const;

                        LedgerTxnEntry load(LedgerTxn& self, LedgerKey const& key);

            void createOrUpdateWithoutLoading(LedgerTxn& self,
                                      LedgerEntry const& entry);

            void eraseWithoutLoading(LedgerKey const& key);

                        std::map<AccountID, std::vector<LedgerTxnEntry>>
    loadAllOffers(LedgerTxn& self);

                                    LedgerTxnEntry loadBestOffer(LedgerTxn& self, Asset const& buying,
                                 Asset const& selling);

        LedgerTxnHeader loadHeader(LedgerTxn& self);

                        std::vector<LedgerTxnEntry>
    loadOffersByAccountAndAsset(LedgerTxn& self, AccountID const& accountID,
                                Asset const& asset);

                        ConstLedgerTxnEntry loadWithoutRecord(LedgerTxn& self,
                                          LedgerKey const& key);

        void rollback();

        void rollbackChild();

        void unsealHeader(LedgerTxn& self, std::function<void(LedgerHeader&)> f);
};

class LedgerTxn::Impl::EntryIteratorImpl : public EntryIterator::AbstractImpl
{
    typedef LedgerTxn::Impl::EntryMap::const_iterator IteratorType;
    IteratorType mIter;
    IteratorType const mEnd;

  public:
    EntryIteratorImpl(IteratorType const& begin, IteratorType const& end);

    void advance() override;

    bool atEnd() const override;

    LedgerEntry const& entry() const override;

    bool entryExists() const override;

    LedgerKey const& key() const override;

    std::unique_ptr<EntryIterator::AbstractImpl> clone() const override;
};

class LedgerTxnRoot::Impl
{
    struct KeyAccesses
    {
        uint64_t hits{0};
        uint64_t misses{0};
    };

    enum class LoadType
    {
        IMMEDIATE,
        PREFETCH
    };

    struct CacheEntry
    {
        std::shared_ptr<LedgerEntry const> entry;
        LoadType type;
    };

    typedef RandomEvictionCache<LedgerKey, CacheEntry> EntryCache;

    typedef std::string BestOffersCacheKey;
    struct BestOffersCacheEntry
    {
        std::list<LedgerEntry> bestOffers;
        bool allLoaded;
    };
    typedef RandomEvictionCache<std::string, BestOffersCacheEntry>
        BestOffersCache;

    Database& mDatabase;
    std::unique_ptr<LedgerHeader> mHeader;
    mutable EntryCache mEntryCache;
    mutable BestOffersCache mBestOffersCache;
    mutable std::unordered_map<LedgerKey, KeyAccesses> mPrefetchMetrics;
    mutable uint64_t mTotalPrefetchHits{0};

    size_t mMaxCacheSize;
    size_t mBulkLoadBatchSize;
    std::unique_ptr<soci::transaction> mTransaction;
    AbstractLedgerTxn* mChild;

    void throwIfChild() const;

    std::shared_ptr<LedgerEntry const> loadAccount(LedgerKey const& key) const;
    std::shared_ptr<LedgerEntry const> loadData(LedgerKey const& key) const;
    std::shared_ptr<LedgerEntry const> loadOffer(LedgerKey const& key) const;
    std::vector<LedgerEntry> loadAllOffers() const;
    std::list<LedgerEntry>::const_iterator
    loadOffers(StatementContext& prep, std::list<LedgerEntry>& offers) const;
    std::list<LedgerEntry>::const_iterator
    loadBestOffers(std::list<LedgerEntry>& offers, Asset const& buying,
                   Asset const& selling, size_t numOffers, size_t offset) const;
    std::vector<LedgerEntry>
    loadOffersByAccountAndAsset(AccountID const& accountID,
                                Asset const& asset) const;
    std::vector<LedgerEntry> loadOffers(StatementContext& prep) const;
    std::vector<InflationWinner> loadInflationWinners(size_t maxWinners,
                                                      int64_t minBalance) const;
    std::shared_ptr<LedgerEntry const>
    loadTrustLine(LedgerKey const& key) const;

    void bulkApply(BulkLedgerEntryChangeAccumulator& bleca,
                   size_t bufferThreshold, LedgerTxnConsistency cons);
    void bulkUpsertAccounts(std::vector<EntryIterator> const& entries);
    void bulkDeleteAccounts(std::vector<EntryIterator> const& entries,
                            LedgerTxnConsistency cons);
    void bulkUpsertTrustLines(std::vector<EntryIterator> const& entries);
    void bulkDeleteTrustLines(std::vector<EntryIterator> const& entries,
                              LedgerTxnConsistency cons);
    void bulkUpsertOffers(std::vector<EntryIterator> const& entries);
    void bulkDeleteOffers(std::vector<EntryIterator> const& entries,
                          LedgerTxnConsistency cons);
    void bulkUpsertAccountData(std::vector<EntryIterator> const& entries);
    void bulkDeleteAccountData(std::vector<EntryIterator> const& entries,
                               LedgerTxnConsistency cons);

    static std::string tableFromLedgerEntryType(LedgerEntryType let);

                                                        std::shared_ptr<LedgerEntry const>
    getFromEntryCache(LedgerKey const& key) const;
    void putInEntryCache(LedgerKey const& key,
                         std::shared_ptr<LedgerEntry const> const& entry,
                         LoadType type) const;

    BestOffersCacheEntry&
    getFromBestOffersCache(Asset const& buying, Asset const& selling,
                           BestOffersCacheEntry& defaultValue) const;

    std::unordered_map<LedgerKey, std::shared_ptr<LedgerEntry const>>
    bulkLoadAccounts(std::unordered_set<LedgerKey> const& keys) const;
    std::unordered_map<LedgerKey, std::shared_ptr<LedgerEntry const>>
    bulkLoadTrustLines(std::unordered_set<LedgerKey> const& keys) const;
    std::unordered_map<LedgerKey, std::shared_ptr<LedgerEntry const>>
    bulkLoadOffers(std::unordered_set<LedgerKey> const& keys) const;
    std::unordered_map<LedgerKey, std::shared_ptr<LedgerEntry const>>
    bulkLoadData(std::unordered_set<LedgerKey> const& keys) const;

  public:
        Impl(Database& db, size_t entryCacheSize, size_t bestOfferCacheSize,
         size_t prefetchBatchSize);

    ~Impl();

        void addChild(AbstractLedgerTxn& child);

        void commitChild(EntryIterator iter, LedgerTxnConsistency cons);

        uint64_t countObjects(LedgerEntryType let) const;
    uint64_t countObjects(LedgerEntryType let,
                          LedgerRange const& ledgers) const;

        void deleteObjectsModifiedOnOrAfterLedger(uint32_t ledger) const;

            void dropAccounts();
    void dropData();
    void dropOffers();
    void dropTrustLines();

                    std::unordered_map<LedgerKey, LedgerEntry> getAllOffers();

                                    std::shared_ptr<LedgerEntry const>
    getBestOffer(Asset const& buying, Asset const& selling,
                 std::unordered_set<LedgerKey>& exclude);

                    std::unordered_map<LedgerKey, LedgerEntry>
    getOffersByAccountAndAsset(AccountID const& account, Asset const& asset);

        LedgerHeader const& getHeader() const;

                    std::vector<InflationWinner> getInflationWinners(size_t maxWinners,
                                                     int64_t minBalance);

                        std::shared_ptr<LedgerEntry const>
    getNewestVersion(LedgerKey const& key) const;

        void rollbackChild();

                    void writeSignersTableIntoAccountsTable();

                    void encodeDataNamesBase64();

                    void encodeHomeDomainsBase64();

                    void writeOffersIntoSimplifiedOffersTable();

                uint32_t prefetch(std::unordered_set<LedgerKey> const& keys);

    double getPrefetchHitRate() const;
};

#ifdef USE_POSTGRES
template <typename T>
inline void
marshalToPGArrayItem(PGconn* conn, std::ostringstream& oss, const T& item)
{
                    oss << std::setprecision(std::numeric_limits<T>::max_digits10) << item;
}

template <>
inline void
marshalToPGArrayItem<std::string>(PGconn* conn, std::ostringstream& oss,
                                  const std::string& item)
{
    std::vector<char> buf(item.size() * 2 + 1, '\0');
    int err = 0;
    size_t len =
        PQescapeStringConn(conn, buf.data(), item.c_str(), item.size(), &err);
    if (err != 0)
    {
        throw std::runtime_error("Could not escape string in SQL");
    }
    oss << '"';
    oss.write(buf.data(), len);
    oss << '"';
}

template <typename T>
inline void
marshalToPGArray(PGconn* conn, std::string& out, const std::vector<T>& v,
                 const std::vector<soci::indicator>* ind = nullptr)
{
    std::ostringstream oss;
    oss << '{';
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i > 0)
        {
            oss << ',';
        }
        if (ind && (*ind)[i] == soci::i_null)
        {
            oss << "NULL";
        }
        else
        {
            marshalToPGArrayItem(conn, oss, v[i]);
        }
    }
    oss << '}';
    out = oss.str();
}
#endif
}

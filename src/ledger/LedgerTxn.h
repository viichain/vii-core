#pragma once


#include "ledger/LedgerTxnEntry.h"
#include "ledger/LedgerTxnHeader.h"
#include "xdr/vii-ledger.h"
#include <functional>
#include <ledger/LedgerHashUtils.h>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>


namespace viichain
{

static const size_t LEDGER_ENTRY_BATCH_COMMIT_SIZE = 0xfff;

enum class LedgerTxnConsistency
{
    EXACT,
    EXTRA_DELETES
};

class Database;
struct InflationVotes;
struct LedgerEntry;
struct LedgerKey;
struct LedgerRange;

bool isBetterOffer(LedgerEntry const& lhsEntry, LedgerEntry const& rhsEntry);

class AbstractLedgerTxn;

struct InflationWinner
{
    AccountID accountID;
    int64_t votes;
};

struct LedgerTxnDelta
{
    struct EntryDelta
    {
        std::shared_ptr<LedgerEntry const> current;
        std::shared_ptr<LedgerEntry const> previous;
    };

    struct HeaderDelta
    {
        LedgerHeader current;
        LedgerHeader previous;
    };

    std::unordered_map<LedgerKey, EntryDelta> entry;
    HeaderDelta header;
};

class EntryIterator
{
  public:
    class AbstractImpl;

  private:
    std::unique_ptr<AbstractImpl> mImpl;

    std::unique_ptr<AbstractImpl> const& getImpl() const;

  public:
    EntryIterator(std::unique_ptr<AbstractImpl>&& impl);

    EntryIterator(EntryIterator const& other);

    EntryIterator(EntryIterator&& other);

    EntryIterator& operator++();

    explicit operator bool() const;

    LedgerEntry const& entry() const;

    bool entryExists() const;

    LedgerKey const& key() const;
};

class AbstractLedgerTxnParent
{
  public:
    virtual ~AbstractLedgerTxnParent();

                virtual void addChild(AbstractLedgerTxn& child) = 0;

                virtual void commitChild(EntryIterator iter, LedgerTxnConsistency cons) = 0;
    virtual void rollbackChild() = 0;

                                        virtual std::unordered_map<LedgerKey, LedgerEntry> getAllOffers() = 0;
    virtual std::shared_ptr<LedgerEntry const>
    getBestOffer(Asset const& buying, Asset const& selling,
                 std::unordered_set<LedgerKey>& exclude) = 0;
    virtual std::unordered_map<LedgerKey, LedgerEntry>
    getOffersByAccountAndAsset(AccountID const& account,
                               Asset const& asset) = 0;

            virtual LedgerHeader const& getHeader() const = 0;

                virtual std::vector<InflationWinner>
    getInflationWinners(size_t maxWinners, int64_t minBalance) = 0;

                        virtual std::shared_ptr<LedgerEntry const>
    getNewestVersion(LedgerKey const& key) const = 0;
};

class AbstractLedgerTxn : public AbstractLedgerTxnParent
{
            friend class LedgerTxnEntry::Impl;
    friend class ConstLedgerTxnEntry::Impl;
    virtual void deactivate(LedgerKey const& key) = 0;

        friend class LedgerTxnHeader::Impl;
    virtual void deactivateHeader() = 0;

  public:
            virtual ~AbstractLedgerTxn();

            virtual void commit() = 0;
    virtual void rollback() = 0;

                                                                                                                            virtual LedgerTxnHeader loadHeader() = 0;
    virtual LedgerTxnEntry create(LedgerEntry const& entry) = 0;
    virtual void erase(LedgerKey const& key) = 0;
    virtual LedgerTxnEntry load(LedgerKey const& key) = 0;
    virtual ConstLedgerTxnEntry loadWithoutRecord(LedgerKey const& key) = 0;

                                        virtual void createOrUpdateWithoutLoading(LedgerEntry const& entry) = 0;
    virtual void eraseWithoutLoading(LedgerKey const& key) = 0;

                                                                                virtual LedgerEntryChanges getChanges() = 0;
    virtual LedgerTxnDelta getDelta() = 0;
    virtual void getAllEntries(std::vector<LedgerEntry>& initEntries,
                               std::vector<LedgerEntry>& liveEntries,
                               std::vector<LedgerKey>& deadEntries) = 0;

                                                        virtual std::map<AccountID, std::vector<LedgerTxnEntry>>
    loadAllOffers() = 0;
    virtual LedgerTxnEntry loadBestOffer(Asset const& buying,
                                         Asset const& selling) = 0;
    virtual std::vector<LedgerTxnEntry>
    loadOffersByAccountAndAsset(AccountID const& accountID,
                                Asset const& asset) = 0;

                virtual std::vector<InflationWinner>
    queryInflationWinners(size_t maxWinners, int64_t minBalance) = 0;

                    virtual void unsealHeader(std::function<void(LedgerHeader&)> f) = 0;
};

class LedgerTxn final : public AbstractLedgerTxn
{
    class Impl;
    std::unique_ptr<Impl> mImpl;

    void deactivate(LedgerKey const& key) override;

    void deactivateHeader() override;

    std::unique_ptr<Impl> const& getImpl() const;

  public:
    explicit LedgerTxn(AbstractLedgerTxnParent& parent,
                       bool shouldUpdateLastModified = true);
    explicit LedgerTxn(LedgerTxn& parent, bool shouldUpdateLastModified = true);

    virtual ~LedgerTxn();

    void addChild(AbstractLedgerTxn& child) override;

    void commit() override;

    void commitChild(EntryIterator iter, LedgerTxnConsistency cons) override;

    LedgerTxnEntry create(LedgerEntry const& entry) override;

    void erase(LedgerKey const& key) override;

    std::unordered_map<LedgerKey, LedgerEntry> getAllOffers() override;

    std::shared_ptr<LedgerEntry const>
    getBestOffer(Asset const& buying, Asset const& selling,
                 std::unordered_set<LedgerKey>& exclude) override;

    LedgerEntryChanges getChanges() override;

    LedgerTxnDelta getDelta() override;

    std::unordered_map<LedgerKey, LedgerEntry>
    getOffersByAccountAndAsset(AccountID const& account,
                               Asset const& asset) override;

    LedgerHeader const& getHeader() const override;

    std::vector<InflationWinner>
    getInflationWinners(size_t maxWinners, int64_t minBalance) override;

    std::vector<InflationWinner>
    queryInflationWinners(size_t maxWinners, int64_t minBalance) override;

    void getAllEntries(std::vector<LedgerEntry>& initEntries,
                       std::vector<LedgerEntry>& liveEntries,
                       std::vector<LedgerKey>& deadEntries) override;

    std::shared_ptr<LedgerEntry const>
    getNewestVersion(LedgerKey const& key) const override;

    LedgerTxnEntry load(LedgerKey const& key) override;

    void createOrUpdateWithoutLoading(LedgerEntry const& entry) override;
    void eraseWithoutLoading(LedgerKey const& key) override;

    std::map<AccountID, std::vector<LedgerTxnEntry>> loadAllOffers() override;

    LedgerTxnEntry loadBestOffer(Asset const& buying,
                                 Asset const& selling) override;

    LedgerTxnHeader loadHeader() override;

    std::vector<LedgerTxnEntry>
    loadOffersByAccountAndAsset(AccountID const& accountID,
                                Asset const& asset) override;

    ConstLedgerTxnEntry loadWithoutRecord(LedgerKey const& key) override;

    void rollback() override;

    void rollbackChild() override;

    void unsealHeader(std::function<void(LedgerHeader&)> f) override;
};

class LedgerTxnRoot : public AbstractLedgerTxnParent
{
    class Impl;
    std::unique_ptr<Impl> const mImpl;

  public:
    explicit LedgerTxnRoot(Database& db, size_t entryCacheSize,
                           size_t bestOfferCacheSize, size_t prefetchBatchSize);

    virtual ~LedgerTxnRoot();

    void addChild(AbstractLedgerTxn& child) override;

    void commitChild(EntryIterator iter, LedgerTxnConsistency cons) override;

    uint64_t countObjects(LedgerEntryType let) const;
    uint64_t countObjects(LedgerEntryType let,
                          LedgerRange const& ledgers) const;

    void deleteObjectsModifiedOnOrAfterLedger(uint32_t ledger) const;

    void dropAccounts();
    void dropData();
    void dropOffers();
    void dropTrustLines();

    std::unordered_map<LedgerKey, LedgerEntry> getAllOffers() override;

    std::shared_ptr<LedgerEntry const>
    getBestOffer(Asset const& buying, Asset const& selling,
                 std::unordered_set<LedgerKey>& exclude) override;

    std::unordered_map<LedgerKey, LedgerEntry>
    getOffersByAccountAndAsset(AccountID const& account,
                               Asset const& asset) override;

    LedgerHeader const& getHeader() const override;

    std::vector<InflationWinner>
    getInflationWinners(size_t maxWinners, int64_t minBalance) override;

    std::shared_ptr<LedgerEntry const>
    getNewestVersion(LedgerKey const& key) const override;

    void rollbackChild() override;

    void writeSignersTableIntoAccountsTable();
    void encodeDataNamesBase64();
    void encodeHomeDomainsBase64();

    void writeOffersIntoSimplifiedOffersTable();
    uint32_t prefetch(std::unordered_set<LedgerKey> const& keys);
    double getPrefetchHitRate() const;
};
}

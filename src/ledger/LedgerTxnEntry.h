#pragma once


#include "xdr/vii-ledger-entries.h"
#include <memory>

namespace viichain
{

class AbstractLedgerTxn;

class EntryImplBase
{
  public:
    virtual ~EntryImplBase()
    {
    }
};

class LedgerTxnEntry
{
  public:
    class Impl;

  private:
    std::weak_ptr<Impl> mImpl;

    std::shared_ptr<Impl> getImpl();
    std::shared_ptr<Impl const> getImpl() const;

  public:
        LedgerTxnEntry();
    explicit LedgerTxnEntry(std::shared_ptr<Impl> const& impl);

    ~LedgerTxnEntry();

        LedgerTxnEntry(LedgerTxnEntry const&) = delete;
    LedgerTxnEntry& operator=(LedgerTxnEntry const&) = delete;

        LedgerTxnEntry(LedgerTxnEntry&& other);
    LedgerTxnEntry& operator=(LedgerTxnEntry&& other);

    explicit operator bool() const;

    LedgerEntry& current();
    LedgerEntry const& current() const;

    void deactivate();

    void erase();

    void swap(LedgerTxnEntry& other);

    static std::shared_ptr<Impl> makeSharedImpl(AbstractLedgerTxn& ltx,
                                                LedgerEntry& current);
};

class ConstLedgerTxnEntry
{
  public:
    class Impl;

  private:
    std::weak_ptr<Impl> mImpl;

    std::shared_ptr<Impl> getImpl();
    std::shared_ptr<Impl const> getImpl() const;

  public:
        ConstLedgerTxnEntry();
    explicit ConstLedgerTxnEntry(std::shared_ptr<Impl> const& impl);

    ~ConstLedgerTxnEntry();

        ConstLedgerTxnEntry(ConstLedgerTxnEntry const&) = delete;
    ConstLedgerTxnEntry& operator=(ConstLedgerTxnEntry const&) = delete;

        ConstLedgerTxnEntry(ConstLedgerTxnEntry&& other);
    ConstLedgerTxnEntry& operator=(ConstLedgerTxnEntry&& other);

    explicit operator bool() const;

    LedgerEntry const& current() const;

    void deactivate();

    void swap(ConstLedgerTxnEntry& other);

    static std::shared_ptr<Impl> makeSharedImpl(AbstractLedgerTxn& ltx,
                                                LedgerEntry const& current);
};

std::shared_ptr<EntryImplBase>
toEntryImplBase(std::shared_ptr<LedgerTxnEntry::Impl> const& impl);

std::shared_ptr<EntryImplBase>
toEntryImplBase(std::shared_ptr<ConstLedgerTxnEntry::Impl> const& impl);
}

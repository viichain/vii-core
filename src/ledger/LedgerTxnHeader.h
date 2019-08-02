#pragma once


#include <memory>

namespace viichain
{

class AbstractLedgerTxn;
struct LedgerHeader;

class LedgerTxnHeader
{
  public:
    class Impl;

  private:
    std::weak_ptr<Impl> mImpl;

    std::shared_ptr<Impl> getImpl();
    std::shared_ptr<Impl const> getImpl() const;

  public:
        explicit LedgerTxnHeader(std::shared_ptr<Impl> const& impl);

    ~LedgerTxnHeader();

        LedgerTxnHeader(LedgerTxnHeader const&) = delete;
    LedgerTxnHeader& operator=(LedgerTxnHeader const&) = delete;

        LedgerTxnHeader(LedgerTxnHeader&& other);
    LedgerTxnHeader& operator=(LedgerTxnHeader&& other);

    explicit operator bool() const;

    LedgerHeader& current();
    LedgerHeader const& current() const;

    void deactivate();

    void swap(LedgerTxnHeader& other);

    static std::shared_ptr<Impl> makeSharedImpl(AbstractLedgerTxn& ltx,
                                                LedgerHeader& current);
};
}

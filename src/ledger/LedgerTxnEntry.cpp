
#include "ledger/LedgerTxnEntry.h"
#include "ledger/LedgerTxn.h"
#include "util/XDROperators.h"
#include "util/types.h"
#include "xdr/vii-ledger-entries.h"

namespace viichain
{

class LedgerTxnEntry::Impl : public EntryImplBase
{
    AbstractLedgerTxn& mLedgerTxn;
    LedgerEntry& mCurrent;

  public:
    explicit Impl(AbstractLedgerTxn& ltx, LedgerEntry& current);

    ~Impl() override;

        Impl(Impl const&) = delete;
    Impl& operator=(Impl const&) = delete;

        Impl(Impl&& other) = delete;
    Impl& operator=(Impl&& other) = delete;

    LedgerEntry& current();
    LedgerEntry const& current() const;

    void deactivate();

    void erase();
};

std::shared_ptr<LedgerTxnEntry::Impl>
LedgerTxnEntry::makeSharedImpl(AbstractLedgerTxn& ltx, LedgerEntry& current)
{
    return std::make_shared<Impl>(ltx, current);
}

std::shared_ptr<EntryImplBase>
toEntryImplBase(std::shared_ptr<LedgerTxnEntry::Impl> const& impl)
{
    return impl;
}

LedgerTxnEntry::LedgerTxnEntry()
{
}

LedgerTxnEntry::LedgerTxnEntry(std::shared_ptr<Impl> const& impl)
    : mImpl(impl) // Constructing weak_ptr from shared_ptr is noexcept
{
}

LedgerTxnEntry::Impl::Impl(AbstractLedgerTxn& ltx, LedgerEntry& current)
    : mLedgerTxn(ltx), mCurrent(current)
{
}

LedgerTxnEntry::~LedgerTxnEntry()
{
    auto impl = mImpl.lock();
    if (impl)
    {
        impl->deactivate();
    }
}

LedgerTxnEntry::Impl::~Impl()
{
}

LedgerTxnEntry::LedgerTxnEntry(LedgerTxnEntry&& other)
    : mImpl(std::move(other.mImpl))
{
        }

LedgerTxnEntry&
LedgerTxnEntry::operator=(LedgerTxnEntry&& other)
{
    if (this != &other)
    {
        LedgerTxnEntry otherCopy(other.mImpl.lock());
        swap(otherCopy);
        other.mImpl.reset();
    }
    return *this;
}

LedgerTxnEntry::operator bool() const
{
    return !mImpl.expired();
}

LedgerEntry&
LedgerTxnEntry::current()
{
    return getImpl()->current();
}

LedgerEntry const&
LedgerTxnEntry::current() const
{
    return getImpl()->current();
}

LedgerEntry&
LedgerTxnEntry::Impl::current()
{
    return mCurrent;
}

LedgerEntry const&
LedgerTxnEntry::Impl::current() const
{
    return mCurrent;
}

void
LedgerTxnEntry::deactivate()
{
    getImpl()->deactivate();
}

void
LedgerTxnEntry::Impl::deactivate()
{
    auto key = LedgerEntryKey(mCurrent);
    mLedgerTxn.deactivate(key);
}

void
LedgerTxnEntry::erase()
{
    getImpl()->erase();
}

void
LedgerTxnEntry::Impl::erase()
{
    auto key = LedgerEntryKey(mCurrent);
    mLedgerTxn.erase(key);
}

std::shared_ptr<LedgerTxnEntry::Impl>
LedgerTxnEntry::getImpl()
{
    auto impl = mImpl.lock();
    if (!impl)
    {
        throw std::runtime_error("LedgerTxnEntry not active");
    }
    return impl;
}

std::shared_ptr<LedgerTxnEntry::Impl const>
LedgerTxnEntry::getImpl() const
{
    auto impl = mImpl.lock();
    if (!impl)
    {
        throw std::runtime_error("LedgerTxnEntry not active");
    }
    return impl;
}

void
LedgerTxnEntry::swap(LedgerTxnEntry& other)
{
    mImpl.swap(other.mImpl);
}

class ConstLedgerTxnEntry::Impl : public EntryImplBase
{
    AbstractLedgerTxn& mLedgerTxn;
    LedgerEntry const mCurrent;

  public:
    explicit Impl(AbstractLedgerTxn& ltx, LedgerEntry const& current);

    ~Impl() override;

        Impl(Impl const&) = delete;
    Impl& operator=(Impl const&) = delete;

        Impl(Impl&& other) = delete;
    Impl& operator=(Impl&& other) = delete;

    LedgerEntry const& current() const;

    void deactivate();
};

std::shared_ptr<ConstLedgerTxnEntry::Impl>
ConstLedgerTxnEntry::makeSharedImpl(AbstractLedgerTxn& ltx,
                                    LedgerEntry const& current)
{
    return std::make_shared<Impl>(ltx, current);
}

std::shared_ptr<EntryImplBase>
toEntryImplBase(std::shared_ptr<ConstLedgerTxnEntry::Impl> const& impl)
{
    return impl;
}

ConstLedgerTxnEntry::ConstLedgerTxnEntry()
{
}

ConstLedgerTxnEntry::ConstLedgerTxnEntry(std::shared_ptr<Impl> const& impl)
    : mImpl(impl) // Constructing weak_ptr from shared_ptr is noexcept
{
}

ConstLedgerTxnEntry::Impl::Impl(AbstractLedgerTxn& ltx,
                                LedgerEntry const& current)
    : mLedgerTxn(ltx), mCurrent(current)
{
}

ConstLedgerTxnEntry::~ConstLedgerTxnEntry()
{
    auto impl = mImpl.lock();
    if (impl)
    {
        impl->deactivate();
    }
}

ConstLedgerTxnEntry::Impl::~Impl()
{
}

ConstLedgerTxnEntry::ConstLedgerTxnEntry(ConstLedgerTxnEntry&& other)
    : mImpl(std::move(other.mImpl))
{
        }

ConstLedgerTxnEntry&
ConstLedgerTxnEntry::operator=(ConstLedgerTxnEntry&& other)
{
    if (this != &other)
    {
        ConstLedgerTxnEntry otherCopy(other.mImpl.lock());
        swap(otherCopy);
        other.mImpl.reset();
    }
    return *this;
}

ConstLedgerTxnEntry::operator bool() const
{
    return !mImpl.expired();
}

LedgerEntry const&
ConstLedgerTxnEntry::current() const
{
    return getImpl()->current();
}

LedgerEntry const&
ConstLedgerTxnEntry::Impl::current() const
{
    return mCurrent;
}

std::shared_ptr<ConstLedgerTxnEntry::Impl>
ConstLedgerTxnEntry::getImpl()
{
    auto impl = mImpl.lock();
    if (!impl)
    {
        throw std::runtime_error("ConstLedgerTxnEntry not active");
    }
    return impl;
}

std::shared_ptr<ConstLedgerTxnEntry::Impl const>
ConstLedgerTxnEntry::getImpl() const
{
    auto impl = mImpl.lock();
    if (!impl)
    {
        throw std::runtime_error("ConstLedgerTxnEntry not active");
    }
    return impl;
}

void
ConstLedgerTxnEntry::deactivate()
{
    getImpl()->deactivate();
}

void
ConstLedgerTxnEntry::Impl::deactivate()
{
    auto key = LedgerEntryKey(mCurrent);
    mLedgerTxn.deactivate(key);
}

void
ConstLedgerTxnEntry::swap(ConstLedgerTxnEntry& other)
{
    mImpl.swap(other.mImpl);
}
}


#include "ledger/LedgerTxnHeader.h"
#include "ledger/LedgerTxn.h"

namespace viichain
{

class LedgerTxnHeader::Impl
{
    AbstractLedgerTxn& mLedgerTxn;
    LedgerHeader& mCurrent;

  public:
    explicit Impl(AbstractLedgerTxn& ltx, LedgerHeader& current);

        Impl(Impl const&) = delete;
    Impl& operator=(Impl const&) = delete;

        Impl(Impl&& other) = delete;
    Impl& operator=(Impl&& other) = delete;

    LedgerHeader& current();
    LedgerHeader const& current() const;

    void deactivate();
};

std::shared_ptr<LedgerTxnHeader::Impl>
LedgerTxnHeader::makeSharedImpl(AbstractLedgerTxn& ltx, LedgerHeader& current)
{
    return std::make_shared<Impl>(ltx, current);
}

LedgerTxnHeader::LedgerTxnHeader(std::shared_ptr<Impl> const& impl)
    : mImpl(impl) // Constructing weak_ptr from shared_ptr is noexcept
{
}

LedgerTxnHeader::Impl::Impl(AbstractLedgerTxn& ltx, LedgerHeader& current)
    : mLedgerTxn(ltx), mCurrent(current)
{
}

LedgerTxnHeader::~LedgerTxnHeader()
{
    auto impl = mImpl.lock();
    if (impl)
    {
        impl->deactivate();
    }
}

LedgerTxnHeader::LedgerTxnHeader(LedgerTxnHeader&& other)
    : mImpl(std::move(other.mImpl))
{
        }

LedgerTxnHeader&
LedgerTxnHeader::operator=(LedgerTxnHeader&& other)
{
    if (this != &other)
    {
        LedgerTxnHeader otherCopy(other.mImpl.lock());
        swap(otherCopy);
        other.mImpl.reset();
    }
    return *this;
}

LedgerTxnHeader::operator bool() const
{
    return !mImpl.expired();
}

LedgerHeader&
LedgerTxnHeader::current()
{
    return getImpl()->current();
}

LedgerHeader const&
LedgerTxnHeader::current() const
{
    return getImpl()->current();
}

LedgerHeader&
LedgerTxnHeader::Impl::current()
{
    return mCurrent;
}

LedgerHeader const&
LedgerTxnHeader::Impl::current() const
{
    return mCurrent;
}

void
LedgerTxnHeader::deactivate()
{
    getImpl()->deactivate();
}

void
LedgerTxnHeader::Impl::deactivate()
{
    mLedgerTxn.deactivateHeader();
}

std::shared_ptr<LedgerTxnHeader::Impl>
LedgerTxnHeader::getImpl()
{
    auto impl = mImpl.lock();
    if (!impl)
    {
        throw std::runtime_error("LedgerTxnHeader not active");
    }
    return impl;
}

std::shared_ptr<LedgerTxnHeader::Impl const>
LedgerTxnHeader::getImpl() const
{
    auto impl = mImpl.lock();
    if (!impl)
    {
        throw std::runtime_error("LedgerTxnHeader not active");
    }
    return impl;
}

void
LedgerTxnHeader::swap(LedgerTxnHeader& other)
{
    mImpl.swap(other.mImpl);
}
}

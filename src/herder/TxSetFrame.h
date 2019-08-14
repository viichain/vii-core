#pragma once


#include "ledger/LedgerHashUtils.h"
#include "overlay/VIIXDR.h"
#include "transactions/TransactionFrame.h"
#include <deque>
#include <functional>
#include <unordered_map>

namespace viichain
{
class Application;

class TxSetFrame;
typedef std::shared_ptr<TxSetFrame> TxSetFramePtr;

class TxSetFrame
{
    bool mHashIsValid;
    Hash mHash;

    Hash mPreviousLedgerHash;

    using AccountTransactionQueue = std::deque<TransactionFramePtr>;

    bool checkOrTrim(Application& app,
                     std::function<bool(TransactionFramePtr, SequenceNumber)>
                         processInvalidTxLambda,
                     std::function<bool(std::deque<TransactionFramePtr> const&)>
                         processLastInvalidTxLambda);

    std::unordered_map<AccountID, AccountTransactionQueue>
    buildAccountTxQueues();
    friend struct SurgeCompare;

  public:
    std::vector<TransactionFramePtr> mTransactions;

    TxSetFrame(Hash const& previousLedgerHash);

    TxSetFrame(TxSetFrame const& other) = default;

        TxSetFrame(Hash const& networkID, TransactionSet const& xdrSet);

        Hash getContentsHash();

    Hash& previousLedgerHash();
    Hash const& previousLedgerHash() const;

    void sortForHash();

    std::vector<TransactionFramePtr> sortForApply();

    bool checkValid(Application& app);

            std::vector<TransactionFramePtr> trimInvalid(Application& app);
    void surgePricingFilter(Application& app);

    void removeTx(TransactionFramePtr tx);

    void
    add(TransactionFramePtr tx)
    {
        mTransactions.push_back(tx);
        mHashIsValid = false;
    }

    size_t size(LedgerHeader const& lh) const;

    size_t
    sizeTx() const
    {
        return mTransactions.size();
    }

    size_t sizeOp() const;

        int64_t getBaseFee(LedgerHeader const& lh) const;

        int64_t getTotalFees(LedgerHeader const& lh) const;
    void toXDR(TransactionSet& set);
};
} // namespace viichain

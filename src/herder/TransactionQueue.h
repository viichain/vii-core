#pragma once


#include "crypto/SecretKey.h"
#include "herder/TxSetFrame.h"
#include "transactions/TransactionFrame.h"
#include "util/HashOfHash.h"
#include "util/XDROperators.h"
#include "xdr/vii-transaction.h"

#include <deque>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace medida
{
class Counter;
}

namespace viichain
{

class Application;

class TransactionQueue
{
  public:
    enum class AddResult
    {
        ADD_STATUS_PENDING = 0,
        ADD_STATUS_DUPLICATE,
        ADD_STATUS_ERROR,
        ADD_STATUS_TRY_AGAIN_LATER,
        ADD_STATUS_COUNT
    };

    struct AccountTxQueueInfo
    {
        SequenceNumber mMaxSeq{0};
        int64_t mTotalFees{0};

        friend bool operator==(AccountTxQueueInfo const& x,
                               AccountTxQueueInfo const& y);
    };

    struct TxMap
    {
        AccountTxQueueInfo mCurrentQInfo;
        std::unordered_map<Hash, TransactionFramePtr> mTransactions;
        void addTx(TransactionFramePtr);
        void recalculate();
    };
    typedef std::unordered_map<AccountID, std::shared_ptr<TxMap>> AccountTxMap;

    explicit TransactionQueue(Application& app, int pendingDepth, int banDepth);

    AddResult tryAdd(TransactionFramePtr tx);
                void remove(std::vector<TransactionFramePtr> const& txs);
            void shift();

    AccountTxQueueInfo
    getAccountTransactionQueueInfo(AccountID const& accountID) const;

    int countBanned(int index) const;
    bool isBanned(Hash const& hash) const;
    std::shared_ptr<TxSetFrame> toTxSet(Hash const& lclHash) const;

  private:
    Application& mApp;
    std::vector<medida::Counter*> mSizeByAge;
    std::deque<AccountTxMap> mPendingTransactions;
    std::deque<std::unordered_set<Hash>> mBannedTransactions;

    bool contains(TransactionFramePtr tx) const;
};

static const char* TX_STATUS_STRING[static_cast<int>(
    TransactionQueue::AddResult::ADD_STATUS_COUNT)] = {
    "PENDING", "DUPLICATE", "ERROR", "TRY_AGAIN_LATER"};
}

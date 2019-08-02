
#pragma once

#include "catchup/CatchupConfiguration.h"
#include "catchup/VerifyLedgerChainWork.h"
#include "history/HistoryArchive.h"
#include "ledger/LedgerRange.h"
#include "work/Work.h"
#include "work/WorkSequence.h"

namespace viichain
{

class HistoryManager;
class Bucket;
class TmpDir;
struct LedgerRange;
struct CheckpointRange;

struct CatchupRange final
{
    struct Ledgers
    {
        uint32_t const mFirst;
        uint32_t const mCount;
    };

    Ledgers mLedgers;
    bool const mApplyBuckets;

        explicit CatchupRange(uint32_t lastClosedLedger,
                          CatchupConfiguration const& configuration,
                          HistoryManager const& historyManager);

    bool
    applyLedgers() const
    {
        return mLedgers.mCount > 0;
    }

    uint32_t getLast() const;
    uint32_t getBucketApplyLedger() const;
};
using WorkSeqPtr = std::shared_ptr<WorkSequence>;


class CatchupWork : public Work
{
  protected:
    HistoryArchiveState mLocalState;
    std::unique_ptr<TmpDir> mDownloadDir;
    std::map<std::string, std::shared_ptr<Bucket>> mBuckets;

    void doReset() override;
    BasicWork::State doWork() override;
    void onFailureRaise() override;
    void onSuccess() override;

  public:
    enum class ProgressState
    {
        APPLIED_BUCKETS,
        APPLIED_TRANSACTIONS,
        FINISHED
    };

                                                                        using ProgressHandler = std::function<void(
        asio::error_code const& ec, ProgressState progressState,
        LedgerHeaderHistoryEntry const& lastClosed,
        CatchupConfiguration::Mode catchupMode)>;

    CatchupWork(Application& app, CatchupConfiguration catchupConfiguration,
                ProgressHandler progressHandler, size_t maxRetries);
    ~CatchupWork();
    std::string getStatus() const override;

  private:
    HistoryArchiveState mRemoteState;
    HistoryArchiveState mApplyBucketsRemoteState;
    LedgerNumHashPair mLastClosedLedgerHashPair;
    CatchupConfiguration const mCatchupConfiguration;
    LedgerHeaderHistoryEntry mVerifiedLedgerRangeStart;
    LedgerHeaderHistoryEntry mLastApplied;
    ProgressHandler mProgressHandler;
    bool mBucketsAppliedEmitted{false};

    std::shared_ptr<BasicWork> mGetHistoryArchiveStateWork;
    std::shared_ptr<BasicWork> mGetBucketStateWork;

    WorkSeqPtr mDownloadVerifyLedgersSeq;
    std::shared_ptr<VerifyLedgerChainWork> mVerifyLedgers;
    WorkSeqPtr mBucketVerifyApplySeq;
    WorkSeqPtr mTransactionsVerifyApplySeq;
    WorkSeqPtr mCatchupSeq;

    bool hasAnyLedgersToCatchupTo() const;
    bool alreadyHaveBucketsHistoryArchiveState(uint32_t atCheckpoint) const;
    void assertBucketState();

    void downloadVerifyLedgerChain(CatchupRange const& catchupRange,
                                   LedgerNumHashPair rangeEnd);
    WorkSeqPtr downloadApplyBuckets();
    WorkSeqPtr downloadApplyTransactions(CatchupRange const& catchupRange);
};
}

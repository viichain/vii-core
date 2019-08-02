
#include "catchup/CatchupWork.h"
#include "catchup/ApplyBucketsWork.h"
#include "catchup/ApplyLedgerChainWork.h"
#include "catchup/CatchupConfiguration.h"
#include "catchup/VerifyLedgerChainWork.h"
#include "history/FileTransferInfo.h"
#include "history/HistoryManager.h"
#include "historywork/BatchDownloadWork.h"
#include "historywork/DownloadBucketsWork.h"
#include "historywork/GetAndUnzipRemoteFileWork.h"
#include "historywork/GetHistoryArchiveStateWork.h"
#include "historywork/VerifyBucketWork.h"
#include "ledger/LedgerManager.h"
#include "main/Application.h"
#include "util/Logging.h"
#include <lib/util/format.h>

namespace viichain
{

CatchupWork::CatchupWork(Application& app,
                         CatchupConfiguration catchupConfiguration,
                         ProgressHandler progressHandler, size_t maxRetries)
    : Work(app, "catchup", maxRetries)
    , mLocalState{app.getHistoryManager().getLastClosedHistoryArchiveState()}
    , mDownloadDir{std::make_unique<TmpDir>(
          mApp.getTmpDirManager().tmpDir(getName()))}
    , mCatchupConfiguration{catchupConfiguration}
    , mProgressHandler{progressHandler}
{
}

CatchupWork::~CatchupWork()
{
}

std::string
CatchupWork::getStatus() const
{
    if (mCatchupSeq)
    {
        return mCatchupSeq->getStatus();
    }
    return BasicWork::getStatus();
}

void
CatchupWork::doReset()
{
    mBucketsAppliedEmitted = false;
    mBuckets.clear();
    mDownloadVerifyLedgersSeq.reset();
    mBucketVerifyApplySeq.reset();
    mTransactionsVerifyApplySeq.reset();
    mGetHistoryArchiveStateWork.reset();
    auto const& lcl = mApp.getLedgerManager().getLastClosedLedgerHeader();
    mLastClosedLedgerHashPair =
        LedgerNumHashPair(lcl.header.ledgerSeq, make_optional<Hash>(lcl.hash));
    mCatchupSeq.reset();
    mGetBucketStateWork.reset();
    mRemoteState = {};
    mApplyBucketsRemoteState = {};
    mLastApplied = mApp.getLedgerManager().getLastClosedLedgerHeader();
}

bool
CatchupWork::hasAnyLedgersToCatchupTo() const
{
    assert(mGetHistoryArchiveStateWork);
    assert(mGetHistoryArchiveStateWork->getState() == State::WORK_SUCCESS);

    if (mLastClosedLedgerHashPair.first <= mRemoteState.currentLedger)
    {
        return true;
    }

    CLOG(INFO, "History")
        << "Last closed ledger is later than current checkpoint: "
        << mLastClosedLedgerHashPair.first << " > "
        << mRemoteState.currentLedger;
    CLOG(INFO, "History") << "Wait until next checkpoint before retrying ";
    CLOG(ERROR, "History") << "Nothing to catchup to ";
    return false;
}

void
CatchupWork::downloadVerifyLedgerChain(CatchupRange const& catchupRange,
                                       LedgerNumHashPair rangeEnd)
{
    auto verifyRange = LedgerRange{catchupRange.mApplyBuckets
                                       ? catchupRange.getBucketApplyLedger()
                                       : catchupRange.mLedgers.mFirst,
                                   catchupRange.getLast()};
    auto checkpointRange =
        CheckpointRange{verifyRange, mApp.getHistoryManager()};
    auto getLedgers = std::make_shared<BatchDownloadWork>(
        mApp, checkpointRange, HISTORY_FILE_TYPE_LEDGER, *mDownloadDir);
    mVerifyLedgers = std::make_shared<VerifyLedgerChainWork>(
        mApp, *mDownloadDir, verifyRange, mLastClosedLedgerHashPair, rangeEnd);

    std::vector<std::shared_ptr<BasicWork>> seq{getLedgers, mVerifyLedgers};
    mDownloadVerifyLedgersSeq =
        addWork<WorkSequence>("download-verify-ledgers-seq", seq);
}

bool
CatchupWork::alreadyHaveBucketsHistoryArchiveState(uint32_t atCheckpoint) const
{
    return atCheckpoint == mRemoteState.currentLedger;
}

WorkSeqPtr
CatchupWork::downloadApplyBuckets()
{
    std::vector<std::string> hashes =
        mApplyBucketsRemoteState.differingBuckets(mLocalState);
    auto getBuckets = std::make_shared<DownloadBucketsWork>(
        mApp, mBuckets, hashes, *mDownloadDir);
    auto applyBuckets = std::make_shared<ApplyBucketsWork>(
        mApp, mBuckets, mApplyBucketsRemoteState,
        mVerifiedLedgerRangeStart.header.ledgerVersion);

    std::vector<std::shared_ptr<BasicWork>> seq{getBuckets, applyBuckets};
    return std::make_shared<WorkSequence>(mApp, "download-verify-apply-buckets",
                                          seq, RETRY_NEVER);
}

void
CatchupWork::assertBucketState()
{
            assert(mApplyBucketsRemoteState.currentLedger ==
           mVerifiedLedgerRangeStart.header.ledgerSeq);
    assert(mApplyBucketsRemoteState.getBucketListHash() ==
           mVerifiedLedgerRangeStart.header.bucketListHash);

                auto lcl = mApp.getLedgerManager().getLastClosedLedgerHeader();
    if (mVerifiedLedgerRangeStart.header.ledgerSeq < lcl.header.ledgerSeq)
    {
        throw std::runtime_error(
            fmt::format("Catchup MINIMAL applying ledger earlier than local "
                        "LCL: {:s} < {:s}",
                        LedgerManager::ledgerAbbrev(mVerifiedLedgerRangeStart),
                        LedgerManager::ledgerAbbrev(lcl)));
    }
}

WorkSeqPtr
CatchupWork::downloadApplyTransactions(CatchupRange const& catchupRange)
{
    auto range =
        LedgerRange{catchupRange.mLedgers.mFirst, catchupRange.getLast()};
    auto checkpointRange = CheckpointRange{range, mApp.getHistoryManager()};
    auto getTxs = std::make_shared<BatchDownloadWork>(
        mApp, checkpointRange, HISTORY_FILE_TYPE_TRANSACTIONS, *mDownloadDir);
    auto applyLedgers = std::make_shared<ApplyLedgerChainWork>(
        mApp, *mDownloadDir, range, mLastApplied);

    std::vector<std::shared_ptr<BasicWork>> seq{getTxs, applyLedgers};
    return std::make_shared<WorkSequence>(mApp, "download-apply-transactions",
                                          seq, RETRY_NEVER);
}

BasicWork::State
CatchupWork::doWork()
{
        if (!mGetHistoryArchiveStateWork)
    {
        auto toLedger = mCatchupConfiguration.toLedger() == 0
                            ? "CURRENT"
                            : std::to_string(mCatchupConfiguration.toLedger());
        CLOG(INFO, "History")
            << "Starting catchup with configuration:\n"
            << "  lastClosedLedger: "
            << mApp.getLedgerManager().getLastClosedLedgerNum() << "\n"
            << "  toLedger: " << toLedger << "\n"
            << "  count: " << mCatchupConfiguration.count();

        auto toCheckpoint =
            mCatchupConfiguration.toLedger() == CatchupConfiguration::CURRENT
                ? CatchupConfiguration::CURRENT
                : mApp.getHistoryManager().nextCheckpointLedger(
                      mCatchupConfiguration.toLedger() + 1) -
                      1;
        mGetHistoryArchiveStateWork =
            addWork<GetHistoryArchiveStateWork>(mRemoteState, toCheckpoint);
        return State::WORK_RUNNING;
    }
    else if (mGetHistoryArchiveStateWork->getState() != State::WORK_SUCCESS)
    {
        return mGetHistoryArchiveStateWork->getState();
    }

        if (!hasAnyLedgersToCatchupTo())
    {
        mApp.getCatchupManager().historyCaughtup();
        asio::error_code ec = std::make_error_code(std::errc::invalid_argument);
        mProgressHandler(ec, ProgressState::FINISHED,
                         LedgerHeaderHistoryEntry{},
                         mCatchupConfiguration.mode());
        return State::WORK_SUCCESS;
    }

    auto resolvedConfiguration =
        mCatchupConfiguration.resolve(mRemoteState.currentLedger);
    auto catchupRange =
        CatchupRange{mLastClosedLedgerHashPair.first, resolvedConfiguration,
                     mApp.getHistoryManager()};

        if (catchupRange.mApplyBuckets)
    {
        auto applyBucketsAt = catchupRange.getBucketApplyLedger();
        if (!alreadyHaveBucketsHistoryArchiveState(applyBucketsAt))
        {
            if (!mGetBucketStateWork)
            {
                mGetBucketStateWork = addWork<GetHistoryArchiveStateWork>(
                    mApplyBucketsRemoteState, applyBucketsAt);
            }
            if (mGetBucketStateWork->getState() != State::WORK_SUCCESS)
            {
                return mGetBucketStateWork->getState();
            }
        }
        else
        {
            mApplyBucketsRemoteState = mRemoteState;
        }
    }

    
        if (mCatchupSeq)
    {
        assert(mDownloadVerifyLedgersSeq);
        assert(mTransactionsVerifyApplySeq || !catchupRange.applyLedgers());

        if (mCatchupSeq->getState() == State::WORK_SUCCESS)
        {
            return State::WORK_SUCCESS;
        }
        else if (mBucketVerifyApplySeq)
        {
            if (mBucketVerifyApplySeq->getState() == State::WORK_SUCCESS &&
                !mBucketsAppliedEmitted)
            {
                mProgressHandler({}, ProgressState::APPLIED_BUCKETS,
                                 mVerifiedLedgerRangeStart,
                                 mCatchupConfiguration.mode());
                mBucketsAppliedEmitted = true;
                mLastApplied =
                    mApp.getLedgerManager().getLastClosedLedgerHeader();
            }
        }
        return mCatchupSeq->getState();
    }
        else if (mDownloadVerifyLedgersSeq)
    {
        if (mDownloadVerifyLedgersSeq->getState() == State::WORK_SUCCESS)
        {
            mVerifiedLedgerRangeStart =
                mVerifyLedgers->getVerifiedLedgerRangeStart();
            if (catchupRange.mApplyBuckets && !mBucketsAppliedEmitted)
            {
                assertBucketState();
            }

            std::vector<std::shared_ptr<BasicWork>> seq;
            if (catchupRange.mApplyBuckets)
            {
                                mBucketVerifyApplySeq = downloadApplyBuckets();
                seq.push_back(mBucketVerifyApplySeq);
            }

            if (catchupRange.applyLedgers())
            {
                                mTransactionsVerifyApplySeq =
                    downloadApplyTransactions(catchupRange);
                seq.push_back(mTransactionsVerifyApplySeq);
            }

            mCatchupSeq =
                addWork<WorkSequence>("catchup-seq", seq, RETRY_NEVER);
            return State::WORK_RUNNING;
        }
        return mDownloadVerifyLedgersSeq->getState();
    }

        downloadVerifyLedgerChain(catchupRange,
                              LedgerNumHashPair(catchupRange.getLast(),
                                                mCatchupConfiguration.hash()));

    return State::WORK_RUNNING;
}

void
CatchupWork::onFailureRaise()
{
    mApp.getCatchupManager().historyCaughtup();
    asio::error_code ec = std::make_error_code(std::errc::timed_out);
    mProgressHandler(ec, ProgressState::FINISHED, LedgerHeaderHistoryEntry{},
                     mCatchupConfiguration.mode());
    Work::onFailureRaise();
}

void
CatchupWork::onSuccess()
{
    mProgressHandler({}, ProgressState::APPLIED_TRANSACTIONS, mLastApplied,
                     mCatchupConfiguration.mode());
    mProgressHandler({}, ProgressState::FINISHED, mLastApplied,
                     mCatchupConfiguration.mode());
    mApp.getCatchupManager().historyCaughtup();
    Work::onSuccess();
}

namespace
{

CatchupRange::Ledgers
computeCatchupledgers(uint32_t lastClosedLedger,
                      CatchupConfiguration const& configuration,
                      HistoryManager const& historyManager)
{
    if (lastClosedLedger == 0)
    {
        throw std::invalid_argument{"lastClosedLedger == 0"};
    }

    if (configuration.toLedger() <= lastClosedLedger)
    {
        throw std::invalid_argument{
            "configuration.toLedger() <= lastClosedLedger"};
    }

    if (configuration.toLedger() == CatchupConfiguration::CURRENT)
    {
        throw std::invalid_argument{
            "configuration.toLedger() == CatchupConfiguration::CURRENT"};
    }

        if (lastClosedLedger > LedgerManager::GENESIS_LEDGER_SEQ)
    {
        return {lastClosedLedger + 1,
                configuration.toLedger() - lastClosedLedger};
    }

        if (configuration.count() >=
        configuration.toLedger() - LedgerManager::GENESIS_LEDGER_SEQ)
    {
        return {LedgerManager::GENESIS_LEDGER_SEQ + 1,
                configuration.toLedger() - LedgerManager::GENESIS_LEDGER_SEQ};
    }

    auto smallestLedgerToApply =
        configuration.toLedger() - std::max(1u, configuration.count()) + 1;

            auto firstCheckpoint = historyManager.checkpointContainingLedger(1);
    auto smallestCheckpointToApply =
        historyManager.checkpointContainingLedger(smallestLedgerToApply);

            if (smallestCheckpointToApply == smallestLedgerToApply)
    {
        return {smallestLedgerToApply + 1,
                configuration.toLedger() - smallestLedgerToApply};
    }

        if (smallestCheckpointToApply == firstCheckpoint)
    {
        return {LedgerManager::GENESIS_LEDGER_SEQ + 1,
                configuration.toLedger() - LedgerManager::GENESIS_LEDGER_SEQ};
    }

            return {smallestCheckpointToApply -
                historyManager.getCheckpointFrequency() + 1,
            configuration.toLedger() - smallestCheckpointToApply +
                historyManager.getCheckpointFrequency()};
}
}

CatchupRange::CatchupRange(uint32_t lastClosedLedger,
                           CatchupConfiguration const& configuration,
                           HistoryManager const& historyManager)
    : mLedgers{computeCatchupledgers(lastClosedLedger, configuration,
                                     historyManager)}
    , mApplyBuckets{mLedgers.mFirst > lastClosedLedger + 1}
{
}

uint32_t
CatchupRange::getLast() const
{
    return mLedgers.mFirst + mLedgers.mCount - 1;
}

uint32_t
CatchupRange::getBucketApplyLedger() const
{
    if (!mApplyBuckets)
    {
        throw std::logic_error("getBucketApplyLedger() cannot be called on "
                               "CatchupRange when mApplyBuckets == false");
    }

    return mLedgers.mFirst - 1;
}
}

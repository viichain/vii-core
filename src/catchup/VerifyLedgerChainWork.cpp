
#include "catchup/VerifyLedgerChainWork.h"
#include "history/FileTransferInfo.h"
#include "historywork/Progress.h"
#include "ledger/LedgerManager.h"
#include "main/Application.h"
#include "main/ErrorMessages.h"
#include "util/FileSystemException.h"
#include "util/XDRStream.h"
#include "util/types.h"
#include <medida/meter.h>
#include <medida/metrics_registry.h>

namespace viichain
{

static HistoryManager::LedgerVerificationStatus
verifyLedgerHistoryEntry(LedgerHeaderHistoryEntry const& hhe)
{
    Hash calculated = sha256(xdr::xdr_to_opaque(hhe.header));
    if (calculated != hhe.hash)
    {
        CLOG(ERROR, "History")
            << "Bad ledger-header history entry: claimed ledger "
            << LedgerManager::ledgerAbbrev(hhe) << " actually hashes to "
            << hexAbbrev(calculated);
        return HistoryManager::VERIFY_STATUS_ERR_BAD_HASH;
    }
    return HistoryManager::VERIFY_STATUS_OK;
}

static HistoryManager::LedgerVerificationStatus
verifyLedgerHistoryLink(Hash const& prev, LedgerHeaderHistoryEntry const& curr)
{
    auto entryResult = verifyLedgerHistoryEntry(curr);
    if (entryResult != HistoryManager::VERIFY_STATUS_OK)
    {
        return entryResult;
    }
    if (prev != curr.header.previousLedgerHash)
    {
        CLOG(ERROR, "History")
            << "Bad hash-chain: " << LedgerManager::ledgerAbbrev(curr)
            << " wants prev hash " << hexAbbrev(curr.header.previousLedgerHash)
            << " but actual prev hash is " << hexAbbrev(prev);
        return HistoryManager::VERIFY_STATUS_ERR_BAD_HASH;
    }
    return HistoryManager::VERIFY_STATUS_OK;
}

static HistoryManager::LedgerVerificationStatus
verifyLastLedgerInCheckpoint(LedgerHeaderHistoryEntry const& ledger,
                             LedgerNumHashPair const& verifiedAhead)
{
            assert(ledger.header.ledgerSeq == verifiedAhead.first);
    auto trustedHash = verifiedAhead.second;
    if (!trustedHash)
    {
        CLOG(DEBUG, "History")
            << "No trusted hash provided to verify "
            << LedgerManager::ledgerAbbrev(ledger) << " against.";
    }
    else
    {
        CLOG(DEBUG, "History")
            << "Verifying ledger " << ledger.header.ledgerSeq
            << " against trusted hash " << hexAbbrev(*trustedHash);
        if (ledger.hash != *trustedHash)
        {
            return HistoryManager::VERIFY_STATUS_ERR_BAD_HASH;
        }
    }
    return HistoryManager::VERIFY_STATUS_OK;
}

VerifyLedgerChainWork::VerifyLedgerChainWork(
    Application& app, TmpDir const& downloadDir, LedgerRange range,
    LedgerNumHashPair const& lastClosedLedger, LedgerNumHashPair ledgerRangeEnd)
    : BasicWork(app, "verify-ledger-chain", RETRY_NEVER)
    , mDownloadDir(downloadDir)
    , mRange(range)
    , mCurrCheckpoint(
          mApp.getHistoryManager().checkpointContainingLedger(mRange.mLast))
    , mLastClosed(lastClosedLedger)
    , mTrustedEndLedger(ledgerRangeEnd)
    , mVerifyLedgerSuccess(app.getMetrics().NewMeter(
          {"history", "verify-ledger", "success"}, "event"))
    , mVerifyLedgerChainSuccess(app.getMetrics().NewMeter(
          {"history", "verify-ledger-chain", "success"}, "event"))
    , mVerifyLedgerChainFailure(app.getMetrics().NewMeter(
          {"history", "verify-ledger-chain", "failure"}, "event"))
{
    assert(range.mLast == ledgerRangeEnd.first);
    assert(lastClosedLedger.second); // LCL hash must be provided
}

std::string
VerifyLedgerChainWork::getStatus() const
{
    if (getState() == State::WORK_RUNNING)
    {
        std::string task = "verifying checkpoint";
        return fmtProgress(mApp, task, mRange.mFirst, mRange.mLast,
                           (mRange.mLast - mCurrCheckpoint));
    }
    return BasicWork::getStatus();
}

void
VerifyLedgerChainWork::onReset()
{
    CLOG(INFO, "History") << "Verifying ledgers " << mRange.toString();

    mVerifiedAhead = LedgerNumHashPair(0, nullptr);
    mVerifiedLedgerRangeStart = {};
    mCurrCheckpoint =
        mApp.getHistoryManager().checkpointContainingLedger(mRange.mLast);
}

HistoryManager::LedgerVerificationStatus
VerifyLedgerChainWork::verifyHistoryOfSingleCheckpoint()
{
                    
    FileTransferInfo ft(mDownloadDir, HISTORY_FILE_TYPE_LEDGER,
                        mCurrCheckpoint);
    XDRInputFileStream hdrIn;
    hdrIn.open(ft.localPath_nogz());

    bool beginCheckpoint = true;
    LedgerHeaderHistoryEntry prev;
    LedgerHeaderHistoryEntry curr;

    CLOG(DEBUG, "History") << "Verifying ledger headers from "
                           << ft.localPath_nogz() << " for checkpoint "
                           << mCurrCheckpoint;

    auto nextCheckpointFirstLedger = mVerifiedAhead;
    while (hdrIn && hdrIn.readOne(curr))
    {
        if (curr.header.ledgerVersion > Config::CURRENT_LEDGER_PROTOCOL_VERSION)
        {
            return HistoryManager::VERIFY_STATUS_ERR_BAD_LEDGER_VERSION;
        }

                if (curr.header.ledgerSeq == mLastClosed.first)
        {
            if (sha256(xdr::xdr_to_opaque(curr.header)) != *mLastClosed.second)
            {
                CLOG(ERROR, "History")
                    << "Bad ledger-header history entry: claimed ledger "
                    << LedgerManager::ledgerAbbrev(curr)
                    << " does not agree with LCL "
                    << LedgerManager::ledgerAbbrev(mLastClosed.first,
                                                   *mLastClosed.second);
                return HistoryManager::VERIFY_STATUS_ERR_BAD_HASH;
            }
        }
                else if (curr.header.ledgerSeq == mLastClosed.first + 1)
        {
            auto lclResult = verifyLedgerHistoryLink(*mLastClosed.second, curr);
            if (lclResult != HistoryManager::VERIFY_STATUS_OK)
            {
                CLOG(ERROR, "History")
                    << "Bad ledger-header history entry: claimed ledger "
                    << LedgerManager::ledgerAbbrev(curr)
                    << " previous hash does not agree with LCL: "
                    << LedgerManager::ledgerAbbrev(mLastClosed.first,
                                                   *mLastClosed.second);
                return lclResult;
            }
        }

        if (beginCheckpoint)
        {
                                                auto hashResult = verifyLedgerHistoryEntry(curr);
            if (hashResult != HistoryManager::VERIFY_STATUS_OK)
            {
                return hashResult;
            }

                                    auto hash = make_optional<Hash>(curr.header.previousLedgerHash);
            mVerifiedAhead = LedgerNumHashPair(curr.header.ledgerSeq - 1, hash);
            beginCheckpoint = false;
        }
        else
        {
            uint32_t expectedSeq = prev.header.ledgerSeq + 1;
            if (curr.header.ledgerSeq < expectedSeq)
            {
                CLOG(ERROR, "History")
                    << "History chain undershot expected ledger seq "
                    << expectedSeq << ", got " << curr.header.ledgerSeq
                    << " instead";
                return HistoryManager::VERIFY_STATUS_ERR_UNDERSHOT;
            }
            else if (curr.header.ledgerSeq > expectedSeq)
            {
                CLOG(ERROR, "History")
                    << "History chain overshot expected ledger seq "
                    << expectedSeq << ", got " << curr.header.ledgerSeq
                    << " instead";
                return HistoryManager::VERIFY_STATUS_ERR_OVERSHOT;
            }
            auto linkResult = verifyLedgerHistoryLink(prev.hash, curr);
            if (linkResult != HistoryManager::VERIFY_STATUS_OK)
            {
                return linkResult;
            }
        }

        mVerifyLedgerSuccess.Mark();
        prev = curr;

                if (curr.header.ledgerSeq == mRange.mLast)
        {
            break;
        }
    }

    if (curr.header.ledgerSeq != mCurrCheckpoint &&
        curr.header.ledgerSeq != mRange.mLast)
    {
                                        CLOG(ERROR, "History") << "History chain did not end with "
                               << mCurrCheckpoint << " or " << mRange.mLast;
        return HistoryManager::VERIFY_STATUS_ERR_MISSING_ENTRIES;
    }

    if (curr.header.ledgerSeq == mRange.mLast)
    {
        assert(nextCheckpointFirstLedger.first == 0);
        nextCheckpointFirstLedger = mTrustedEndLedger;
        CLOG(INFO, "History")
            << (mTrustedEndLedger.second ? "Verifying"
                                         : "Skipping verification for ")
            << "ledger " << LedgerManager::ledgerAbbrev(curr)
            << " against SCP hash";
    }
    else
    {
        assert(nextCheckpointFirstLedger.second);
        assert(nextCheckpointFirstLedger.first != 0);
    }

            auto verifyTrustedHash =
        verifyLastLedgerInCheckpoint(curr, nextCheckpointFirstLedger);
    if (verifyTrustedHash != HistoryManager::VERIFY_STATUS_OK)
    {
        assert(nextCheckpointFirstLedger.second);
        CLOG(ERROR, "History")
            << "Checkpoint does not agree with checkpoint ahead: "
            << "current " << LedgerManager::ledgerAbbrev(curr) << ", verified: "
            << LedgerManager::ledgerAbbrev(nextCheckpointFirstLedger.first,
                                           *(nextCheckpointFirstLedger.second));
        return verifyTrustedHash;
    }

    if (mCurrCheckpoint ==
        mApp.getHistoryManager().checkpointContainingLedger(mRange.mFirst))
    {
        mVerifiedLedgerRangeStart = curr;
    }

    return HistoryManager::VERIFY_STATUS_OK;
}

BasicWork::State
VerifyLedgerChainWork::onRun()
{
    mApp.getCatchupManager().logAndUpdateCatchupStatus(true);

    if (mCurrCheckpoint <
        mApp.getHistoryManager().checkpointContainingLedger(mRange.mFirst))
    {
        throw std::runtime_error(
            "Verification undershot first ledger in the range.");
    }

    HistoryManager::LedgerVerificationStatus result;

        try
    {
        result = verifyHistoryOfSingleCheckpoint();
    }
    catch (FileSystemException&)
    {
        CLOG(ERROR, "History") << "Catchup material failed verification";
        CLOG(ERROR, "History") << POSSIBLY_CORRUPTED_LOCAL_FS;
        mVerifyLedgerChainFailure.Mark();
        return BasicWork::State::WORK_FAILURE;
    }

    switch (result)
    {
    case HistoryManager::VERIFY_STATUS_OK:
        if (mCurrCheckpoint ==
            mApp.getHistoryManager().checkpointContainingLedger(mRange.mFirst))
        {
            CLOG(INFO, "History") << "History chain [" << mRange.mFirst << ","
                                  << mRange.mLast << "] verified";
            mVerifyLedgerChainSuccess.Mark();
            return BasicWork::State::WORK_SUCCESS;
        }

        mCurrCheckpoint -= mApp.getHistoryManager().getCheckpointFrequency();
        return BasicWork::State::WORK_RUNNING;
    case HistoryManager::VERIFY_STATUS_ERR_BAD_LEDGER_VERSION:
        CLOG(ERROR, "History") << "Catchup material failed verification - "
                                  "unsupported ledger version, propagating "
                                  "failure";
        CLOG(ERROR, "History") << UPGRADE_VII_CORE;
        mVerifyLedgerChainFailure.Mark();
        return BasicWork::State::WORK_FAILURE;
    case HistoryManager::VERIFY_STATUS_ERR_BAD_HASH:
        CLOG(ERROR, "History") << "Catchup material failed verification - hash "
                                  "mismatch, propagating failure";
        CLOG(ERROR, "History") << POSSIBLY_CORRUPTED_HISTORY;
        mVerifyLedgerChainFailure.Mark();
        return BasicWork::State::WORK_FAILURE;
    case HistoryManager::VERIFY_STATUS_ERR_OVERSHOT:
        CLOG(ERROR, "History") << "Catchup material failed verification - "
                                  "overshot, propagating failure";
        CLOG(ERROR, "History") << POSSIBLY_CORRUPTED_HISTORY;
        mVerifyLedgerChainFailure.Mark();
        return BasicWork::State::WORK_FAILURE;
    case HistoryManager::VERIFY_STATUS_ERR_UNDERSHOT:
        CLOG(ERROR, "History") << "Catchup material failed verification - "
                                  "undershot, propagating failure";
        CLOG(ERROR, "History") << POSSIBLY_CORRUPTED_HISTORY;
        mVerifyLedgerChainFailure.Mark();
        return BasicWork::State::WORK_FAILURE;
    case HistoryManager::VERIFY_STATUS_ERR_MISSING_ENTRIES:
        CLOG(ERROR, "History") << "Catchup material failed verification - "
                                  "missing entries, propagating failure";
        CLOG(ERROR, "History") << POSSIBLY_CORRUPTED_HISTORY;
        mVerifyLedgerChainFailure.Mark();
        return BasicWork::State::WORK_FAILURE;
    default:
        assert(false);
        throw std::runtime_error("unexpected VerifyLedgerChainWork state");
    }
}
}

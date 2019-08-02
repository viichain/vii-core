
#include "catchup/ApplyLedgerChainWork.h"
#include "herder/LedgerCloseData.h"
#include "history/FileTransferInfo.h"
#include "history/HistoryManager.h"
#include "historywork/Progress.h"
#include "invariant/InvariantDoesNotHold.h"
#include "ledger/CheckpointRange.h"
#include "ledger/LedgerManager.h"
#include "lib/xdrpp/xdrpp/printer.h"
#include "main/Application.h"
#include "main/ErrorMessages.h"
#include "util/FileSystemException.h"

#include <lib/util/format.h>
#include <medida/meter.h>
#include <medida/metrics_registry.h>

namespace viichain
{

ApplyLedgerChainWork::ApplyLedgerChainWork(
    Application& app, TmpDir const& downloadDir, LedgerRange range,
    LedgerHeaderHistoryEntry& lastApplied)
    : BasicWork(app, "apply-ledger-chain", RETRY_NEVER)
    , mDownloadDir(downloadDir)
    , mRange(range)
    , mLastApplied(lastApplied)
    , mApplyLedgerSuccess(app.getMetrics().NewMeter(
          {"history", "apply-ledger-chain", "success"}, "event"))
    , mApplyLedgerFailure(app.getMetrics().NewMeter(
          {"history", "apply-ledger-chain", "failure"}, "event"))
{
}

std::string
ApplyLedgerChainWork::getStatus() const
{
    if (getState() == State::WORK_RUNNING)
    {
        std::string task = "applying checkpoint";
        return fmtProgress(mApp, task, mRange.mFirst, mRange.mLast, mCurrSeq);
    }
    return BasicWork::getStatus();
}

void
ApplyLedgerChainWork::onReset()
{
    auto& lm = mApp.getLedgerManager();
    auto& hm = mApp.getHistoryManager();

    CLOG(INFO, "History") << fmt::format(
        "Applying transactions for ledgers {}, LCL is {}", mRange.toString(),
        LedgerManager::ledgerAbbrev(lm.getLastClosedLedgerHeader()));

    mLastApplied = lm.getLastClosedLedgerHeader();

    mCurrSeq = hm.checkpointContainingLedger(mRange.mFirst);
    mHdrIn.close();
    mTxIn.close();
    mFilesOpen = false;
}

void
ApplyLedgerChainWork::openCurrentInputFiles()
{
    mHdrIn.close();
    mTxIn.close();
    FileTransferInfo hi(mDownloadDir, HISTORY_FILE_TYPE_LEDGER, mCurrSeq);
    FileTransferInfo ti(mDownloadDir, HISTORY_FILE_TYPE_TRANSACTIONS, mCurrSeq);
    CLOG(DEBUG, "History") << "Replaying ledger headers from "
                           << hi.localPath_nogz();
    CLOG(DEBUG, "History") << "Replaying transactions from "
                           << ti.localPath_nogz();
    mHdrIn.open(hi.localPath_nogz());
    mTxIn.open(ti.localPath_nogz());
    mTxHistoryEntry = TransactionHistoryEntry();
    mFilesOpen = true;
}

TxSetFramePtr
ApplyLedgerChainWork::getCurrentTxSet()
{
    auto& lm = mApp.getLedgerManager();
    auto seq = lm.getLastClosedLedgerNum() + 1;

                    do
    {
        if (mTxHistoryEntry.ledgerSeq < seq)
        {
            CLOG(DEBUG, "History")
                << "Skipping txset for ledger " << mTxHistoryEntry.ledgerSeq;
        }
        else if (mTxHistoryEntry.ledgerSeq > seq)
        {
            break;
        }
        else
        {
            assert(mTxHistoryEntry.ledgerSeq == seq);
            CLOG(DEBUG, "History") << "Loaded txset for ledger " << seq;
            return std::make_shared<TxSetFrame>(mApp.getNetworkID(),
                                                mTxHistoryEntry.txSet);
        }
    } while (mTxIn && mTxIn.readOne(mTxHistoryEntry));

    CLOG(DEBUG, "History") << "Using empty txset for ledger " << seq;
    return std::make_shared<TxSetFrame>(lm.getLastClosedLedgerHeader().hash);
}

bool
ApplyLedgerChainWork::applyHistoryOfSingleLedger()
{
    LedgerHeaderHistoryEntry hHeader;
    LedgerHeader& header = hHeader.header;

    if (!mHdrIn || !mHdrIn.readOne(hHeader))
    {
        return false;
    }

    auto& lm = mApp.getLedgerManager();

    auto const& lclHeader = lm.getLastClosedLedgerHeader();

        if (header.ledgerSeq + 1 < lclHeader.header.ledgerSeq)
    {
        CLOG(DEBUG, "History")
            << "Catchup skipping old ledger " << header.ledgerSeq;
        return true;
    }

        if (header.ledgerSeq + 1 == lclHeader.header.ledgerSeq)
    {
        if (hHeader.hash != lclHeader.header.previousLedgerHash)
        {
            throw std::runtime_error(
                fmt::format("replay of {:s} failed to connect on hash of LCL "
                            "predecessor {:s}",
                            LedgerManager::ledgerAbbrev(hHeader),
                            LedgerManager::ledgerAbbrev(
                                lclHeader.header.ledgerSeq - 1,
                                lclHeader.header.previousLedgerHash)));
        }
        CLOG(DEBUG, "History") << "Catchup at 1-before LCL ("
                               << header.ledgerSeq << "), hash correct";
        return true;
    }

        if (header.ledgerSeq == lclHeader.header.ledgerSeq)
    {
        if (hHeader.hash != lm.getLastClosedLedgerHeader().hash)
        {
            mApplyLedgerFailure.Mark();
            throw std::runtime_error(
                fmt::format("replay of {:s} at LCL {:s} disagreed on hash",
                            LedgerManager::ledgerAbbrev(hHeader),
                            LedgerManager::ledgerAbbrev(lclHeader)));
        }
        CLOG(DEBUG, "History")
            << "Catchup at LCL=" << header.ledgerSeq << ", hash correct";
        return true;
    }

        if (header.ledgerSeq != lclHeader.header.ledgerSeq + 1)
    {
        mApplyLedgerFailure.Mark();
        throw std::runtime_error(
            fmt::format("replay overshot current ledger: {:d} > {:d}",
                        header.ledgerSeq, lclHeader.header.ledgerSeq + 1));
    }

        if (header.previousLedgerHash != lm.getLastClosedLedgerHeader().hash)
    {
        mApplyLedgerFailure.Mark();
        throw std::runtime_error(fmt::format(
            "replay at current ledger {:s} disagreed on LCL hash {:s}",
            LedgerManager::ledgerAbbrev(header.ledgerSeq - 1,
                                        header.previousLedgerHash),
            LedgerManager::ledgerAbbrev(lclHeader)));
    }

    auto txset = getCurrentTxSet();
    CLOG(DEBUG, "History") << "Ledger " << header.ledgerSeq << " has "
                           << txset->sizeTx() << " transactions";

                    if (header.scpValue.txSetHash != txset->getContentsHash())
    {
        mApplyLedgerFailure.Mark();
        throw std::runtime_error(fmt::format(
            "replay txset hash differs from txset hash in replay ledger: hash "
            "for txset for {:d} is {:s}, expected {:s}",
            header.ledgerSeq, hexAbbrev(txset->getContentsHash()),
            hexAbbrev(header.scpValue.txSetHash)));
    }

    LedgerCloseData closeData(header.ledgerSeq, txset, header.scpValue);
    lm.closeLedger(closeData);

    CLOG(DEBUG, "History") << "LedgerManager LCL:\n"
                           << xdr::xdr_to_string(
                                  lm.getLastClosedLedgerHeader());
    CLOG(DEBUG, "History") << "Replay header:\n" << xdr::xdr_to_string(hHeader);
    if (lm.getLastClosedLedgerHeader().hash != hHeader.hash)
    {
        mApplyLedgerFailure.Mark();
        throw std::runtime_error(fmt::format(
            "replay of {:s} produced mismatched ledger hash {:s}",
            LedgerManager::ledgerAbbrev(hHeader),
            LedgerManager::ledgerAbbrev(lm.getLastClosedLedgerHeader())));
    }

    mApplyLedgerSuccess.Mark();
    mLastApplied = hHeader;
    return true;
}

BasicWork::State
ApplyLedgerChainWork::onRun()
{
    try
    {
        if (!mFilesOpen)
        {
            openCurrentInputFiles();
        }

        if (!applyHistoryOfSingleLedger())
        {
            mCurrSeq += mApp.getHistoryManager().getCheckpointFrequency();
            mFilesOpen = false;
        }

        mApp.getCatchupManager().logAndUpdateCatchupStatus(true);

        auto& lm = mApp.getLedgerManager();
        auto const& lclHeader = lm.getLastClosedLedgerHeader();
        if (lclHeader.header.ledgerSeq == mRange.mLast)
        {
            return State::WORK_SUCCESS;
        }
        return State::WORK_RUNNING;
    }
    catch (InvariantDoesNotHold&)
    {
                CLOG(ERROR, "History") << "Replay failed";
        throw;
    }
    catch (FileSystemException&)
    {
        CLOG(ERROR, "History") << POSSIBLY_CORRUPTED_LOCAL_FS;
        return State::WORK_FAILURE;
    }
    catch (std::exception& e)
    {
        CLOG(ERROR, "History") << "Replay failed: " << e.what();
        return State::WORK_FAILURE;
    }
}
}

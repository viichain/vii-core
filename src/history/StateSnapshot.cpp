
#include "history/StateSnapshot.h"
#include "bucket/Bucket.h"
#include "bucket/BucketList.h"
#include "bucket/BucketManager.h"
#include "crypto/Hex.h"
#include "database/Database.h"
#include "herder/HerderPersistence.h"
#include "history/FileTransferInfo.h"
#include "history/HistoryArchive.h"
#include "history/HistoryManager.h"
#include "ledger/LedgerHeaderUtils.h"
#include "main/Application.h"
#include "main/Config.h"
#include "transactions/TransactionFrame.h"
#include "util/Logging.h"
#include "util/XDRStream.h"

namespace viichain
{

StateSnapshot::StateSnapshot(Application& app, HistoryArchiveState const& state)
    : mApp(app)
    , mLocalState(state)
    , mSnapDir(app.getTmpDirManager().tmpDir("snapshot"))
    , mLedgerSnapFile(std::make_shared<FileTransferInfo>(
          mSnapDir, HISTORY_FILE_TYPE_LEDGER, mLocalState.currentLedger))

    , mTransactionSnapFile(std::make_shared<FileTransferInfo>(
          mSnapDir, HISTORY_FILE_TYPE_TRANSACTIONS, mLocalState.currentLedger))

    , mTransactionResultSnapFile(std::make_shared<FileTransferInfo>(
          mSnapDir, HISTORY_FILE_TYPE_RESULTS, mLocalState.currentLedger))

    , mSCPHistorySnapFile(std::make_shared<FileTransferInfo>(
          mSnapDir, HISTORY_FILE_TYPE_SCP, mLocalState.currentLedger))

{
    makeLive();
}

void
StateSnapshot::makeLive()
{
    for (uint32_t i = 0;
         i < static_cast<uint32>(mLocalState.currentBuckets.size()); i++)
    {
        auto& hb = mLocalState.currentBuckets[i];
        if (hb.next.hasHashes() && !hb.next.isLive())
        {
                                                                                                                                    uint32_t maxProtocolVersion =
                Config::CURRENT_LEDGER_PROTOCOL_VERSION;
            hb.next.makeLive(mApp, maxProtocolVersion,
                             BucketList::keepDeadEntries(i));
        }
    }
}

bool
StateSnapshot::writeHistoryBlocks() const
{
    std::unique_ptr<soci::session> snapSess(
        mApp.getDatabase().canUsePool()
            ? std::make_unique<soci::session>(mApp.getDatabase().getPool())
            : nullptr);
    soci::session& sess(snapSess ? *snapSess : mApp.getDatabase().getSession());
    soci::transaction tx(sess);

                        size_t nbSCPMessages;
    uint32_t begin, count;
    size_t nHeaders;
    {
        XDROutputFileStream ledgerOut, txOut, txResultOut, scpHistory;
        ledgerOut.open(mLedgerSnapFile->localPath_nogz());
        txOut.open(mTransactionSnapFile->localPath_nogz());
        txResultOut.open(mTransactionResultSnapFile->localPath_nogz());
        scpHistory.open(mSCPHistorySnapFile->localPath_nogz());

                                                begin = mApp.getHistoryManager().prevCheckpointLedger(
            mLocalState.currentLedger);

        count = (mLocalState.currentLedger - begin) + 1;
        CLOG(DEBUG, "History") << "Streaming " << count
                               << " ledgers worth of history, from " << begin;

        nHeaders = LedgerHeaderUtils::copyToStream(mApp.getDatabase(), sess,
                                                   begin, count, ledgerOut);
        size_t nTxs = TransactionFrame::copyTransactionsToStream(
            mApp.getNetworkID(), mApp.getDatabase(), sess, begin, count, txOut,
            txResultOut);
        CLOG(DEBUG, "History") << "Wrote " << nHeaders << " ledger headers to "
                               << mLedgerSnapFile->localPath_nogz();
        CLOG(DEBUG, "History")
            << "Wrote " << nTxs << " transactions to "
            << mTransactionSnapFile->localPath_nogz() << " and "
            << mTransactionResultSnapFile->localPath_nogz();

        nbSCPMessages = HerderPersistence::copySCPHistoryToStream(
            mApp.getDatabase(), sess, begin, count, scpHistory);

        CLOG(DEBUG, "History")
            << "Wrote " << nbSCPMessages << " SCP messages to "
            << mSCPHistorySnapFile->localPath_nogz();
    }

    if (nbSCPMessages == 0)
    {
                std::remove(mSCPHistorySnapFile->localPath_nogz().c_str());
    }

                                            if (!((begin == 0 && nHeaders == count - 1) || nHeaders == count))
    {
        CLOG(WARNING, "History")
            << "Only wrote " << nHeaders << " ledger headers for "
            << mLedgerSnapFile->localPath_nogz() << ", expecting " << count
            << ", will retry";
        return false;
    }

    return true;
}
}

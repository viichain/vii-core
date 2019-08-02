
#include "transactions/MergeOpFrame.h"
#include "database/Database.h"
#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnEntry.h"
#include "ledger/LedgerTxnHeader.h"
#include "main/Application.h"
#include "transactions/TransactionUtils.h"
#include "util/Logging.h"
#include "util/XDROperators.h"

using namespace soci;

namespace viichain
{

MergeOpFrame::MergeOpFrame(Operation const& op, OperationResult& res,
                           TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx)
{
}

ThresholdLevel
MergeOpFrame::getThresholdLevel() const
{
    return ThresholdLevel::HIGH;
}

bool
MergeOpFrame::doApply(AbstractLedgerTxn& ltx)
{
    auto header = ltx.loadHeader();

    auto otherAccount =
        viichain::loadAccount(ltx, mOperation.body.destination());
    if (!otherAccount)
    {
        innerResult().code(ACCOUNT_MERGE_NO_ACCOUNT);
        return false;
    }

    int64_t sourceBalance = 0;
    if (header.current().ledgerVersion > 4 &&
        header.current().ledgerVersion < 8)
    {
                LedgerKey key(ACCOUNT);
        key.account().accountID = getSourceID();
        auto thisAccount = ltx.loadWithoutRecord(key);
        if (!thisAccount)
        {
            innerResult().code(ACCOUNT_MERGE_NO_ACCOUNT);
            return false;
        }

        if (header.current().ledgerVersion > 5)
        {
            sourceBalance = thisAccount.current().data.account().balance;
        }
    }

    auto sourceAccountEntry = loadSourceAccount(ltx, header);
    auto const& sourceAccount = sourceAccountEntry.current().data.account();
        if (header.current().ledgerVersion <= 5 ||
        header.current().ledgerVersion >= 8)
    {
        sourceBalance = sourceAccount.balance;
    }

    if (isImmutableAuth(sourceAccountEntry))
    {
        innerResult().code(ACCOUNT_MERGE_IMMUTABLE_SET);
        return false;
    }

    if (sourceAccount.numSubEntries != sourceAccount.signers.size())
    {
        innerResult().code(ACCOUNT_MERGE_HAS_SUB_ENTRIES);
        return false;
    }

    if (header.current().ledgerVersion >= 10)
    {
        SequenceNumber maxSeq = getStartingSequenceNumber(header);

                        if (sourceAccount.seqNum >= maxSeq)
        {
            innerResult().code(ACCOUNT_MERGE_SEQNUM_TOO_FAR);
            return false;
        }
    }

        if (!addBalance(header, otherAccount, sourceBalance))
    {
        innerResult().code(ACCOUNT_MERGE_DEST_FULL);
        return false;
    }

    sourceAccountEntry.erase();

    innerResult().code(ACCOUNT_MERGE_SUCCESS);
    innerResult().sourceAccountBalance() = sourceBalance;
    return true;
}

bool
MergeOpFrame::doCheckValid(uint32_t ledgerVersion)
{
        if (getSourceID() == mOperation.body.destination())
    {
        innerResult().code(ACCOUNT_MERGE_MALFORMED);
        return false;
    }
    return true;
}
}


#include "transactions/BumpSequenceOpFrame.h"
#include "crypto/SignerKey.h"
#include "database/Database.h"
#include "main/Application.h"
#include "transactions/TransactionFrame.h"
#include "util/XDROperators.h"

namespace viichain
{
BumpSequenceOpFrame::BumpSequenceOpFrame(Operation const& op,
                                         OperationResult& res,
                                         TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx)
    , mBumpSequenceOp(mOperation.body.bumpSequenceOp())
{
}

ThresholdLevel
BumpSequenceOpFrame::getThresholdLevel() const
{
        return ThresholdLevel::LOW;
}

bool
BumpSequenceOpFrame::isVersionSupported(uint32_t protocolVersion) const
{
    return protocolVersion >= 10;
}

bool
BumpSequenceOpFrame::doApply(AbstractLedgerTxn& ltx)
{
    LedgerTxn ltxInner(ltx);
    auto header = ltxInner.loadHeader();
    auto sourceAccountEntry = loadSourceAccount(ltxInner, header);
    auto& sourceAccount = sourceAccountEntry.current().data.account();
    SequenceNumber current = sourceAccount.seqNum;

        if (mBumpSequenceOp.bumpTo > current)
    {
        sourceAccount.seqNum = mBumpSequenceOp.bumpTo;
        ltxInner.commit();
    }

        innerResult().code(BUMP_SEQUENCE_SUCCESS);
    return true;
}

bool
BumpSequenceOpFrame::doCheckValid(uint32_t ledgerVersion)
{
    if (mBumpSequenceOp.bumpTo < 0)
    {
        innerResult().code(BUMP_SEQUENCE_BAD_SEQ);
        return false;
    }
    return true;
}
}

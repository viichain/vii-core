#pragma once


#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnEntry.h"
#include "ledger/LedgerTxnHeader.h"
#include "transactions/OperationFrame.h"

namespace viichain
{
class BumpSequenceOpFrame : public OperationFrame
{
    ThresholdLevel getThresholdLevel() const override;
    bool isVersionSupported(uint32_t protocolVersion) const override;

    BumpSequenceResult&
    innerResult()
    {
        return mResult.tr().bumpSeqResult();
    }
    BumpSequenceOp const& mBumpSequenceOp;

  public:
    BumpSequenceOpFrame(Operation const& op, OperationResult& res,
                        TransactionFrame& parentTx);

    bool doApply(AbstractLedgerTxn& ltx) override;
    bool doCheckValid(uint32_t ledgerVersion) override;

    static BumpSequenceResultCode
    getInnerCode(OperationResult const& res)
    {
        return res.tr().bumpSeqResult().code();
    }
};
}

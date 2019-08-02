#pragma once


#include "transactions/OperationFrame.h"

namespace viichain
{
class AbstractLedgerTxn;

class SetOptionsOpFrame : public OperationFrame
{
    ThresholdLevel getThresholdLevel() const override;
    SetOptionsResult&
    innerResult()
    {
        return mResult.tr().setOptionsResult();
    }
    SetOptionsOp const& mSetOptions;

  public:
    SetOptionsOpFrame(Operation const& op, OperationResult& res,
                      TransactionFrame& parentTx);

    bool doApply(AbstractLedgerTxn& ltx) override;
    bool doCheckValid(uint32_t ledgerVersion) override;

    static SetOptionsResultCode
    getInnerCode(OperationResult const& res)
    {
        return res.tr().setOptionsResult().code();
    }
};
}

#pragma once

#include "transactions/OperationFrame.h"


namespace viichain
{
class ChangeTrustOpFrame : public OperationFrame
{
    ChangeTrustResult&
    innerResult()
    {
        return mResult.tr().changeTrustResult();
    }
    ChangeTrustOp const& mChangeTrust;

  public:
    ChangeTrustOpFrame(Operation const& op, OperationResult& res,
                       TransactionFrame& parentTx);

    bool doApply(AbstractLedgerTxn& ltx) override;
    bool doCheckValid(uint32_t ledgerVersion) override;

    static ChangeTrustResultCode
    getInnerCode(OperationResult const& res)
    {
        return res.tr().changeTrustResult().code();
    }
};
}

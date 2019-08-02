#pragma once


#include "transactions/OperationFrame.h"

namespace viichain
{
class AbstractLedgerTxn;

class InflationOpFrame : public OperationFrame
{
    InflationResult&
    innerResult()
    {
        return mResult.tr().inflationResult();
    }

    ThresholdLevel getThresholdLevel() const override;

  public:
    InflationOpFrame(Operation const& op, OperationResult& res,
                     TransactionFrame& parentTx);

    bool doApply(AbstractLedgerTxn& ltx) override;
    bool doCheckValid(uint32_t ledgerVersion) override;

    static InflationResultCode
    getInnerCode(OperationResult const& res)
    {
        return res.tr().inflationResult().code();
    }
};
}

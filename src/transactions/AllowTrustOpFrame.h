#pragma once


#include "transactions/OperationFrame.h"

namespace viichain
{
class AllowTrustOpFrame : public OperationFrame
{
    ThresholdLevel getThresholdLevel() const override;
    AllowTrustResult&
    innerResult() const
    {
        return getResult().tr().allowTrustResult();
    }

    AllowTrustOp const& mAllowTrust;

  public:
    AllowTrustOpFrame(Operation const& op, OperationResult& res,
                      TransactionFrame& parentTx);

    bool doApply(AbstractLedgerTxn& ls) override;
    bool doCheckValid(uint32_t ledgerVersion) override;

    static AllowTrustResultCode
    getInnerCode(OperationResult const& res)
    {
        return res.tr().allowTrustResult().code();
    }
};
}

#pragma once


#include "transactions/OperationFrame.h"

namespace viichain
{
class AbstractLedgerTxn;

class PathPaymentOpFrame : public OperationFrame
{
    PathPaymentResult&
    innerResult()
    {
        return mResult.tr().pathPaymentResult();
    }
    PathPaymentOp const& mPathPayment;

  public:
    PathPaymentOpFrame(Operation const& op, OperationResult& res,
                       TransactionFrame& parentTx);

    bool doApply(AbstractLedgerTxn& ltx) override;
    bool doCheckValid(uint32_t ledgerVersion) override;
    void insertLedgerKeysToPrefetch(
        std::unordered_set<LedgerKey>& keys) const override;

    static PathPaymentResultCode
    getInnerCode(OperationResult const& res)
    {
        return res.tr().pathPaymentResult().code();
    }
};
}

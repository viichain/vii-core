#pragma once


#include "transactions/OperationFrame.h"

namespace viichain
{
class AbstractLedgerTxn;

class PaymentOpFrame : public OperationFrame
{
    PaymentResult&
    innerResult()
    {
        return mResult.tr().paymentResult();
    }
    PaymentOp const& mPayment;

  public:
    PaymentOpFrame(Operation const& op, OperationResult& res,
                   TransactionFrame& parentTx);

    bool doApply(AbstractLedgerTxn& ltx) override;
    bool doCheckValid(uint32_t ledgerVersion) override;
    void insertLedgerKeysToPrefetch(
        std::unordered_set<LedgerKey>& keys) const override;

    static PaymentResultCode
    getInnerCode(OperationResult const& res)
    {
        return res.tr().paymentResult().code();
    }
};
}

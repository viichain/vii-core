#pragma once


#include "transactions/OperationFrame.h"

namespace viichain
{

class AbstractLedgerTxn;

class CreateAccountOpFrame : public OperationFrame
{
    CreateAccountResult&
    innerResult()
    {
        return mResult.tr().createAccountResult();
    }
    CreateAccountOp const& mCreateAccount;

  public:
    CreateAccountOpFrame(Operation const& op, OperationResult& res,
                         TransactionFrame& parentTx);

    bool doApply(AbstractLedgerTxn& ltx) override;
    bool doCheckValid(uint32_t ledgerVersion) override;
    void insertLedgerKeysToPrefetch(
        std::unordered_set<LedgerKey>& keys) const override;

    static CreateAccountResultCode
    getInnerCode(OperationResult const& res)
    {
        return res.tr().createAccountResult().code();
    }
};
}

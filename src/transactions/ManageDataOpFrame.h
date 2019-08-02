#pragma once


#include "transactions/OperationFrame.h"

namespace viichain
{
class AbstractLedgerTxn;

class ManageDataOpFrame : public OperationFrame
{

    ManageDataResult&
    innerResult()
    {
        return mResult.tr().manageDataResult();
    }

    ManageDataOp const& mManageData;

  public:
    ManageDataOpFrame(Operation const& op, OperationResult& res,
                      TransactionFrame& parentTx);

    bool doApply(AbstractLedgerTxn& ltx) override;
    bool doCheckValid(uint32_t ledgerVersion) override;
    void insertLedgerKeysToPrefetch(
        std::unordered_set<LedgerKey>& keys) const override;

    static ManageDataResultCode
    getInnerCode(OperationResult const& res)
    {
        return res.tr().manageDataResult().code();
    }
};
}

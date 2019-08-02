#pragma once


#include "ledger/LedgerHashUtils.h"
#include "ledger/LedgerManager.h"
#include "overlay/StellarXDR.h"
#include "util/types.h"
#include <memory>

namespace medida
{
class MetricsRegistry;
}

namespace viichain
{
class AbstractLedgerTxn;
class LedgerManager;
class LedgerTxnEntry;
class LedgerTxnHeader;

class SignatureChecker;
class TransactionFrame;

enum class ThresholdLevel
{
    LOW,
    MEDIUM,
    HIGH
};

class OperationFrame
{
  protected:
    Operation const& mOperation;
    TransactionFrame& mParentTx;
    OperationResult& mResult;

    virtual bool doCheckValid(uint32_t ledgerVersion) = 0;
    virtual bool doApply(AbstractLedgerTxn& ltx) = 0;

        virtual ThresholdLevel getThresholdLevel() const;

        virtual bool isVersionSupported(uint32_t protocolVersion) const;

    LedgerTxnEntry loadSourceAccount(AbstractLedgerTxn& ltx,
                                     LedgerTxnHeader const& header);

  public:
    static std::shared_ptr<OperationFrame>
    makeHelper(Operation const& op, OperationResult& res,
               TransactionFrame& parentTx);

    OperationFrame(Operation const& op, OperationResult& res,
                   TransactionFrame& parentTx);
    OperationFrame(OperationFrame const&) = delete;
    virtual ~OperationFrame() = default;

    bool checkSignature(SignatureChecker& signatureChecker,
                        AbstractLedgerTxn& ltx, bool forApply);

    AccountID const& getSourceID() const;

    OperationResult&
    getResult() const
    {
        return mResult;
    }
    OperationResultCode getResultCode() const;

    bool checkValid(SignatureChecker& signatureChecker,
                    AbstractLedgerTxn& ltxOuter, bool forApply);

    bool apply(SignatureChecker& signatureChecker, AbstractLedgerTxn& ltx);

    Operation const&
    getOperation() const
    {
        return mOperation;
    }

    virtual void
    insertLedgerKeysToPrefetch(std::unordered_set<LedgerKey>& keys) const;
};
}

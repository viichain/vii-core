
#include "util/asio.h"
#include "transactions/PaymentOpFrame.h"
#include "OfferExchange.h"
#include "database/Database.h"
#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnHeader.h"
#include "main/Application.h"
#include "transactions/PathPaymentOpFrame.h"
#include "transactions/TransactionUtils.h"
#include "util/Logging.h"
#include "util/XDROperators.h"
#include <algorithm>

namespace viichain
{

using namespace std;

PaymentOpFrame::PaymentOpFrame(Operation const& op, OperationResult& res,
                               TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx), mPayment(mOperation.body.paymentOp())
{
}

bool
PaymentOpFrame::doApply(AbstractLedgerTxn& ltx)
{
                auto ledgerVersion = ltx.loadHeader().current().ledgerVersion;
    auto instantSuccess = ledgerVersion > 2
                              ? mPayment.destination == getSourceID() &&
                                    mPayment.asset.type() == ASSET_TYPE_NATIVE
                              : mPayment.destination == getSourceID();
    if (instantSuccess)
    {
        innerResult().code(PAYMENT_SUCCESS);
        return true;
    }

        Operation op;
    op.sourceAccount = mOperation.sourceAccount;
    op.body.type(PATH_PAYMENT);
    PathPaymentOp& ppOp = op.body.pathPaymentOp();
    ppOp.sendAsset = mPayment.asset;
    ppOp.destAsset = mPayment.asset;

    ppOp.destAmount = mPayment.amount;
    ppOp.sendMax = mPayment.amount;

    ppOp.destination = mPayment.destination;

    OperationResult opRes;
    opRes.code(opINNER);
    opRes.tr().type(PATH_PAYMENT);
    PathPaymentOpFrame ppayment(op, opRes, mParentTx);

    if (!ppayment.doCheckValid(ledgerVersion) || !ppayment.doApply(ltx))
    {
        if (ppayment.getResultCode() != opINNER)
        {
            throw std::runtime_error("Unexpected error code from pathPayment");
        }
        PaymentResultCode res;

        switch (PathPaymentOpFrame::getInnerCode(ppayment.getResult()))
        {
        case PATH_PAYMENT_UNDERFUNDED:
            res = PAYMENT_UNDERFUNDED;
            break;
        case PATH_PAYMENT_SRC_NOT_AUTHORIZED:
            res = PAYMENT_SRC_NOT_AUTHORIZED;
            break;
        case PATH_PAYMENT_SRC_NO_TRUST:
            res = PAYMENT_SRC_NO_TRUST;
            break;
        case PATH_PAYMENT_NO_DESTINATION:
            res = PAYMENT_NO_DESTINATION;
            break;
        case PATH_PAYMENT_NO_TRUST:
            res = PAYMENT_NO_TRUST;
            break;
        case PATH_PAYMENT_NOT_AUTHORIZED:
            res = PAYMENT_NOT_AUTHORIZED;
            break;
        case PATH_PAYMENT_LINE_FULL:
            res = PAYMENT_LINE_FULL;
            break;
        case PATH_PAYMENT_NO_ISSUER:
            res = PAYMENT_NO_ISSUER;
            break;
        default:
            throw std::runtime_error("Unexpected error code from pathPayment");
        }
        innerResult().code(res);
        return false;
    }

    assert(PathPaymentOpFrame::getInnerCode(ppayment.getResult()) ==
           PATH_PAYMENT_SUCCESS);

    innerResult().code(PAYMENT_SUCCESS);

    return true;
}

bool
PaymentOpFrame::doCheckValid(uint32_t ledgerVersion)
{
    if (mPayment.amount <= 0)
    {
        innerResult().code(PAYMENT_MALFORMED);
        return false;
    }
    if (!isAssetValid(mPayment.asset))
    {
        innerResult().code(PAYMENT_MALFORMED);
        return false;
    }
    return true;
}

void
PaymentOpFrame::insertLedgerKeysToPrefetch(
    std::unordered_set<LedgerKey>& keys) const
{
    keys.emplace(accountKey(mPayment.destination));

        if (mPayment.asset.type() != ASSET_TYPE_NATIVE)
    {
        auto issuer = getIssuer(mPayment.asset);
        keys.emplace(accountKey(issuer));

                keys.emplace(trustlineKey(mPayment.destination, mPayment.asset));
        keys.emplace(trustlineKey(getSourceID(), mPayment.asset));
    }
}
}

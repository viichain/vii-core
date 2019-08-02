
#include "ChangeTrustOpFrame.h"
#include "database/Database.h"
#include "ledger/LedgerManager.h"
#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnEntry.h"
#include "ledger/LedgerTxnHeader.h"
#include "ledger/TrustLineWrapper.h"
#include "main/Application.h"
#include "transactions/TransactionUtils.h"

namespace viichain
{

ChangeTrustOpFrame::ChangeTrustOpFrame(Operation const& op,
                                       OperationResult& res,
                                       TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx)
    , mChangeTrust(mOperation.body.changeTrustOp())
{
}

bool
ChangeTrustOpFrame::doApply(AbstractLedgerTxn& ltx)
{
    auto header = ltx.loadHeader();
    auto issuerID = getIssuer(mChangeTrust.line);

    if (header.current().ledgerVersion > 2)
    {
                                        if (issuerID == getSourceID())
        {
                        innerResult().code(CHANGE_TRUST_SELF_NOT_ALLOWED);
            return false;
        }
    }
    else if (issuerID == getSourceID())
    {
        if (mChangeTrust.limit < INT64_MAX)
        {
            innerResult().code(CHANGE_TRUST_INVALID_LIMIT);
            return false;
        }
        else if (!viichain::loadAccountWithoutRecord(ltx, issuerID))
        {
            innerResult().code(CHANGE_TRUST_NO_ISSUER);
            return false;
        }
        return true;
    }

    LedgerKey key(TRUSTLINE);
    key.trustLine().accountID = getSourceID();
    key.trustLine().asset = mChangeTrust.line;

    auto trustLine = ltx.load(key);
    if (trustLine)
    { // we are modifying an old trustline
        if (mChangeTrust.limit < getMinimumLimit(header, trustLine))
        {
                        innerResult().code(CHANGE_TRUST_INVALID_LIMIT);
            return false;
        }

        if (mChangeTrust.limit == 0)
        {
                        trustLine.erase();
            auto sourceAccount = loadSourceAccount(ltx, header);
            addNumEntries(header, sourceAccount, -1);
        }
        else
        {
            auto issuer = viichain::loadAccountWithoutRecord(ltx, issuerID);
            if (!issuer)
            {
                innerResult().code(CHANGE_TRUST_NO_ISSUER);
                return false;
            }
            trustLine.current().data.trustLine().limit = mChangeTrust.limit;
        }
        innerResult().code(CHANGE_TRUST_SUCCESS);
        return true;
    }
    else
    { // new trust line
        if (mChangeTrust.limit == 0)
        {
            innerResult().code(CHANGE_TRUST_INVALID_LIMIT);
            return false;
        }
        auto issuer = viichain::loadAccountWithoutRecord(ltx, issuerID);
        if (!issuer)
        {
            innerResult().code(CHANGE_TRUST_NO_ISSUER);
            return false;
        }

        auto sourceAccount = loadSourceAccount(ltx, header);
        switch (addNumEntries(header, sourceAccount, 1))
        {
        case AddSubentryResult::SUCCESS:
            break;
        case AddSubentryResult::LOW_RESERVE:
            innerResult().code(CHANGE_TRUST_LOW_RESERVE);
            return false;
        case AddSubentryResult::TOO_MANY_SUBENTRIES:
            mResult.code(opTOO_MANY_SUBENTRIES);
            return false;
        default:
            throw std::runtime_error("Unexpected result from addNumEntries");
        }

        LedgerEntry trustLineEntry;
        trustLineEntry.data.type(TRUSTLINE);
        auto& tl = trustLineEntry.data.trustLine();
        tl.accountID = getSourceID();
        tl.asset = mChangeTrust.line;
        tl.limit = mChangeTrust.limit;
        tl.balance = 0;
        if (!isAuthRequired(issuer))
        {
            tl.flags = AUTHORIZED_FLAG;
        }
        ltx.create(trustLineEntry);

        innerResult().code(CHANGE_TRUST_SUCCESS);
        return true;
    }
}

bool
ChangeTrustOpFrame::doCheckValid(uint32_t ledgerVersion)
{
    if (mChangeTrust.limit < 0)
    {
        innerResult().code(CHANGE_TRUST_MALFORMED);
        return false;
    }
    if (!isAssetValid(mChangeTrust.line))
    {
        innerResult().code(CHANGE_TRUST_MALFORMED);
        return false;
    }
    if (ledgerVersion > 9)
    {
        if (mChangeTrust.line.type() == ASSET_TYPE_NATIVE)
        {
            innerResult().code(CHANGE_TRUST_MALFORMED);
            return false;
        }
    }
    return true;
}
}

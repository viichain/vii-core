
#include "transactions/CreatePassiveSellOfferOpFrame.h"

namespace viichain
{

ManageSellOfferOpHolder::ManageSellOfferOpHolder(Operation const& op)
{
    mCreateOp.body.type(MANAGE_SELL_OFFER);
    auto& manageOffer = mCreateOp.body.manageSellOfferOp();
    auto const& createPassiveOp = op.body.createPassiveSellOfferOp();
    manageOffer.amount = createPassiveOp.amount;
    manageOffer.buying = createPassiveOp.buying;
    manageOffer.selling = createPassiveOp.selling;
    manageOffer.offerID = 0;
    manageOffer.price = createPassiveOp.price;
    mCreateOp.sourceAccount = op.sourceAccount;
}

CreatePassiveSellOfferOpFrame::CreatePassiveSellOfferOpFrame(
    Operation const& op, OperationResult& res, TransactionFrame& parentTx)
    : ManageSellOfferOpHolder(op)
    , ManageSellOfferOpFrame(mCreateOp, res, parentTx, true)
{
}
}

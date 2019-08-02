#pragma once


#include "transactions/ManageSellOfferOpFrame.h"

namespace viichain
{
class ManageSellOfferOpHolder
{
  public:
    ManageSellOfferOpHolder(Operation const& op);
    Operation mCreateOp;
};

class CreatePassiveSellOfferOpFrame : public ManageSellOfferOpHolder,
                                      public ManageSellOfferOpFrame
{
  public:
    CreatePassiveSellOfferOpFrame(Operation const& op, OperationResult& res,
                                  TransactionFrame& parentTx);
};
}

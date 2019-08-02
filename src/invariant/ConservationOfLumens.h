#pragma once


#include "invariant/Invariant.h"
#include <memory>

namespace viichain
{

class Application;
struct LedgerTxnDelta;

class ConservationOfLumens : public Invariant
{
  public:
    ConservationOfLumens();

    static std::shared_ptr<Invariant> registerInvariant(Application& app);

    virtual std::string getName() const override;

    virtual std::string
    checkOnOperationApply(Operation const& operation,
                          OperationResult const& result,
                          LedgerTxnDelta const& ltxDelta) override;
};
}

#pragma once


#include "invariant/Invariant.h"
#include "xdr/vii-ledger-entries.h"
#include <memory>

namespace viichain
{
class Application;
struct LedgerTxnDelta;

class LedgerEntryIsValid : public Invariant
{
  public:
    LedgerEntryIsValid();

    static std::shared_ptr<Invariant> registerInvariant(Application& app);

    virtual std::string getName() const override;

    virtual std::string
    checkOnOperationApply(Operation const& operation,
                          OperationResult const& result,
                          LedgerTxnDelta const& ltxDelta) override;

  private:
    template <typename IterType>
    std::string check(IterType iter, IterType const& end, uint32_t ledgerSeq,
                      uint32 version) const;

    std::string checkIsValid(LedgerEntry const& le, uint32_t ledgerSeq,
                             uint32 version) const;
    std::string checkIsValid(AccountEntry const& ae, uint32 version) const;
    std::string checkIsValid(TrustLineEntry const& tl, uint32 version) const;
    std::string checkIsValid(OfferEntry const& oe, uint32 version) const;
    std::string checkIsValid(DataEntry const& de, uint32 version) const;
};
}

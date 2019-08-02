#pragma once


#include "invariant/Invariant.h"
#include <memory>
#include <unordered_map>

namespace viichain
{

class Application;
class Database;
struct LedgerTxnDelta;

struct SubEntriesChange
{
    int32_t numSubEntries;
    int32_t calculatedSubEntries;
    int32_t signers;

    SubEntriesChange() : numSubEntries(0), calculatedSubEntries(0), signers(0)
    {
    }
};

class AccountSubEntriesCountIsValid : public Invariant
{
  public:
    AccountSubEntriesCountIsValid();

    static std::shared_ptr<Invariant> registerInvariant(Application& app);

    virtual std::string getName() const override;

    virtual std::string
    checkOnOperationApply(Operation const& operation,
                          OperationResult const& result,
                          LedgerTxnDelta const& ltxDelta) override;
};
}

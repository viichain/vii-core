#pragma once


#include "herder/TxSetFrame.h"
#include "lib/json/json.h"
#include <memory>

namespace viichain
{

class Application;
class Bucket;
class Invariant;
struct LedgerTxnDelta;
struct Operation;

class InvariantManager
{
  public:
    static std::unique_ptr<InvariantManager> create(Application& app);

    virtual ~InvariantManager()
    {
    }

    virtual Json::Value getJsonInfo() = 0;
    virtual std::vector<std::string> getEnabledInvariants() const = 0;

    virtual void checkOnBucketApply(std::shared_ptr<Bucket const> bucket,
                                    uint32_t ledger, uint32_t level,
                                    bool isCurr) = 0;

    virtual void checkOnOperationApply(Operation const& operation,
                                       OperationResult const& opres,
                                       LedgerTxnDelta const& ltxDelta) = 0;

    virtual void registerInvariant(std::shared_ptr<Invariant> invariant) = 0;

    virtual void enableInvariant(std::string const& name) = 0;

    template <typename T, typename... Args>
    std::shared_ptr<T>
    registerInvariant(Args&&... args)
    {
        auto invariant = std::make_shared<T>(std::forward<Args>(args)...);
        registerInvariant(invariant);
        return invariant;
    }
};
}

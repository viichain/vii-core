#pragma once


#include <memory>
#include <string>

namespace viichain
{

class Bucket;
struct LedgerTxnDelta;
struct Operation;
struct OperationResult;

class Invariant
{
    bool const mStrict;

  public:
    explicit Invariant(bool strict) : mStrict(strict)
    {
    }

    virtual ~Invariant()
    {
    }

    virtual std::string getName() const = 0;

    bool
    isStrict() const
    {
        return mStrict;
    }

    virtual std::string
    checkOnBucketApply(std::shared_ptr<Bucket const> bucket,
                       uint32_t oldestLedger, uint32_t newestLedger)
    {
        return std::string{};
    }

    virtual std::string
    checkOnOperationApply(Operation const& operation,
                          OperationResult const& result,
                          LedgerTxnDelta const& ltxDelta)
    {
        return std::string{};
    }
};
}

#pragma once


#include "invariant/Invariant.h"

namespace viichain
{

class Application;

class BucketListIsConsistentWithDatabase : public Invariant
{
  public:
    static std::shared_ptr<Invariant> registerInvariant(Application& app);

    explicit BucketListIsConsistentWithDatabase(Application& app);

    virtual std::string getName() const override;

    virtual std::string checkOnBucketApply(std::shared_ptr<Bucket const> bucket,
                                           uint32_t oldestLedger,
                                           uint32_t newestLedger) override;

  private:
    Application& mApp;
};
}

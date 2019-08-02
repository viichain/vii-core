#pragma once


#include "work/BasicWork.h"

namespace viichain
{

class WorkSequence : public BasicWork
{
    std::vector<std::shared_ptr<BasicWork>> mSequenceOfWork;
    std::vector<std::shared_ptr<BasicWork>>::const_iterator mNextInSequence;
    std::shared_ptr<BasicWork> mCurrentExecuting;

  public:
    WorkSequence(Application& app, std::string name,
                 std::vector<std::shared_ptr<BasicWork>> sequence,
                 size_t maxRetries = RETRY_A_FEW);
    ~WorkSequence() = default;

    std::string getStatus() const override;
    void shutdown() override;

  protected:
    State onRun() final;
    bool onAbort() final;
    void onReset() final;
};
}

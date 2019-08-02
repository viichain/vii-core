
#pragma once

#include "history/HistoryArchive.h"
#include "work/WorkSequence.h"

namespace viichain
{

struct StateSnapshot;

class PublishWork : public WorkSequence
{
    std::shared_ptr<StateSnapshot> mSnapshot;
    std::vector<std::string> mOriginalBuckets;

  public:
    PublishWork(Application& app, std::shared_ptr<StateSnapshot> snapshot,
                std::vector<std::shared_ptr<BasicWork>> seq);
    ~PublishWork() = default;

  protected:
    void onFailureRaise() override;
    void onSuccess() override;
};
}


#pragma once

#include "work/Work.h"
#include "work/WorkSequence.h"

namespace viichain
{

class HistoryArchive;
struct HistoryArchiveState;

class PutHistoryArchiveStateWork : public Work
{
    HistoryArchiveState const& mState;
    std::shared_ptr<HistoryArchive> mArchive;
    std::string mLocalFilename;
    std::shared_ptr<WorkSequence> mPutRemoteFileWork;

    void spawnPublishWork();

  public:
    PutHistoryArchiveStateWork(Application& app,
                               HistoryArchiveState const& state,
                               std::shared_ptr<HistoryArchive> archive);
    ~PutHistoryArchiveStateWork() = default;

  protected:
    void doReset() override;
    State doWork() override;
};
}

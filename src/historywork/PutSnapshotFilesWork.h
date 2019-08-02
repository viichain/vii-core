
#pragma once

#include "history/FileTransferInfo.h"
#include "history/HistoryArchive.h"
#include "work/Work.h"

namespace viichain
{

struct StateSnapshot;

class PutSnapshotFilesWork : public Work
{
    std::shared_ptr<StateSnapshot> mSnapshot;
    HistoryArchiveState mRemoteState;
    bool mStarted{false};

  public:
    PutSnapshotFilesWork(Application& app,
                         std::shared_ptr<StateSnapshot> snapshot);
    ~PutSnapshotFilesWork() = default;

  protected:
    State doWork() override;
    void doReset() override;
};
}

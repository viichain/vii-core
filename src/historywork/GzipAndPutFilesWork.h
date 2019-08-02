
#pragma once

#include "history/FileTransferInfo.h"
#include "history/HistoryArchive.h"
#include "history/StateSnapshot.h"
#include "work/Work.h"

namespace viichain
{
class GzipAndPutFilesWork : public Work
{
    std::shared_ptr<HistoryArchive> mArchive;
    std::shared_ptr<StateSnapshot> mSnapshot;
    HistoryArchiveState const& mRemoteState;

    bool mChildrenSpawned{false};

  public:
    GzipAndPutFilesWork(Application& app,
                        std::shared_ptr<HistoryArchive> archive,
                        std::shared_ptr<StateSnapshot> snapshot,
                        HistoryArchiveState const& remoteState);
    ~GzipAndPutFilesWork() = default;

  protected:
    void doReset() override;
    State doWork() override;
};
}

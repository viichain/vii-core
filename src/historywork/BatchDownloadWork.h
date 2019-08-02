
#pragma once

#include "history/FileTransferInfo.h"
#include "ledger/CheckpointRange.h"
#include "work/BatchWork.h"

namespace medida
{
class Meter;
}

namespace viichain
{

class BatchDownloadWork : public BatchWork
{
    CheckpointRange const mRange;
    uint32_t mNext;
    std::string const mFileType;
    TmpDir const& mDownloadDir;

  public:
    BatchDownloadWork(Application& app, CheckpointRange range,
                      std::string const& type, TmpDir const& downloadDir);
    ~BatchDownloadWork() = default;
    std::string getStatus() const override;

  protected:
    bool hasNext() const override;
    std::shared_ptr<BasicWork> yieldMoreWork() override;
    void resetIter() override;
};
}


#pragma once

#include "history/FileTransferInfo.h"
#include "work/Work.h"
#include <medida/meter.h>
#include <medida/metrics_registry.h>

namespace viichain
{

class HistoryArchive;

class GetAndUnzipRemoteFileWork : public Work
{
    std::shared_ptr<BasicWork> mGetRemoteFileWork;
    std::shared_ptr<BasicWork> mGunzipFileWork;

    FileTransferInfo mFt;
    std::shared_ptr<HistoryArchive> mArchive;

    medida::Meter& mDownloadStart;
    medida::Meter& mDownloadSuccess;
    medida::Meter& mDownloadFailure;

    bool validateFile();

  public:
                GetAndUnzipRemoteFileWork(Application& app, FileTransferInfo ft,
                              std::shared_ptr<HistoryArchive> archive = nullptr,
                              size_t maxRetries = BasicWork::RETRY_A_LOT);
    ~GetAndUnzipRemoteFileWork() = default;
    std::string getStatus() const override;

  protected:
    void doReset() override;
    void onFailureRaise() override;
    void onSuccess() override;
    State doWork() override;
};
}

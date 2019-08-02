
#pragma once

#include "historywork/RunCommandWork.h"

namespace viichain
{

class HistoryArchive;

class GetRemoteFileWork : public RunCommandWork
{
    std::string const mRemote;
    std::string const mLocal;
    std::shared_ptr<HistoryArchive> mArchive;
    std::shared_ptr<HistoryArchive> mCurrentArchive;
    CommandInfo getCommand() override;

  public:
                GetRemoteFileWork(Application& app, std::string const& remote,
                      std::string const& local,
                      std::shared_ptr<HistoryArchive> archive = nullptr,
                      size_t maxRetries = BasicWork::RETRY_A_LOT);
    ~GetRemoteFileWork() = default;

  protected:
    void onReset() override;
    void onSuccess() override;
    void onFailureRaise() override;
};
}

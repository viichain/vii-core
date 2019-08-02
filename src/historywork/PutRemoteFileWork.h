
#pragma once

#include "historywork/RunCommandWork.h"

namespace viichain
{

class HistoryArchive;

class PutRemoteFileWork : public RunCommandWork
{
    std::string const mLocal;
    std::string const mRemote;
    std::shared_ptr<HistoryArchive> mArchive;
    CommandInfo getCommand() override;

  public:
    PutRemoteFileWork(Application& app, std::string const& local,
                      std::string const& remote,
                      std::shared_ptr<HistoryArchive> archive);
    ~PutRemoteFileWork() = default;

  protected:
    void onSuccess() override;
    void onFailureRaise() override;
};
}

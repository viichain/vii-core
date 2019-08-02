
#pragma once

#include "historywork/RunCommandWork.h"

namespace viichain
{

class HistoryArchive;

class MakeRemoteDirWork : public RunCommandWork
{
    std::string const mDir;
    std::shared_ptr<HistoryArchive> mArchive;
    CommandInfo getCommand() override;

  public:
    MakeRemoteDirWork(Application& app, std::string const& dir,
                      std::shared_ptr<HistoryArchive> archive);
    ~MakeRemoteDirWork() = default;

    void onSuccess() override;
    void onFailureRaise() override;
};
}

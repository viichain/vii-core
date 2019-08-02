#pragma once


#include "process/ProcessManager.h"
#include "work/Work.h"

namespace viichain
{
struct CommandInfo
{
    std::string mCommand;
    std::string mOutFile;
};

class RunCommandWork : public BasicWork
{
    bool mDone{false};
    asio::error_code mEc;
    virtual CommandInfo getCommand() = 0;
    std::weak_ptr<ProcessExitEvent> mExitEvent;

  public:
    RunCommandWork(Application& app, std::string const& name,
                   size_t maxRetries = BasicWork::RETRY_A_FEW);
    ~RunCommandWork() = default;

  protected:
    void onReset() override;
    BasicWork::State onRun() override;
    bool onAbort() override;
};
}

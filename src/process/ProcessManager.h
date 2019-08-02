#pragma once


#include "util/NonCopyable.h"
#include "util/Timer.h"
#include <functional>
#include <memory>
#include <string>

namespace viichain
{

class RealTimer;

class ProcessExitEvent
{
    class Impl;
    std::shared_ptr<RealTimer> mTimer;
    std::shared_ptr<Impl> mImpl;
    std::shared_ptr<asio::error_code> mEc;
    ProcessExitEvent(asio::io_context& io_context);
    friend class ProcessManagerImpl;

  public:
    ~ProcessExitEvent();
    void async_wait(std::function<void(asio::error_code)> const& handler);
};

class ProcessManager : public std::enable_shared_from_this<ProcessManager>,
                       public NonMovableOrCopyable
{
  public:
    static std::shared_ptr<ProcessManager> create(Application& app);
    virtual std::weak_ptr<ProcessExitEvent>
    runProcess(std::string const& cmdLine, std::string outputFile) = 0;
    virtual size_t getNumRunningProcesses() = 0;
    virtual bool isShutdown() const = 0;
    virtual void shutdown() = 0;

            virtual bool tryProcessShutdown(std::shared_ptr<ProcessExitEvent> pe) = 0;
    virtual ~ProcessManager()
    {
    }
};
}

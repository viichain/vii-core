#pragma once


#include "process/ProcessManager.h"
#include <atomic>
#include <deque>
#include <mutex>
#include <vector>

namespace viichain
{

class ProcessManagerImpl : public ProcessManager
{
            static std::atomic<size_t> gNumProcessesActive;

            std::recursive_mutex mProcessesMutex;
    std::map<int, std::shared_ptr<ProcessExitEvent>> mProcesses;

    bool mIsShutdown{false};
    size_t mMaxProcesses;
    asio::io_context& mIOContext;

    std::deque<std::shared_ptr<ProcessExitEvent>> mPending;
    std::deque<std::shared_ptr<ProcessExitEvent>> mKillable;
    void maybeRunPendingProcesses();

        asio::signal_set mSigChild;
    void startSignalWait();
    void handleSignalWait();
    void handleProcessTermination(int pid, int status);
    bool cleanShutdown(ProcessExitEvent& pe);
    bool forceShutdown(ProcessExitEvent& pe);

    friend class ProcessExitEvent::Impl;

  public:
    explicit ProcessManagerImpl(Application& app);
    std::weak_ptr<ProcessExitEvent> runProcess(std::string const& cmdLine,
                                               std::string outFile) override;
    size_t getNumRunningProcesses() override;

    bool isShutdown() const override;
    void shutdown() override;
    bool tryProcessShutdown(std::shared_ptr<ProcessExitEvent> pe) override;

    ~ProcessManagerImpl() override;
};
}

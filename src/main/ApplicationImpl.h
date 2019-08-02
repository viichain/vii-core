#pragma once


#include "Application.h"
#include "main/Config.h"
#include "main/PersistentState.h"
#include "medida/timer_context.h"
#include "util/MetricResetter.h"
#include "util/Timer.h"
#include <thread>

namespace medida
{
class Counter;
class Timer;
}

namespace viichain
{
class TmpDirManager;
class LedgerManager;
class Herder;
class HerderPersistence;
class BucketManager;
class HistoryManager;
class ProcessManager;
class CommandHandler;
class Database;
class LedgerTxnRoot;
class LoadGenerator;

class ApplicationImpl : public Application
{
  public:
    ApplicationImpl(VirtualClock& clock, Config const& cfg);
    virtual ~ApplicationImpl() override;

    virtual void initialize(bool newDB) override;

    virtual uint64_t timeNow() override;

    virtual Config const& getConfig() override;

    virtual State getState() const override;
    virtual std::string getStateHuman() const override;
    virtual bool isStopping() const override;
    virtual VirtualClock& getClock() override;
    virtual medida::MetricsRegistry& getMetrics() override;
    virtual void syncOwnMetrics() override;
    virtual void syncAllMetrics() override;
    virtual void clearMetrics(std::string const& domain) override;
    virtual TmpDirManager& getTmpDirManager() override;
    virtual LedgerManager& getLedgerManager() override;
    virtual BucketManager& getBucketManager() override;
    virtual CatchupManager& getCatchupManager() override;
    virtual HistoryArchiveManager& getHistoryArchiveManager() override;
    virtual HistoryManager& getHistoryManager() override;
    virtual Maintainer& getMaintainer() override;
    virtual ProcessManager& getProcessManager() override;
    virtual Herder& getHerder() override;
    virtual HerderPersistence& getHerderPersistence() override;
    virtual InvariantManager& getInvariantManager() override;
    virtual OverlayManager& getOverlayManager() override;
    virtual Database& getDatabase() const override;
    virtual PersistentState& getPersistentState() override;
    virtual CommandHandler& getCommandHandler() override;
    virtual WorkScheduler& getWorkScheduler() override;
    virtual BanManager& getBanManager() override;
    virtual StatusManager& getStatusManager() override;

    virtual asio::io_context& getWorkerIOContext() override;
    virtual void postOnMainThread(std::function<void()>&& f,
                                  std::string jobName) override;
    virtual void postOnMainThreadWithDelay(std::function<void()>&& f,
                                           std::string jobName) override;
    virtual void postOnBackgroundThread(std::function<void()>&& f,
                                        std::string jobName) override;

    virtual void start() override;

                virtual void gracefulStop() override;

                    virtual void joinAllThreads() override;

    virtual bool manualClose() override;

#ifdef BUILD_TESTS
    virtual void generateLoad(bool isCreate, uint32_t nAccounts,
                              uint32_t offset, uint32_t nTxs, uint32_t txRate,
                              uint32_t batchSize) override;

    virtual LoadGenerator& getLoadGenerator() override;
#endif

    virtual void applyCfgCommands() override;

    virtual void reportCfgMetrics() override;

    virtual Json::Value getJsonInfo() override;

    virtual void reportInfo() override;

    virtual Hash const& getNetworkID() const override;

    virtual LedgerTxnRoot& getLedgerTxnRoot() override;

  protected:
    std::unique_ptr<LedgerManager>
        mLedgerManager;              // allow to change that for tests
    std::unique_ptr<Herder> mHerder; // allow to change that for tests

  private:
    VirtualClock& mVirtualClock;
    Config mConfig;

                                        
    asio::io_context mWorkerIOContext;
    std::unique_ptr<asio::io_context::work> mWork;

    std::unique_ptr<Database> mDatabase;
    std::unique_ptr<OverlayManager> mOverlayManager;
    std::unique_ptr<BucketManager> mBucketManager;
    std::unique_ptr<CatchupManager> mCatchupManager;
    std::unique_ptr<HerderPersistence> mHerderPersistence;
    std::unique_ptr<HistoryArchiveManager> mHistoryArchiveManager;
    std::unique_ptr<HistoryManager> mHistoryManager;
    std::unique_ptr<InvariantManager> mInvariantManager;
    std::unique_ptr<Maintainer> mMaintainer;
    std::shared_ptr<ProcessManager> mProcessManager;
    std::unique_ptr<CommandHandler> mCommandHandler;
    std::shared_ptr<WorkScheduler> mWorkScheduler;
    std::unique_ptr<PersistentState> mPersistentState;
    std::unique_ptr<BanManager> mBanManager;
    std::unique_ptr<StatusManager> mStatusManager;
    std::unique_ptr<LedgerTxnRoot> mLedgerTxnRoot;

#ifdef BUILD_TESTS
    std::unique_ptr<LoadGenerator> mLoadGenerator;
#endif

    std::vector<std::thread> mWorkerThreads;

    asio::signal_set mStopSignals;

    bool mStarted;
    bool mStopping;

    VirtualTimer mStoppingTimer;

    std::unique_ptr<medida::MetricsRegistry> mMetrics;
    medida::Counter& mAppStateCurrent;
    medida::Timer& mPostOnMainThreadDelay;
    medida::Timer& mPostOnMainThreadWithDelayDelay;
    medida::Timer& mPostOnBackgroundThreadDelay;
    VirtualClock::time_point mStartedOn;

    Hash mNetworkID;

    void newDB();
    void upgradeDB();

    void shutdownMainIOContext();
    void shutdownWorkScheduler();

    void enableInvariantsFromConfig();

    virtual std::unique_ptr<Herder> createHerder();
    virtual std::unique_ptr<InvariantManager> createInvariantManager();
    virtual std::unique_ptr<OverlayManager> createOverlayManager();
    virtual std::unique_ptr<LedgerManager> createLedgerManager();
};
}

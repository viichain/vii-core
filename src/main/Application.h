#pragma once


#include "main/Config.h"
#include "xdr/vii-types.h"
#include <lib/json/json.h>
#include <memory>
#include <string>

namespace asio
{
}
namespace medida
{
class MetricsRegistry;
}

namespace viichain
{

class VirtualClock;
class TmpDirManager;
class LedgerManager;
class BucketManager;
class CatchupManager;
class HistoryArchiveManager;
class HistoryManager;
class Maintainer;
class ProcessManager;
class Herder;
class HerderPersistence;
class InvariantManager;
class OverlayManager;
class Database;
class PersistentState;
class CommandHandler;
class WorkScheduler;
class BanManager;
class StatusManager;
class LedgerTxnRoot;

#ifdef BUILD_TESTS
class LoadGenerator;
#endif

class Application;
void validateNetworkPassphrase(std::shared_ptr<Application> app);

class Application
{
  public:
    typedef std::shared_ptr<Application> pointer;

            enum State
    {
                APP_CREATED_STATE,

                APP_ACQUIRING_CONSENSUS_STATE,

        
                APP_CONNECTED_STANDBY_STATE,

                                APP_CATCHING_UP_STATE,

                APP_SYNCED_STATE,

                APP_STOPPING_STATE,

        APP_NUM_STATE
    };

    virtual ~Application(){};

    virtual void initialize(bool createNewDB) = 0;

            virtual uint64_t timeNow() = 0;

            virtual Config const& getConfig() = 0;

            virtual State getState() const = 0;
    virtual std::string getStateHuman() const = 0;
    virtual bool isStopping() const = 0;

        virtual VirtualClock& getClock() = 0;

            virtual medida::MetricsRegistry& getMetrics() = 0;

            virtual void syncOwnMetrics() = 0;

        virtual void syncAllMetrics() = 0;

        virtual void clearMetrics(std::string const& domain) = 0;

        virtual TmpDirManager& getTmpDirManager() = 0;
    virtual LedgerManager& getLedgerManager() = 0;
    virtual BucketManager& getBucketManager() = 0;
    virtual CatchupManager& getCatchupManager() = 0;
    virtual HistoryArchiveManager& getHistoryArchiveManager() = 0;
    virtual HistoryManager& getHistoryManager() = 0;
    virtual Maintainer& getMaintainer() = 0;
    virtual ProcessManager& getProcessManager() = 0;
    virtual Herder& getHerder() = 0;
    virtual HerderPersistence& getHerderPersistence() = 0;
    virtual InvariantManager& getInvariantManager() = 0;
    virtual OverlayManager& getOverlayManager() = 0;
    virtual Database& getDatabase() const = 0;
    virtual PersistentState& getPersistentState() = 0;
    virtual CommandHandler& getCommandHandler() = 0;
    virtual WorkScheduler& getWorkScheduler() = 0;
    virtual BanManager& getBanManager() = 0;
    virtual StatusManager& getStatusManager() = 0;

                virtual asio::io_context& getWorkerIOContext() = 0;

    virtual void postOnMainThread(std::function<void()>&& f,
                                  std::string jobName) = 0;
    virtual void postOnMainThreadWithDelay(std::function<void()>&& f,
                                           std::string jobName) = 0;
    virtual void postOnBackgroundThread(std::function<void()>&& f,
                                        std::string jobName) = 0;

                    virtual void start() = 0;

            virtual void gracefulStop() = 0;

                    virtual void joinAllThreads() = 0;

            virtual bool manualClose() = 0;

#ifdef BUILD_TESTS
            virtual void generateLoad(bool isCreate, uint32_t nAccounts,
                              uint32_t offset, uint32_t nTxs, uint32_t txRate,
                              uint32_t batchSize) = 0;

        virtual LoadGenerator& getLoadGenerator() = 0;
#endif

                virtual void applyCfgCommands() = 0;

            virtual void reportCfgMetrics() = 0;

        virtual Json::Value getJsonInfo() = 0;

        virtual void reportInfo() = 0;

            virtual Hash const& getNetworkID() const = 0;

    virtual LedgerTxnRoot& getLedgerTxnRoot() = 0;

            static pointer create(VirtualClock& clock, Config const& cfg,
                          bool newDB = true);
    template <typename T>
    static std::shared_ptr<T>
    create(VirtualClock& clock, Config const& cfg, bool newDB = true)
    {
        auto ret = std::make_shared<T>(clock, cfg);
        ret->initialize(newDB);
        validateNetworkPassphrase(ret);

        return ret;
    }

  protected:
    Application()
    {
    }
};
}

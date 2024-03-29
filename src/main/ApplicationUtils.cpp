
#include "main/ApplicationUtils.h"
#include "bucket/Bucket.h"
#include "catchup/CatchupConfiguration.h"
#include "history/HistoryArchive.h"
#include "history/HistoryArchiveManager.h"
#include "historywork/GetHistoryArchiveStateWork.h"
#include "ledger/LedgerManager.h"
#include "main/ErrorMessages.h"
#include "main/ExternalQueue.h"
#include "main/Maintainer.h"
#include "main/PersistentState.h"
#include "main/VIICoreVersion.h"
#include "util/Logging.h"
#include "work/WorkScheduler.h"

#include <lib/http/HttpClient.h>
#include <locale>

namespace viichain
{

int
runWithConfig(Config cfg)
{
    if (cfg.MANUAL_CLOSE)
    {
        if (!cfg.NODE_IS_VALIDATOR)
        {
            LOG(ERROR) << "Starting vii-core in MANUAL_CLOSE mode requires "
                          "NODE_IS_VALIDATOR to be set";
            return 1;
        }

                                cfg.FORCE_SCP = true;
    }

    LOG(INFO) << "Starting vii-core " << VII_CORE_VERSION;
    VirtualClock clock(VirtualClock::REAL_TIME);
    Application::pointer app;
    try
    {
        app = Application::create(clock, cfg, false);

        if (!app->getHistoryArchiveManager().checkSensibleConfig())
        {
            return 1;
        }
        if (cfg.ARTIFICIALLY_ACCELERATE_TIME_FOR_TESTING)
        {
            LOG(WARNING) << "Artificial acceleration of time enabled "
                         << "(for testing only)";
        }

        app->start();

        app->applyCfgCommands();
    }
    catch (std::exception const& e)
    {
        LOG(FATAL) << "Got an exception: " << e.what();
        LOG(FATAL) << REPORT_INTERNAL_BUG;
        return 1;
    }

    try
    {
        auto& io = clock.getIOContext();
        asio::io_context::work mainWork(io);
        while (!io.stopped())
        {
            clock.crank();
        }
    }
    catch (std::exception const& e)
    {
        LOG(FATAL) << "Got an exception: " << e.what();
        throw; // propagate exception (core dump, etc)
    }
    return 0;
}

void
httpCommand(std::string const& command, unsigned short port)
{
    std::string ret;
    std::ostringstream path;

    path << "/";
    bool gotCommand = false;

    std::locale loc("C");

    for (auto const& c : command)
    {
        if (gotCommand)
        {
            if (std::isalnum(c, loc))
            {
                path << c;
            }
            else
            {
                path << '%' << std::hex << std::setw(2) << std::setfill('0')
                     << (unsigned int)c;
            }
        }
        else
        {
            path << c;
            if (c == '?')
            {
                gotCommand = true;
            }
        }
    }

    int code = http_request("127.0.0.1", path.str(), port, ret);
    if (code == 200)
    {
        LOG(INFO) << ret;
    }
    else
    {
        LOG(INFO) << "http failed(" << code << ") port: " << port
                  << " command: " << command;
    }
}

void
loadXdr(Config cfg, std::string const& bucketFile)
{
    VirtualClock clock;
    cfg.setNoListen();
    Application::pointer app = Application::create(clock, cfg, false);

    uint256 zero;
    Bucket bucket(bucketFile, zero);
    bucket.apply(*app);
}

void
setForceSCPFlag(Config cfg, bool set)
{
    VirtualClock clock;
    cfg.setNoListen();
    Application::pointer app = Application::create(clock, cfg, false);

    app->getPersistentState().setState(PersistentState::kForceSCPOnNextLaunch,
                                       (set ? "true" : "false"));
    if (set)
    {
        LOG(INFO) << "* ";
        LOG(INFO) << "* The `force scp` flag has been set in the db.";
        LOG(INFO) << "* ";
        LOG(INFO)
            << "* The next launch will start scp from the account balances";
        LOG(INFO) << "* as they stand in the db now, without waiting to "
                     "hear from";
        LOG(INFO) << "* the network.";
        LOG(INFO) << "* ";
    }
    else
    {
        LOG(INFO) << "* ";
        LOG(INFO) << "* The `force scp` flag has been cleared.";
        LOG(INFO) << "* The next launch will start normally.";
        LOG(INFO) << "* ";
    }
}

void
initializeDatabase(Config cfg)
{
    VirtualClock clock;
    cfg.setNoListen();
    Application::pointer app = Application::create(clock, cfg);

    LOG(INFO) << "*";
    LOG(INFO) << "* The next launch will catchup from the network afresh.";
    LOG(INFO) << "*";
}

void
showOfflineInfo(Config cfg)
{
        VirtualClock clock(VirtualClock::REAL_TIME);
    cfg.setNoListen();
    Application::pointer app = Application::create(clock, cfg, false);
    app->reportInfo();
}

int
reportLastHistoryCheckpoint(Config cfg, std::string const& outputFile)
{
    VirtualClock clock(VirtualClock::REAL_TIME);
    cfg.setNoListen();
    Application::pointer app = Application::create(clock, cfg, false);

    auto state = HistoryArchiveState{};
    auto& wm = app->getWorkScheduler();
    auto getHistoryArchiveStateWork =
        wm.executeWork<GetHistoryArchiveStateWork>(state);

    auto ok = getHistoryArchiveStateWork->getState() ==
              BasicWork::State::WORK_SUCCESS;
    if (ok)
    {
        std::string filename = outputFile.empty() ? "-" : outputFile;

        if (filename == "-")
        {
            LOG(INFO) << "*";
            LOG(INFO) << "* Last history checkpoint " << state.toString();
            LOG(INFO) << "*";
        }
        else
        {
            state.save(filename);
            LOG(INFO) << "*";
            LOG(INFO) << "* Wrote last history checkpoint " << filename;
            LOG(INFO) << "*";
        }
    }
    else
    {
        LOG(INFO) << "*";
        LOG(INFO) << "* Fetching last history checkpoint failed.";
        LOG(INFO) << "*";
    }

    app->gracefulStop();
    while (clock.crank(true))
        ;

    return ok ? 0 : 1;
}

void
genSeed()
{
    auto key = SecretKey::random();
    std::cout << "Secret seed: " << key.getStrKeySeed().value << std::endl;
    std::cout << "Public: " << key.getStrKeyPublic() << std::endl;
}

int
initializeHistories(Config cfg, std::vector<std::string> const& newHistories)
{
    VirtualClock clock;
    cfg.setNoListen();
    Application::pointer app = Application::create(clock, cfg, false);

    for (auto const& arch : newHistories)
    {
        if (!app->getHistoryArchiveManager().initializeHistoryArchive(arch))
            return 1;
    }
    return 0;
}

void
writeCatchupInfo(Json::Value const& catchupInfo, std::string const& outputFile)
{
    std::string filename = outputFile.empty() ? "-" : outputFile;
    auto content = catchupInfo.toStyledString();

    if (filename == "-")
    {
        LOG(INFO) << "*";
        LOG(INFO) << "* Catchup info: " << content;
        LOG(INFO) << "*";
    }
    else
    {
        std::ofstream out{};
        out.open(filename);
        out.write(content.c_str(), content.size());
        out.close();

        LOG(INFO) << "*";
        LOG(INFO) << "* Wrote catchup info to " << filename;
        LOG(INFO) << "*";
    }
}

int
catchup(Application::pointer app, CatchupConfiguration cc,
        Json::Value& catchupInfo)
{
    app->start();

    try
    {
        app->getLedgerManager().startCatchup(cc);
    }
    catch (std::invalid_argument const&)
    {
        LOG(INFO) << "*";
        LOG(INFO) << "* Target ledger " << cc.toLedger()
                  << " is not newer than last closed ledger"
                  << " - nothing to do";
        LOG(INFO) << "* If you really want to catchup to " << cc.toLedger()
                  << " run vii-core new-db";
        LOG(INFO) << "*";
        return 2;
    }

    auto& clock = app->getClock();
    auto& io = clock.getIOContext();
    auto synced = false;
    asio::io_context::work mainWork(io);
    auto done = false;
    while (!done && clock.crank(true))
    {
        switch (app->getLedgerManager().getState())
        {
        case LedgerManager::LM_BOOTING_STATE:
        {
            done = true;
            break;
        }
        case LedgerManager::LM_SYNCED_STATE:
        {
            done = true;
            synced = true;
            break;
        }
        case LedgerManager::LM_CATCHING_UP_STATE:
        {
            if (app->getLedgerManager().getCatchupState() ==
                LedgerManager::CatchupState::NONE)
            {
                abort();
            }
            break;
        }
        case LedgerManager::LM_NUM_STATE:
            abort();
        }
    }

    LOG(INFO) << "*";
    if (synced)
    {
        LOG(INFO) << "* Catchup finished.";
    }
    else
    {
        LOG(INFO) << "* Catchup failed.";
    }
    LOG(INFO) << "*";

    catchupInfo = app->getJsonInfo();
    return synced ? 0 : 3;
}

int
publish(Application::pointer app)
{
    app->start();

    auto& clock = app->getClock();
    auto& io = clock.getIOContext();
    asio::io_context::work mainWork(io);

    auto lcl = app->getLedgerManager().getLastClosedLedgerNum();
    auto isCheckpoint =
        lcl == app->getHistoryManager().checkpointContainingLedger(lcl);
    auto expectedPublishQueueSize = isCheckpoint ? 1 : 0;

    app->getHistoryManager().publishQueuedHistory();
    while (app->getHistoryManager().publishQueueLength() !=
               expectedPublishQueueSize &&
           clock.crank(true))
    {
    }

    LOG(INFO) << "*";
    LOG(INFO) << "* Publish finished.";
    LOG(INFO) << "*";

    return 0;
}
}

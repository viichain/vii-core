
#define CATCH_CONFIG_RUNNER

#include "util/asio.h"

#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnHeader.h"
#include "main/Config.h"
#include "main/VIICoreVersion.h"
#include "test.h"
#include "test/TestUtils.h"
#include "util/Logging.h"
#include "util/Math.h"
#include "util/TmpDir.h"

#include <cstdlib>
#include <lib/util/format.h>
#include <numeric>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#define GETPID _getpid
#include <direct.h>
#else
#include <unistd.h>
#define GETPID getpid
#include <sys/stat.h>
#endif

#include "test/SimpleTestReporter.h"

namespace Catch
{

SimpleTestReporter::~SimpleTestReporter()
{
}
}

namespace viichain
{

struct ReseedPRNGListener : Catch::TestEventListenerBase
{
    using TestEventListenerBase::TestEventListenerBase;
    static unsigned int sCommandLineSeed;
    static void
    reseed()
    {
        if (sCommandLineSeed == 0)
        {
            srand(1);
            gRandomEngine.seed(gRandomEngine.default_seed);
            Catch::rng().seed(Catch::rng().default_seed);
        }
        else
        {
            srand(sCommandLineSeed);
            gRandomEngine.seed(sCommandLineSeed);
            Catch::rng().seed(sCommandLineSeed);
        }
    }
    virtual void
    testCaseStarting(Catch::TestCaseInfo const& testInfo) override
    {
        reseed();
    }
};

unsigned int ReseedPRNGListener::sCommandLineSeed = 0;

CATCH_REGISTER_LISTENER(ReseedPRNGListener)

static std::vector<std::string> gTestMetrics;
static std::vector<std::unique_ptr<Config>> gTestCfg[Config::TESTDB_MODES];
static std::vector<TmpDir> gTestRoots;
static bool gTestAllVersions{false};
static std::vector<uint32> gVersionsToTest;
static int gBaseInstance{0};

bool force_sqlite = (std::getenv("VII_FORCE_SQLITE") != nullptr);

Config const&
getTestConfig(int instanceNumber, Config::TestDbMode mode)
{
    instanceNumber += gBaseInstance;
    if (mode == Config::TESTDB_DEFAULT)
    {
                        mode = Config::TESTDB_IN_MEMORY_SQLITE;
                    }
    auto& cfgs = gTestCfg[mode];
    if (cfgs.size() <= static_cast<size_t>(instanceNumber))
    {
        cfgs.resize(instanceNumber + 1);
    }

    if (!cfgs[instanceNumber])
    {
        gTestRoots.emplace_back("vii-core-test");

        std::string rootDir = gTestRoots.back().getName();
        rootDir += "/";

        cfgs[instanceNumber] = std::make_unique<Config>();
        Config& thisConfig = *cfgs[instanceNumber];
        thisConfig.USE_CONFIG_FOR_GENESIS = true;

        std::ostringstream sstream;

        sstream << "vii" << instanceNumber << ".log";
        thisConfig.LOG_FILE_PATH = sstream.str();
        thisConfig.BUCKET_DIR_PATH = rootDir + "bucket";

        thisConfig.INVARIANT_CHECKS = {".*"};

        thisConfig.ALLOW_LOCALHOST_FOR_TESTING = true;

                thisConfig.TESTING_UPGRADE_DATETIME = VirtualClock::from_time_t(1);

                                        thisConfig.RUN_STANDALONE = true;
        thisConfig.FORCE_SCP = true;

        thisConfig.PEER_PORT =
            static_cast<unsigned short>(DEFAULT_PEER_PORT + instanceNumber * 2);
        thisConfig.HTTP_PORT = static_cast<unsigned short>(
            DEFAULT_PEER_PORT + instanceNumber * 2 + 1);

                                                                                        thisConfig.NODE_SEED = SecretKey::pseudoRandomForTestingFromSeed(
            0xFFFF0000 +
            (instanceNumber ^ ReseedPRNGListener::sCommandLineSeed));
        thisConfig.NODE_IS_VALIDATOR = true;

                thisConfig.QUORUM_SET.validators.push_back(
            thisConfig.NODE_SEED.getPublicKey());
        thisConfig.QUORUM_SET.threshold = 1;
        thisConfig.UNSAFE_QUORUM = true;

        thisConfig.NETWORK_PASSPHRASE = "(V) (;,,;) (V)";

        std::ostringstream dbname;
        switch (mode)
        {
        case Config::TESTDB_IN_MEMORY_SQLITE:
            dbname << "sqlite3://:memory:";
            break;
        case Config::TESTDB_ON_DISK_SQLITE:
            dbname << "sqlite3://" << rootDir << "test" << instanceNumber
                   << ".db";
            break;
#ifdef USE_POSTGRES
        case Config::TESTDB_POSTGRESQL:
            dbname << "postgresql://dbname=test" << instanceNumber;
            break;
#endif
        default:
            abort();
        }
        thisConfig.DATABASE = SecretValue{dbname.str()};
        thisConfig.REPORT_METRICS = gTestMetrics;
                thisConfig.AUTOMATIC_MAINTENANCE_COUNT = 0;
    }
    return *cfgs[instanceNumber];
}

int
test(int argc, char* const* argv, el::Level ll,
     std::vector<std::string> const& metrics)
{
    gTestMetrics = metrics;

            Logging::setFmt("<test>");
    Logging::setLogLevel(ll, nullptr);
    Config const& cfg = getTestConfig();
    Logging::setLoggingToFile(cfg.LOG_FILE_PATH);
    Logging::setLogLevel(ll, nullptr);

    LOG(INFO) << "Testing vii-core " << VII_CORE_VERSION;
    LOG(INFO) << "Logging to " << cfg.LOG_FILE_PATH;

    using namespace Catch;
    Session session{};

    auto cli = session.cli();
    cli |= clara::Opt(gTestAllVersions)["--all-versions"]("Test all versions");
    cli |= clara::Opt(gVersionsToTest,
                      "version")["--version"]("Test specific version(s)");
    cli |= clara::Opt(gBaseInstance, "offset")["--base-instance"](
        "Instance number offset so multiple instances of "
        "vii-core can run tests concurrently");
    session.cli(cli);

    auto r = session.applyCommandLine(argc, argv);
    if (r != 0)
        return r;
    ReseedPRNGListener::sCommandLineSeed = session.configData().rngSeed;
    ReseedPRNGListener::reseed();
    if (gVersionsToTest.empty())
    {
        gVersionsToTest.emplace_back(Config::CURRENT_LEDGER_PROTOCOL_VERSION);
    }
    r = session.run();
    gTestRoots.clear();
    gTestCfg->clear();
    if (r != 0 && ReseedPRNGListener::sCommandLineSeed != 0)
    {
        LOG(FATAL) << "Nonzero test result with --rng-seed "
                   << ReseedPRNGListener::sCommandLineSeed;
    }
    return r;
}

int
runTest(CommandLineArgs const& args)
{
    el::Level logLevel{el::Level::Info};

    Catch::Session session{};

    auto parser = session.cli();
    parser |= Catch::clara::Opt(
        [&](std::string const& arg) {
            logLevel = Logging::getLLfromString(arg);
        },
        "LEVEL")["--ll"]("set the log level");
    parser |= Catch::clara::Opt(gTestMetrics, "METRIC-NAME")["--metric"](
        "report metric METRIC-NAME on exit");
    parser |= Catch::clara::Opt(gTestAllVersions)["--all-versions"](
        "test all versions");
    parser |= Catch::clara::Opt(gVersionsToTest, "version")["--version"](
        "test specific version(s)");
    parser |= Catch::clara::Opt(gBaseInstance, "offset")["--base-instance"](
        "instance number offset so multiple instances of "
        "vii-core can run tests concurrently");
    session.cli(parser);

    auto result = session.cli().parse(
        args.mCommandName, Catch::clara::detail::TokenStream{
                               std::begin(args.mArgs), std::end(args.mArgs)});
    if (!result)
    {
        writeWithTextFlow(std::cerr, result.errorMessage());
        writeWithTextFlow(std::cerr, args.mCommandDescription);
        session.cli().writeToStream(std::cerr);
        return 1;
    }

    if (session.configData().showHelp)
    {
        writeWithTextFlow(std::cout, args.mCommandDescription);
        session.cli().writeToStream(std::cout);
        return 0;
    }

    if (session.configData().libIdentify)
    {
        session.libIdentify();
        return 0;
    }

            Logging::setFmt("<test>");
    Logging::setLogLevel(logLevel, nullptr);
    Config const& cfg = getTestConfig();
    Logging::setLoggingToFile(cfg.LOG_FILE_PATH);
    Logging::setLogLevel(logLevel, nullptr);
    auto seed = session.configData().rngSeed;

    LOG(INFO) << "Testing vii-core " << VII_CORE_VERSION;
    LOG(INFO) << "Logging to " << cfg.LOG_FILE_PATH;

    if (gVersionsToTest.empty())
    {
        gVersionsToTest.emplace_back(Config::CURRENT_LEDGER_PROTOCOL_VERSION);
    }
    if (seed != 0)
    {
        viichain::gRandomEngine.seed(seed);
    }

    auto r = session.run();
    gTestRoots.clear();
    gTestCfg->clear();
    if (r != 0 && seed != 0)
    {
        LOG(FATAL) << "Nonzero test result with --rng-seed " << seed;
    }
    return r;
}

void
for_versions_to(uint32 to, Application& app, std::function<void(void)> const& f)
{
    for_versions(1, to, app, f);
}

void
for_versions_from(uint32 from, Application& app,
                  std::function<void(void)> const& f)
{
    for_versions(from, Config::CURRENT_LEDGER_PROTOCOL_VERSION, app, f);
}

void
for_versions_from(std::vector<uint32> const& versions, Application& app,
                  std::function<void(void)> const& f)
{
    for_versions(versions, app, f);
    for_versions_from(versions.back() + 1, app, f);
}

void
for_all_versions(Application& app, std::function<void(void)> const& f)
{
    for_versions(1, Config::CURRENT_LEDGER_PROTOCOL_VERSION, app, f);
}

void
for_all_versions(Config const& cfg, std::function<void(Config const&)> const& f)
{
    for_versions(1, Config::CURRENT_LEDGER_PROTOCOL_VERSION, cfg, f);
}

void
for_versions(uint32 from, uint32 to, Application& app,
             std::function<void(void)> const& f)
{
    if (from > to)
    {
        return;
    }
    auto versions = std::vector<uint32>{};
    versions.resize(to - from + 1);
    std::iota(std::begin(versions), std::end(versions), from);

    for_versions(versions, app, f);
}

void
for_versions(uint32 from, uint32 to, Config const& cfg,
             std::function<void(Config const&)> const& f)
{
    if (from > to)
    {
        return;
    }
    auto versions = std::vector<uint32>{};
    versions.resize(to - from + 1);
    std::iota(std::begin(versions), std::end(versions), from);

    for_versions(versions, cfg, f);
}

void
for_versions(std::vector<uint32> const& versions, Application& app,
             std::function<void(void)> const& f)
{
    uint32_t previousVersion = 0;
    {
        LedgerTxn ltx(app.getLedgerTxnRoot());
        previousVersion = ltx.loadHeader().current().ledgerVersion;
    }

    for (auto v : versions)
    {
        if (!gTestAllVersions &&
            std::find(gVersionsToTest.begin(), gVersionsToTest.end(), v) ==
                gVersionsToTest.end())
        {
            continue;
        }
        SECTION("protocol version " + std::to_string(v))
        {
            {
                LedgerTxn ltx(app.getLedgerTxnRoot());
                ltx.loadHeader().current().ledgerVersion = v;
                ltx.commit();
            }
            f();
        }
    }

    {
        LedgerTxn ltx(app.getLedgerTxnRoot());
        ltx.loadHeader().current().ledgerVersion = previousVersion;
        ltx.commit();
    }
}

void
for_versions(std::vector<uint32> const& versions, Config const& cfg,
             std::function<void(Config const&)> const& f)
{
    for (auto v : versions)
    {
        if (!gTestAllVersions &&
            std::find(gVersionsToTest.begin(), gVersionsToTest.end(), v) ==
                gVersionsToTest.end())
        {
            continue;
        }
        SECTION("protocol version " + std::to_string(v))
        {
            Config vcfg = cfg;
            vcfg.LEDGER_PROTOCOL_VERSION = v;
            f(vcfg);
        }
    }
}

void
for_all_versions_except(std::vector<uint32> const& versions, Application& app,
                        std::function<void(void)> const& f)
{
    uint32 lastExcept = 0;
    for (uint32 except : versions)
    {
        for_versions(lastExcept + 1, except - 1, app, f);
        lastExcept = except;
    }
    for_versions_from(lastExcept + 1, app, f);
}
}

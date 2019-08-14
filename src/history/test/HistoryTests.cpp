
#include "bucket/BucketManager.h"
#include "catchup/test/CatchupWorkTests.h"
#include "history/FileTransferInfo.h"
#include "history/HistoryArchiveManager.h"
#include "history/HistoryManager.h"
#include "history/test/HistoryTestsUtils.h"
#include "historywork/GetHistoryArchiveStateWork.h"
#include "historywork/GunzipFileWork.h"
#include "historywork/GzipFileWork.h"
#include "historywork/PutHistoryArchiveStateWork.h"
#include "ledger/LedgerManager.h"
#include "main/ExternalQueue.h"
#include "main/PersistentState.h"
#include "process/ProcessManager.h"
#include "test/TestAccount.h"
#include "test/TestUtils.h"
#include "test/TxTests.h"
#include "test/test.h"
#include "util/Fs.h"
#include "util/Logging.h"
#include "work/WorkScheduler.h"

#include "historywork/DownloadBucketsWork.h"
#include <lib/catch.hpp>
#include <lib/util/format.h>

using namespace viichain;
using namespace historytestutils;

TEST_CASE("next checkpoint ledger", "[history]")
{
    CatchupSimulation catchupSimulation{};
    HistoryManager& hm = catchupSimulation.getApp().getHistoryManager();
    REQUIRE(hm.nextCheckpointLedger(0) == 64);
    REQUIRE(hm.nextCheckpointLedger(1) == 64);
    REQUIRE(hm.nextCheckpointLedger(32) == 64);
    REQUIRE(hm.nextCheckpointLedger(62) == 64);
    REQUIRE(hm.nextCheckpointLedger(63) == 64);
    REQUIRE(hm.nextCheckpointLedger(64) == 64);
    REQUIRE(hm.nextCheckpointLedger(65) == 128);
    REQUIRE(hm.nextCheckpointLedger(66) == 128);
    REQUIRE(hm.nextCheckpointLedger(126) == 128);
    REQUIRE(hm.nextCheckpointLedger(127) == 128);
    REQUIRE(hm.nextCheckpointLedger(128) == 128);
    REQUIRE(hm.nextCheckpointLedger(129) == 192);
    REQUIRE(hm.nextCheckpointLedger(130) == 192);
}

TEST_CASE("HistoryManager compress", "[history]")
{
    CatchupSimulation catchupSimulation{};

    std::string s = "hello there";
    HistoryManager& hm = catchupSimulation.getApp().getHistoryManager();
    std::string fname = hm.localFilename("compressme");
    {
        std::ofstream out(fname, std::ofstream::binary);
        out.write(s.data(), s.size());
    }
    std::string compressed = fname + ".gz";
    auto& wm = catchupSimulation.getApp().getWorkScheduler();
    auto g = wm.executeWork<GzipFileWork>(fname);
    REQUIRE(g->getState() == BasicWork::State::WORK_SUCCESS);
    REQUIRE(!fs::exists(fname));
    REQUIRE(fs::exists(compressed));

    auto u = wm.executeWork<GunzipFileWork>(compressed);
    REQUIRE(u->getState() == BasicWork::State::WORK_SUCCESS);
    REQUIRE(fs::exists(fname));
    REQUIRE(!fs::exists(compressed));
}

TEST_CASE("HistoryArchiveState get_put", "[history]")
{
    CatchupSimulation catchupSimulation{};

    HistoryArchiveState has;
    has.currentLedger = 0x1234;

    auto archive =
        catchupSimulation.getApp().getHistoryArchiveManager().getHistoryArchive(
            "test");
    REQUIRE(archive);

    has.resolveAllFutures();

    auto& wm = catchupSimulation.getApp().getWorkScheduler();
    auto put = wm.executeWork<PutHistoryArchiveStateWork>(has, archive);
    REQUIRE(put->getState() == BasicWork::State::WORK_SUCCESS);

    HistoryArchiveState has2;
    auto get = wm.executeWork<GetHistoryArchiveStateWork>(has2, 0, archive);
    REQUIRE(get->getState() == BasicWork::State::WORK_SUCCESS);
    REQUIRE(has2.currentLedger == 0x1234);
}

TEST_CASE("History bucket verification",
          "[history][bucketverification][batching]")
{
        Config cfg(getTestConfig());
    VirtualClock clock;
    auto cg = std::make_shared<TmpDirHistoryConfigurator>();
    cg->configure(cfg, true);
    Application::pointer app = createTestApplication(clock, cfg);
    REQUIRE(app->getHistoryArchiveManager().initializeHistoryArchive("test"));

    auto bucketGenerator = TestBucketGenerator{
        *app, app->getHistoryArchiveManager().getHistoryArchive("test")};
    std::vector<std::string> hashes;
    auto& wm = app->getWorkScheduler();
    std::map<std::string, std::shared_ptr<Bucket>> mBuckets;
    auto tmpDir =
        std::make_unique<TmpDir>(app->getTmpDirManager().tmpDir("bucket-test"));

    SECTION("successful download and verify")
    {
        hashes.push_back(bucketGenerator.generateBucket(
            TestBucketState::CONTENTS_AND_HASH_OK));
        hashes.push_back(bucketGenerator.generateBucket(
            TestBucketState::CONTENTS_AND_HASH_OK));
        auto verify =
            wm.executeWork<DownloadBucketsWork>(mBuckets, hashes, *tmpDir);
        REQUIRE(verify->getState() == BasicWork::State::WORK_SUCCESS);
    }
    SECTION("download fails file not found")
    {
        hashes.push_back(
            bucketGenerator.generateBucket(TestBucketState::FILE_NOT_UPLOADED));

        auto verify =
            wm.executeWork<DownloadBucketsWork>(mBuckets, hashes, *tmpDir);
        REQUIRE(verify->getState() == BasicWork::State::WORK_FAILURE);
    }
    SECTION("download succeeds but unzip fails")
    {
        hashes.push_back(bucketGenerator.generateBucket(
            TestBucketState::CORRUPTED_ZIPPED_FILE));

        auto verify =
            wm.executeWork<DownloadBucketsWork>(mBuckets, hashes, *tmpDir);
        REQUIRE(verify->getState() == BasicWork::State::WORK_FAILURE);
    }
    SECTION("verify fails hash mismatch")
    {
        hashes.push_back(
            bucketGenerator.generateBucket(TestBucketState::HASH_MISMATCH));

        auto verify =
            wm.executeWork<DownloadBucketsWork>(mBuckets, hashes, *tmpDir);
        REQUIRE(verify->getState() == BasicWork::State::WORK_FAILURE);
    }
    SECTION("no hashes to verify")
    {
                auto verify = wm.executeWork<DownloadBucketsWork>(
            mBuckets, std::vector<std::string>(), *tmpDir);
        REQUIRE(verify->getState() == BasicWork::State::WORK_SUCCESS);
    }
}

TEST_CASE("Ledger chain verification", "[ledgerheaderverification]")
{
    Config cfg(getTestConfig(0));
    VirtualClock clock;
    auto cg = std::make_shared<TmpDirHistoryConfigurator>();
    cg->configure(cfg, true);
    Application::pointer app = createTestApplication(clock, cfg);
    REQUIRE(app->getHistoryArchiveManager().initializeHistoryArchive("test"));

    auto tmpDir = app->getTmpDirManager().tmpDir("tmp-chain-test");
    auto& wm = app->getWorkScheduler();

    LedgerHeaderHistoryEntry firstVerified{};
    LedgerHeaderHistoryEntry verifiedAhead{};

    uint32_t initLedger = 127;
    LedgerRange ledgerRange{
        initLedger,
        initLedger + app->getHistoryManager().getCheckpointFrequency() * 10};
    CheckpointRange checkpointRange{ledgerRange, app->getHistoryManager()};
    auto ledgerChainGenerator = TestLedgerChainGenerator{
        *app, app->getHistoryArchiveManager().getHistoryArchive("test"),
        checkpointRange, tmpDir};

    auto checkExpectedBehavior = [&](Work::State expectedState,
                                     LedgerHeaderHistoryEntry lcl,
                                     LedgerHeaderHistoryEntry last) {
        auto lclPair = LedgerNumHashPair(lcl.header.ledgerSeq,
                                         make_optional<Hash>(lcl.hash));
        auto ledgerRangeEnd = LedgerNumHashPair(last.header.ledgerSeq,
                                                make_optional<Hash>(last.hash));
        auto w = wm.executeWork<VerifyLedgerChainWork>(tmpDir, ledgerRange,
                                                       lclPair, ledgerRangeEnd);
        REQUIRE(expectedState == w->getState());
    };

    LedgerHeaderHistoryEntry lcl, last;
    LOG(DEBUG) << "fully valid";
    {
        std::tie(lcl, last) = ledgerChainGenerator.makeLedgerChainFiles(
            HistoryManager::VERIFY_STATUS_OK);
        checkExpectedBehavior(BasicWork::State::WORK_SUCCESS, lcl, last);
    }
    LOG(DEBUG) << "invalid link due to bad hash";
    {
        std::tie(lcl, last) = ledgerChainGenerator.makeLedgerChainFiles(
            HistoryManager::VERIFY_STATUS_ERR_BAD_HASH);
        checkExpectedBehavior(BasicWork::State::WORK_FAILURE, lcl, last);
    }
    LOG(DEBUG) << "invalid ledger version";
    {
        std::tie(lcl, last) = ledgerChainGenerator.makeLedgerChainFiles(
            HistoryManager::VERIFY_STATUS_ERR_BAD_LEDGER_VERSION);
        checkExpectedBehavior(BasicWork::State::WORK_FAILURE, lcl, last);
    }
    LOG(DEBUG) << "overshot";
    {
        std::tie(lcl, last) = ledgerChainGenerator.makeLedgerChainFiles(
            HistoryManager::VERIFY_STATUS_ERR_OVERSHOT);
        checkExpectedBehavior(BasicWork::State::WORK_FAILURE, lcl, last);
    }
    LOG(DEBUG) << "undershot";
    {
        std::tie(lcl, last) = ledgerChainGenerator.makeLedgerChainFiles(
            HistoryManager::VERIFY_STATUS_ERR_UNDERSHOT);
        checkExpectedBehavior(BasicWork::State::WORK_FAILURE, lcl, last);
    }
    LOG(DEBUG) << "missing entries";
    {
        std::tie(lcl, last) = ledgerChainGenerator.makeLedgerChainFiles(
            HistoryManager::VERIFY_STATUS_ERR_MISSING_ENTRIES);
        checkExpectedBehavior(BasicWork::State::WORK_FAILURE, lcl, last);
    }
    LOG(DEBUG) << "chain does not agree with LCL";
    {
        std::tie(lcl, last) = ledgerChainGenerator.makeLedgerChainFiles(
            HistoryManager::VERIFY_STATUS_OK);
        lcl.hash = HashUtils::random();

        checkExpectedBehavior(BasicWork::State::WORK_FAILURE, lcl, last);
    }
    LOG(DEBUG) << "chain does not agree with LCL on checkpoint boundary";
    {
        std::tie(lcl, last) = ledgerChainGenerator.makeLedgerChainFiles(
            HistoryManager::VERIFY_STATUS_OK);
        lcl.header.ledgerSeq +=
            app->getHistoryManager().getCheckpointFrequency() - 1;
        lcl.hash = HashUtils::random();
        checkExpectedBehavior(BasicWork::State::WORK_FAILURE, lcl, last);
    }
    LOG(DEBUG) << "chain does not agree with LCL outside of range";
    {
        std::tie(lcl, last) = ledgerChainGenerator.makeLedgerChainFiles(
            HistoryManager::VERIFY_STATUS_OK);
        lcl.header.ledgerSeq -= 1;
        lcl.hash = HashUtils::random();
        checkExpectedBehavior(BasicWork::State::WORK_FAILURE, lcl, last);
    }
    LOG(DEBUG) << "chain does not agree with trusted hash";
    {
        std::tie(lcl, last) = ledgerChainGenerator.makeLedgerChainFiles(
            HistoryManager::VERIFY_STATUS_OK);
        last.hash = HashUtils::random();
        checkExpectedBehavior(BasicWork::State::WORK_FAILURE, lcl, last);
    }
    LOG(DEBUG) << "missing file";
    {
        std::tie(lcl, last) = ledgerChainGenerator.makeLedgerChainFiles(
            HistoryManager::VERIFY_STATUS_OK);
        FileTransferInfo ft(tmpDir, HISTORY_FILE_TYPE_LEDGER,
                            last.header.ledgerSeq);
        std::remove(ft.localPath_nogz().c_str());

                checkExpectedBehavior(BasicWork::State::WORK_FAILURE, lcl, last);
    }
}

TEST_CASE("History publish", "[history]")
{
    CatchupSimulation catchupSimulation{};
    auto checkpointLedger = catchupSimulation.getLastCheckpointLedger(1);
    catchupSimulation.ensureOfflineCatchupPossible(checkpointLedger);
}

static std::string
resumeModeName(uint32_t count)
{
    switch (count)
    {
    case 0:
        return "CATCHUP_MINIMAL";
    case std::numeric_limits<uint32_t>::max():
        return "CATCHUP_COMPLETE";
    default:
        return "CATCHUP_RECENT";
    }
}

static std::string
dbModeName(Config::TestDbMode mode)
{
    switch (mode)
    {
    case Config::TESTDB_IN_MEMORY_SQLITE:
        return "TESTDB_IN_MEMORY_SQLITE";
    case Config::TESTDB_ON_DISK_SQLITE:
        return "TESTDB_ON_DISK_SQLITE";
#ifdef USE_POSTGRES
    case Config::TESTDB_POSTGRESQL:
        return "TESTDB_POSTGRESQL";
#endif
    default:
        abort();
    }
}

TEST_CASE("History catchup", "[history][catchup]")
{
                CatchupSimulation catchupSimulation{VirtualClock::REAL_TIME};
    auto checkpointLedger = catchupSimulation.getLastCheckpointLedger(3);
    auto app = catchupSimulation.createCatchupApplication(
        std::numeric_limits<uint32_t>::max(), Config::TESTDB_ON_DISK_SQLITE,
        "app");

    auto offlineNonCheckpointDestinationLedger =
        checkpointLedger -
        app->getHistoryManager().getCheckpointFrequency() / 2;

    SECTION("when not enough publishes has been performed")
    {
                catchupSimulation.ensureLedgerAvailable(checkpointLedger);

        SECTION("online")
        {
            REQUIRE(!catchupSimulation.catchupOnline(app, checkpointLedger));
        }

        SECTION("offline")
        {
            REQUIRE(!catchupSimulation.catchupOffline(app, checkpointLedger));
        }

        SECTION("offline, in the middle of checkpoint")
        {
            REQUIRE(!catchupSimulation.catchupOffline(
                app, offlineNonCheckpointDestinationLedger));
        }
    }

    SECTION("when enough publishes has been performed, but no trigger ledger "
            "was externalized")
    {
                catchupSimulation.ensureLedgerAvailable(checkpointLedger + 1);
        catchupSimulation.ensurePublishesComplete();

        SECTION("online")
        {
            REQUIRE(!catchupSimulation.catchupOnline(app, checkpointLedger));
        }

        SECTION("offline")
        {
            REQUIRE(catchupSimulation.catchupOffline(app, checkpointLedger));
        }

        SECTION("offline, in the middle of checkpoint")
        {
            REQUIRE(catchupSimulation.catchupOffline(
                app, offlineNonCheckpointDestinationLedger));
        }
    }

    SECTION("when enough publishes has been performed, but no closing ledger "
            "was externalized")
    {
                catchupSimulation.ensureLedgerAvailable(checkpointLedger + 2);
        catchupSimulation.ensurePublishesComplete();
        REQUIRE(!catchupSimulation.catchupOnline(app, checkpointLedger));
    }

    SECTION("when enough publishes has been performed, 3 ledgers are buffered "
            "and no closing ledger was externalized")
    {
                        catchupSimulation.ensureLedgerAvailable(checkpointLedger + 5);
        catchupSimulation.ensurePublishesComplete();
        REQUIRE(!catchupSimulation.catchupOnline(app, checkpointLedger, 3));
    }

    SECTION("when enough publishes has been performed, 3 ledgers are buffered "
            "and closing ledger was externalized")
    {
                        catchupSimulation.ensureLedgerAvailable(checkpointLedger + 6);
        catchupSimulation.ensurePublishesComplete();
        REQUIRE(catchupSimulation.catchupOnline(app, checkpointLedger, 3));
    }
}

TEST_CASE("History catchup with different modes", "[history][catchup]")
{
    CatchupSimulation catchupSimulation{};

    auto checkpointLedger = catchupSimulation.getLastCheckpointLedger(3);
    catchupSimulation.ensureOnlineCatchupPossible(checkpointLedger, 5);

    std::vector<Application::pointer> apps;

    std::vector<uint32_t> counts = {0, std::numeric_limits<uint32_t>::max(),
                                    60};

    std::vector<Config::TestDbMode> dbModes = {Config::TESTDB_IN_MEMORY_SQLITE,
                                               Config::TESTDB_ON_DISK_SQLITE};
#ifdef USE_POSTGRES
    if (!force_sqlite)
        dbModes.push_back(Config::TESTDB_POSTGRESQL);
#endif

    for (auto dbMode : dbModes)
    {
        for (auto count : counts)
        {
            auto a = catchupSimulation.createCatchupApplication(
                count, dbMode,
                std::string("full, ") + resumeModeName(count) + ", " +
                    dbModeName(dbMode));
            REQUIRE(catchupSimulation.catchupOnline(a, checkpointLedger, 5));
            apps.push_back(a);
        }
    }
}

TEST_CASE("History prefix catchup", "[history][catchup][prefixcatchup]")
{
    CatchupSimulation catchupSimulation{};

    auto checkpointLedger = catchupSimulation.getLastCheckpointLedger(3);
    catchupSimulation.ensureOnlineCatchupPossible(checkpointLedger, 5);

    auto a = catchupSimulation.createCatchupApplication(
        std::numeric_limits<uint32_t>::max(), Config::TESTDB_IN_MEMORY_SQLITE,
        std::string("Catchup to prefix of published history"));
                                REQUIRE(catchupSimulation.catchupOnline(a, 10, 5));
    uint32_t freq = a->getHistoryManager().getCheckpointFrequency();
    REQUIRE(a->getLedgerManager().getLastClosedLedgerNum() == freq + 7);

                                auto b = catchupSimulation.createCatchupApplication(
        std::numeric_limits<uint32_t>::max(), Config::TESTDB_IN_MEMORY_SQLITE,
        std::string("Catchup to second prefix of published history"));
    REQUIRE(catchupSimulation.catchupOnline(b, freq + 10, 5));
    REQUIRE(b->getLedgerManager().getLastClosedLedgerNum() == 2 * freq + 7);
}

TEST_CASE("Catchup non-initentry buckets to initentry-supporting works",
          "[history][historyinitentry]")
{
    uint32_t newProto =
        Bucket::FIRST_PROTOCOL_SUPPORTING_INITENTRY_AND_METAENTRY;
    uint32_t oldProto = newProto - 1;
    auto configurator =
        std::make_shared<RealGenesisTmpDirHistoryConfigurator>();
    CatchupSimulation catchupSimulation{VirtualClock::VIRTUAL_TIME,
                                        configurator};

        catchupSimulation.generateRandomLedger(oldProto);

    auto checkpointLedger = catchupSimulation.getLastCheckpointLedger(3);
    catchupSimulation.ensureOnlineCatchupPossible(checkpointLedger);

    std::vector<Application::pointer> apps;
    std::vector<uint32_t> counts = {0, std::numeric_limits<uint32_t>::max(),
                                    60};
    for (auto count : counts)
    {
        auto a = catchupSimulation.createCatchupApplication(
            count, Config::TESTDB_IN_MEMORY_SQLITE,
            std::string("full, ") + resumeModeName(count) + ", " +
                dbModeName(Config::TESTDB_IN_MEMORY_SQLITE));
        REQUIRE(catchupSimulation.catchupOnline(a, checkpointLedger - 2));

                        auto mc = a->getBucketManager().readMergeCounters();
        REQUIRE(mc.mPostInitEntryProtocolMerges == 0);
        REQUIRE(mc.mNewInitEntries == 0);
        REQUIRE(mc.mOldInitEntries == 0);

                for (auto i = 0; i < 3; ++i)
        {
            auto root = TestAccount{*a, txtest::getRoot(a->getNetworkID())};
            auto stranger = TestAccount{
                *a, txtest::getAccount(fmt::format("stranger{}", i))};
            auto& lm = a->getLedgerManager();
            TxSetFramePtr txSet = std::make_shared<TxSetFrame>(
                lm.getLastClosedLedgerHeader().hash);
            uint32_t ledgerSeq = lm.getLastClosedLedgerNum() + 1;
            uint64_t minBalance = lm.getLastMinBalance(5);
            uint64_t big = minBalance + ledgerSeq;
            uint64_t closeTime = 60 * 5 * ledgerSeq;
            txSet->add(root.tx({txtest::createAccount(stranger, big)}));
                        txSet->getContentsHash();

                                    auto upgrades = xdr::xvector<UpgradeType, 6>{};
            if (i == 0)
            {
                auto ledgerUpgrade = LedgerUpgrade{LEDGER_UPGRADE_VERSION};
                ledgerUpgrade.newLedgerVersion() = newProto;
                auto v = xdr::xdr_to_opaque(ledgerUpgrade);
                upgrades.push_back(UpgradeType{v.begin(), v.end()});
            }
            CLOG(DEBUG, "History")
                << "Closing synthetic ledger " << ledgerSeq << " with "
                << txSet->size(lm.getLastClosedLedgerHeader().header)
                << " txs (txhash:" << hexAbbrev(txSet->getContentsHash())
                << ")";
            VIIValue sv(txSet->getContentsHash(), closeTime, upgrades,
                            VII_VALUE_BASIC);
            lm.closeLedger(LedgerCloseData(ledgerSeq, txSet, sv));
        }

                mc = a->getBucketManager().readMergeCounters();
        REQUIRE(mc.mPostInitEntryProtocolMerges != 0);
        REQUIRE(mc.mNewInitEntries != 0);
        REQUIRE(mc.mOldInitEntries != 0);

        apps.push_back(a);
    }
}

TEST_CASE("Publish catchup alternation with stall",
          "[history][catchup][catchupalternation]")
{
    CatchupSimulation catchupSimulation{};
    auto& lm = catchupSimulation.getApp().getLedgerManager();

                auto checkpoint = 3;
    auto checkpointLedger =
        catchupSimulation.getLastCheckpointLedger(checkpoint);
    catchupSimulation.ensureOnlineCatchupPossible(checkpointLedger, 5);

    auto completeApp = catchupSimulation.createCatchupApplication(
        std::numeric_limits<uint32_t>::max(), Config::TESTDB_IN_MEMORY_SQLITE,
        std::string("completeApp"));
    auto minimalApp = catchupSimulation.createCatchupApplication(
        0, Config::TESTDB_IN_MEMORY_SQLITE, std::string("minimalApp"));

    REQUIRE(catchupSimulation.catchupOnline(completeApp, checkpointLedger, 5));
    REQUIRE(catchupSimulation.catchupOnline(minimalApp, checkpointLedger, 5));

    for (int i = 1; i < 4; ++i)
    {
                checkpoint += i;
        checkpointLedger =
            catchupSimulation.getLastCheckpointLedger(checkpoint);
        catchupSimulation.ensureOnlineCatchupPossible(checkpointLedger, 5);

        REQUIRE(
            catchupSimulation.catchupOnline(completeApp, checkpointLedger, 5));
        REQUIRE(
            catchupSimulation.catchupOnline(minimalApp, checkpointLedger, 5));
    }

            catchupSimulation.generateRandomLedger();
    catchupSimulation.generateRandomLedger();
    catchupSimulation.generateRandomLedger();

                auto targetLedger = lm.getLastClosedLedgerNum();
    REQUIRE(!catchupSimulation.catchupOnline(completeApp, targetLedger, 5));
    REQUIRE(!catchupSimulation.catchupOnline(minimalApp, targetLedger, 5));

            catchupSimulation.ensureOnlineCatchupPossible(targetLedger, 5);

    REQUIRE(catchupSimulation.catchupOnline(completeApp, targetLedger, 5));
    REQUIRE(catchupSimulation.catchupOnline(minimalApp, targetLedger, 5));
}

TEST_CASE("Repair missing buckets via history",
          "[history][historybucketrepair]")
{
    CatchupSimulation catchupSimulation{};
    auto checkpointLedger = catchupSimulation.getLastCheckpointLedger(1);
    catchupSimulation.ensureOfflineCatchupPossible(checkpointLedger);

                        HistoryArchiveState has(checkpointLedger + 1,
                            catchupSimulation.getBucketListAtLastPublish());
    has.resolveAllFutures();
    auto state = has.toString();

    auto cfg = getTestConfig(1);
    cfg.BUCKET_DIR_PATH += "2";
    VirtualClock clock;
    auto app = createTestApplication(
        clock,
        catchupSimulation.getHistoryConfigurator().configure(cfg, false));
    app->getPersistentState().setState(PersistentState::kHistoryArchiveState,
                                       state);

    app->start();
    catchupSimulation.crankUntil(
        app, [&]() { return app->getWorkScheduler().allChildrenDone(); },
        std::chrono::seconds(30));

    auto hash1 = catchupSimulation.getBucketListAtLastPublish().getHash();
    auto hash2 = app->getBucketManager().getBucketList().getHash();
    REQUIRE(hash1 == hash2);
}

TEST_CASE("Repair missing buckets fails", "[history][historybucketrepair]")
{
    CatchupSimulation catchupSimulation{};
    auto checkpointLedger = catchupSimulation.getLastCheckpointLedger(1);
    catchupSimulation.ensureOfflineCatchupPossible(checkpointLedger);

                        HistoryArchiveState has(checkpointLedger + 1,
                            catchupSimulation.getBucketListAtLastPublish());
    has.resolveAllFutures();
    auto state = has.toString();

            auto dir = catchupSimulation.getHistoryConfigurator().getArchiveDirName();
    REQUIRE(!dir.empty());
    fs::deltree(dir + "/bucket");

    auto cfg = getTestConfig(1);
    VirtualClock clock;
    cfg.BUCKET_DIR_PATH += "2";
    auto app = createTestApplication(
        clock,
        catchupSimulation.getHistoryConfigurator().configure(cfg, false));
    app->getPersistentState().setState(PersistentState::kHistoryArchiveState,
                                       state);

        REQUIRE_THROWS_AS(app->start(), std::runtime_error);
}

TEST_CASE("Publish catchup via s3", "[!hide][s3]")
{
    CatchupSimulation catchupSimulation{
        VirtualClock::VIRTUAL_TIME, std::make_shared<S3HistoryConfigurator>()};
    auto checkpointLedger = catchupSimulation.getLastCheckpointLedger(3);
    catchupSimulation.ensureOfflineCatchupPossible(checkpointLedger);

    auto app = catchupSimulation.createCatchupApplication(
        std::numeric_limits<uint32_t>::max(), Config::TESTDB_IN_MEMORY_SQLITE,
        "s3");
    REQUIRE(catchupSimulation.catchupOnline(app, checkpointLedger, 5));
}

TEST_CASE("persist publish queue", "[history]")
{
    Config cfg(getTestConfig(0, Config::TESTDB_ON_DISK_SQLITE));
    cfg.MAX_CONCURRENT_SUBPROCESSES = 0;
    cfg.ARTIFICIALLY_ACCELERATE_TIME_FOR_TESTING = true;
    TmpDirHistoryConfigurator tcfg;
    cfg = tcfg.configure(cfg, true);

    {
        VirtualClock clock;
        Application::pointer app0 = createTestApplication(clock, cfg);
        app0->start();
        auto& hm0 = app0->getHistoryManager();
        while (hm0.getPublishQueueCount() < 5)
        {
            clock.crank(true);
        }
                        REQUIRE(hm0.getPublishSuccessCount() == 0);
        REQUIRE(hm0.getMinLedgerQueuedToPublish() == 7);
        while (clock.cancelAllEvents() ||
               app0->getProcessManager().getNumRunningProcesses() > 0)
        {
            clock.crank(true);
        }
        LOG(INFO) << app0->isStopping();

                ExternalQueue ps(*app0);
        ps.deleteOldEntries(50000);
    }

    cfg.MAX_CONCURRENT_SUBPROCESSES = 32;

    {
        VirtualClock clock;
        Application::pointer app1 = Application::create(clock, cfg, 0);
        app1->getHistoryArchiveManager().initializeHistoryArchive("test");
        for (size_t i = 0; i < 100; ++i)
            clock.crank(false);
        app1->start();
        auto& hm1 = app1->getHistoryManager();
        while (hm1.getPublishSuccessCount() < 5)
        {
            clock.crank(true);

                        ExternalQueue ps(*app1);
            ps.deleteOldEntries(50000);
        }
                        auto minLedger = hm1.getMinLedgerQueuedToPublish();
        LOG(INFO) << "minLedger " << minLedger;
        bool okQueue = minLedger == 0 || minLedger >= 35;
        REQUIRE(okQueue);
        clock.cancelAllEvents();
        while (clock.cancelAllEvents() ||
               app1->getProcessManager().getNumRunningProcesses() > 0)
        {
            clock.crank(true);
        }
        LOG(INFO) << app1->isStopping();
    }
}

TEST_CASE("catchup with a gap", "[history][catchup][catchupstall]")
{
    CatchupSimulation catchupSimulation{};
    auto checkpointLedger = catchupSimulation.getLastCheckpointLedger(1);
    catchupSimulation.ensureOnlineCatchupPossible(checkpointLedger, 5);

        auto app = catchupSimulation.createCatchupApplication(
        std::numeric_limits<uint32_t>::max(), Config::TESTDB_IN_MEMORY_SQLITE,
        "app2");
    REQUIRE(catchupSimulation.catchupOnline(app, checkpointLedger, 5));

        checkpointLedger = catchupSimulation.getLastCheckpointLedger(2);
    catchupSimulation.ensureOnlineCatchupPossible(checkpointLedger, 5);

    auto init = app->getLedgerManager().getLastClosedLedgerNum() + 2;
    REQUIRE(init == 73);

        LOG(INFO) << "Starting catchup (with gap) from " << init;
    REQUIRE(!catchupSimulation.catchupOnline(app, init, 5, init + 10));
    REQUIRE(app->getLedgerManager().getLastClosedLedgerNum() == 82);

        checkpointLedger = catchupSimulation.getLastCheckpointLedger(3);
    catchupSimulation.ensureOnlineCatchupPossible(checkpointLedger, 5);

        CHECK(catchupSimulation.catchupOnline(app, checkpointLedger, 5));
}

TEST_CASE("Catchup recent", "[history][catchup][catchuprecent][!hide]")
{
    CatchupSimulation catchupSimulation{};
    auto checkpointLedger = catchupSimulation.getLastCheckpointLedger(3);
    catchupSimulation.ensureOnlineCatchupPossible(checkpointLedger, 5);

    auto dbMode = Config::TESTDB_IN_MEMORY_SQLITE;
    std::vector<Application::pointer> apps;

        
            std::vector<uint32_t> recents = {0,   1,   2,   31,  32,  33,  62,  63,
                                     64,  65,  66,  126, 127, 128, 129, 130,
                                     190, 191, 192, 193, 194, 1000};

    for (auto r : recents)
    {
        auto name = std::string("catchup-recent-") + std::to_string(r);
        auto app = catchupSimulation.createCatchupApplication(r, dbMode, name);
        REQUIRE(catchupSimulation.catchupOnline(app, checkpointLedger, 5));
        apps.push_back(app);
    }

            checkpointLedger = catchupSimulation.getLastCheckpointLedger(5);
    catchupSimulation.ensureOnlineCatchupPossible(checkpointLedger, 5);

    for (auto const& app : apps)
    {
        REQUIRE(catchupSimulation.catchupOnline(app, checkpointLedger, 5));
    }

            checkpointLedger = catchupSimulation.getLastCheckpointLedger(30);
    catchupSimulation.ensureOnlineCatchupPossible(checkpointLedger, 5);

    for (auto const& app : apps)
    {
        REQUIRE(catchupSimulation.catchupOnline(app, checkpointLedger, 5));
    }
}

TEST_CASE("Catchup manual", "[history][catchup][catchupmanual]")
{
    CatchupSimulation catchupSimulation{};
    auto checkpointLedger = catchupSimulation.getLastCheckpointLedger(6);
    catchupSimulation.ensureOfflineCatchupPossible(checkpointLedger);
    auto dbMode = Config::TESTDB_IN_MEMORY_SQLITE;

        for (auto i = 0; i < viichain::gCatchupRangeCases.size(); i += 10)
    {
        auto test = viichain::gCatchupRangeCases[i];
        auto configuration = test.second;
        auto name =
            fmt::format("lcl = {}, to ledger = {}, count = {}", test.first,
                        configuration.toLedger(), configuration.count());
        LOG(INFO) << "Catchup configuration: " << name;
                auto app = catchupSimulation.createCatchupApplication(
            configuration.count(), dbMode, name);
        REQUIRE(
            catchupSimulation.catchupOffline(app, configuration.toLedger()));
    }
}

TEST_CASE("initialize existing history store fails", "[history]")
{
    Config cfg(getTestConfig(0, Config::TESTDB_ON_DISK_SQLITE));
    TmpDirHistoryConfigurator tcfg;
    cfg = tcfg.configure(cfg, true);

    {
        VirtualClock clock;
        Application::pointer app = createTestApplication(clock, cfg);
        REQUIRE(
            app->getHistoryArchiveManager().initializeHistoryArchive("test"));
    }

    {
        VirtualClock clock;
        Application::pointer app = createTestApplication(clock, cfg);
        REQUIRE(
            !app->getHistoryArchiveManager().initializeHistoryArchive("test"));
    }
}

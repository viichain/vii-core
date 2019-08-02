
#include "catchup/test/CatchupWorkTests.h"
#include "catchup/CatchupConfiguration.h"
#include "catchup/CatchupWork.h"
#include "ledger/CheckpointRange.h"
#include "test/TestUtils.h"
#include "test/test.h"
#include "util/Logging.h"
#include "util/Timer.h"
#include <lib/catch.hpp>
#include <lib/util/format.h>

using namespace viichain;

auto max = std::numeric_limits<uint32_t>::max();
namespace viichain
{
std::vector<std::pair<uint32_t, CatchupConfiguration>> gCatchupRangeCases{
            {1, {2, 0, CatchupConfiguration::Mode::OFFLINE}},
    {1, {2, 1, CatchupConfiguration::Mode::OFFLINE}},
    {1, {2, max, CatchupConfiguration::Mode::OFFLINE}},
        {1, {63, 0, CatchupConfiguration::Mode::OFFLINE}},
    {1, {63, 1, CatchupConfiguration::Mode::OFFLINE}},
    {1, {63, 2, CatchupConfiguration::Mode::OFFLINE}},
    {1, {63, max, CatchupConfiguration::Mode::OFFLINE}},
        {1, {64, 0, CatchupConfiguration::Mode::OFFLINE}},
    {1, {64, 1, CatchupConfiguration::Mode::OFFLINE}},
    {1, {64, 2, CatchupConfiguration::Mode::OFFLINE}},
    {1, {64, 3, CatchupConfiguration::Mode::OFFLINE}},
    {1, {64, max, CatchupConfiguration::Mode::OFFLINE}},
        {1, {191, 0, CatchupConfiguration::Mode::OFFLINE}},
    {1, {191, 1, CatchupConfiguration::Mode::OFFLINE}},
    {1, {191, 2, CatchupConfiguration::Mode::OFFLINE}},
    {1, {191, 65, CatchupConfiguration::Mode::OFFLINE}},
    {1, {191, 66, CatchupConfiguration::Mode::OFFLINE}},
    {1, {191, 128, CatchupConfiguration::Mode::OFFLINE}},
    {1, {191, max, CatchupConfiguration::Mode::OFFLINE}},
        {1, {320, 0, CatchupConfiguration::Mode::OFFLINE}},
    {1, {320, 1, CatchupConfiguration::Mode::OFFLINE}},
    {1, {320, 2, CatchupConfiguration::Mode::OFFLINE}},
    {1, {320, 3, CatchupConfiguration::Mode::OFFLINE}},
    {1, {320, 66, CatchupConfiguration::Mode::OFFLINE}},
    {1, {320, 67, CatchupConfiguration::Mode::OFFLINE}},
    {1, {320, 319, CatchupConfiguration::Mode::OFFLINE}},
    {1, {320, 320, CatchupConfiguration::Mode::OFFLINE}},
    {1, {320, max, CatchupConfiguration::Mode::OFFLINE}},

            {62, {63, 0, CatchupConfiguration::Mode::OFFLINE}},
    {62, {63, 1, CatchupConfiguration::Mode::OFFLINE}},
    {62, {63, 2, CatchupConfiguration::Mode::OFFLINE}},
    {62, {63, max, CatchupConfiguration::Mode::OFFLINE}},
        {62, {64, 0, CatchupConfiguration::Mode::OFFLINE}},
    {62, {64, 1, CatchupConfiguration::Mode::OFFLINE}},
    {62, {64, max, CatchupConfiguration::Mode::OFFLINE}},
        {62, {319, 0, CatchupConfiguration::Mode::OFFLINE}},
    {62, {319, 1, CatchupConfiguration::Mode::OFFLINE}},
    {62, {319, 65, CatchupConfiguration::Mode::OFFLINE}},
    {62, {319, 66, CatchupConfiguration::Mode::OFFLINE}},
    {62, {319, 129, CatchupConfiguration::Mode::OFFLINE}},
    {62, {319, 130, CatchupConfiguration::Mode::OFFLINE}},
    {62, {319, 319, CatchupConfiguration::Mode::OFFLINE}},
    {62, {319, max, CatchupConfiguration::Mode::OFFLINE}},
        {62, {320, 0, CatchupConfiguration::Mode::OFFLINE}},
    {62, {320, 1, CatchupConfiguration::Mode::OFFLINE}},
    {62, {320, 66, CatchupConfiguration::Mode::OFFLINE}},
    {62, {320, 67, CatchupConfiguration::Mode::OFFLINE}},
    {62, {320, 319, CatchupConfiguration::Mode::OFFLINE}},
    {62, {320, 320, CatchupConfiguration::Mode::OFFLINE}},
    {62, {320, max, CatchupConfiguration::Mode::OFFLINE}},

            {63, {64, 0, CatchupConfiguration::Mode::OFFLINE}},
    {63, {64, 1, CatchupConfiguration::Mode::OFFLINE}},
    {63, {64, max, CatchupConfiguration::Mode::OFFLINE}},
        {63, {319, 0, CatchupConfiguration::Mode::OFFLINE}},
    {63, {319, 1, CatchupConfiguration::Mode::OFFLINE}},
    {63, {319, 65, CatchupConfiguration::Mode::OFFLINE}},
    {63, {319, 66, CatchupConfiguration::Mode::OFFLINE}},
    {63, {319, 129, CatchupConfiguration::Mode::OFFLINE}},
    {63, {319, 130, CatchupConfiguration::Mode::OFFLINE}},
    {63, {319, 319, CatchupConfiguration::Mode::OFFLINE}},
    {63, {319, max, CatchupConfiguration::Mode::OFFLINE}},
        {63, {320, 0, CatchupConfiguration::Mode::OFFLINE}},
    {63, {320, 1, CatchupConfiguration::Mode::OFFLINE}},
    {63, {320, 66, CatchupConfiguration::Mode::OFFLINE}},
    {63, {320, 67, CatchupConfiguration::Mode::OFFLINE}},
    {63, {320, 319, CatchupConfiguration::Mode::OFFLINE}},
    {63, {320, 320, CatchupConfiguration::Mode::OFFLINE}},
    {63, {320, max, CatchupConfiguration::Mode::OFFLINE}},

            {64, {319, 0, CatchupConfiguration::Mode::OFFLINE}},
    {64, {319, 1, CatchupConfiguration::Mode::OFFLINE}},
    {64, {319, 65, CatchupConfiguration::Mode::OFFLINE}},
    {64, {319, 66, CatchupConfiguration::Mode::OFFLINE}},
    {64, {319, 319, CatchupConfiguration::Mode::OFFLINE}},
    {64, {319, max, CatchupConfiguration::Mode::OFFLINE}},
        {64, {320, 0, CatchupConfiguration::Mode::OFFLINE}},
    {64, {320, 1, CatchupConfiguration::Mode::OFFLINE}},
    {64, {320, 66, CatchupConfiguration::Mode::OFFLINE}},
    {64, {320, 67, CatchupConfiguration::Mode::OFFLINE}},
    {64, {320, 319, CatchupConfiguration::Mode::OFFLINE}},
    {64, {320, 320, CatchupConfiguration::Mode::OFFLINE}},
    {64, {320, max, CatchupConfiguration::Mode::OFFLINE}}};
}

TEST_CASE("compute CatchupRange from CatchupConfiguration", "[catchup]")
{
    VirtualClock clock;
    auto app = createTestApplication(clock, getTestConfig());
    auto& historyManager = app->getHistoryManager();

    for (auto const& test : gCatchupRangeCases)
    {
        auto lastClosedLedger = test.first;
        auto configuration = test.second;

        auto name = fmt::format("lcl = {}, to ledger = {}, count = {}",
                                lastClosedLedger, configuration.toLedger(),
                                configuration.count());
        LOG(DEBUG) << "Catchup configuration: " << name;
        {
            auto range =
                CatchupRange{lastClosedLedger, configuration, historyManager};

                        REQUIRE(configuration.toLedger() == range.getLast());

            if (range.mApplyBuckets)
            {
                                REQUIRE(lastClosedLedger == LedgerManager::GENESIS_LEDGER_SEQ);

                                REQUIRE(historyManager.checkpointContainingLedger(
                            range.getBucketApplyLedger()) ==
                        range.getBucketApplyLedger());

                                REQUIRE(range.mLedgers.mFirst ==
                        range.getBucketApplyLedger() + 1);

                                                REQUIRE(range.mLedgers.mCount + 1 >= configuration.count());

                if (std::numeric_limits<uint32_t>::max() -
                        historyManager.getCheckpointFrequency() >=
                    configuration.count())
                {
                                                            REQUIRE(range.mLedgers.mCount <=
                            configuration.count() +
                                historyManager.getCheckpointFrequency());
                }
            }
            else
            {
                                REQUIRE(lastClosedLedger + 1 == range.mLedgers.mFirst);
            }
        }
    }
}

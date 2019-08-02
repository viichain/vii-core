
#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnEntry.h"
#include "ledger/LedgerTxnHeader.h"
#include "ledger/test/LedgerTestUtils.h"
#include "lib/catch.hpp"
#include "main/Application.h"
#include "test/TestUtils.h"
#include "test/test.h"
#include "transactions/TransactionUtils.h"
#include "util/Timer.h"

using namespace viichain;

TEST_CASE("liabilities", "[ledger][liabilities]")
{
    VirtualClock clock;
    auto app = createTestApplication(clock, getTestConfig());
    auto& lm = app->getLedgerManager();
    app->start();

    SECTION("add account selling liabilities")
    {
        auto addSellingLiabilities = [&](uint32_t initNumSubEntries,
                                         int64_t initBalance,
                                         int64_t initSellingLiabilities,
                                         int64_t deltaLiabilities) {
            AccountEntry ae = LedgerTestUtils::generateValidAccountEntry();
            ae.balance = initBalance;
            ae.numSubEntries = initNumSubEntries;
            if (ae.ext.v() < 1)
            {
                ae.ext.v(1);
            }
            ae.ext.v1().liabilities.selling = initSellingLiabilities;
            int64_t initBuyingLiabilities = ae.ext.v1().liabilities.buying;

            LedgerEntry le;
            le.data.type(ACCOUNT);
            le.data.account() = ae;

            LedgerTxn ltx(app->getLedgerTxnRoot());
            auto header = ltx.loadHeader();
            auto acc = ltx.create(le);
            bool res =
                viichain::addSellingLiabilities(header, acc, deltaLiabilities);
            REQUIRE(acc.current().data.account().balance == initBalance);
            REQUIRE(getBuyingLiabilities(header, acc) == initBuyingLiabilities);
            if (res)
            {
                REQUIRE(getSellingLiabilities(header, acc) ==
                        initSellingLiabilities + deltaLiabilities);
            }
            else
            {
                REQUIRE(getSellingLiabilities(header, acc) ==
                        initSellingLiabilities);
                REQUIRE(acc.current() == le);
            }
            return res;
        };
        auto addSellingLiabilitiesUninitialized =
            [&](uint32_t initNumSubEntries, int64_t initBalance,
                int64_t deltaLiabilities) {
                AccountEntry ae = LedgerTestUtils::generateValidAccountEntry();
                ae.balance = initBalance;
                ae.numSubEntries = initNumSubEntries;
                ae.ext.v(0);

                LedgerEntry le;
                le.data.type(ACCOUNT);
                le.data.account() = ae;

                LedgerTxn ltx(app->getLedgerTxnRoot());
                auto header = ltx.loadHeader();
                auto acc = ltx.create(le);
                bool res = viichain::addSellingLiabilities(header, acc,
                                                          deltaLiabilities);
                REQUIRE(acc.current().data.account().balance == initBalance);
                REQUIRE(getBuyingLiabilities(header, acc) == 0);
                if (res)
                {
                    REQUIRE(acc.current().data.account().ext.v() ==
                            ((deltaLiabilities != 0) ? 1 : 0));
                    REQUIRE(getSellingLiabilities(header, acc) ==
                            deltaLiabilities);
                }
                else
                {
                    REQUIRE(getSellingLiabilities(header, acc) == 0);
                    REQUIRE(acc.current() == le);
                }
                return res;
            };

        for_versions_from(10, *app, [&] {
            SECTION("uninitialized liabilities")
            {
                                REQUIRE(!addSellingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0), 1));

                                REQUIRE(addSellingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0), 0));

                                REQUIRE(addSellingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0) + 1, 1));
            }

            SECTION("below reserve")
            {
                                REQUIRE(addSellingLiabilities(0, lm.getLastMinBalance(0) - 1, 0,
                                              0));

                                REQUIRE(!addSellingLiabilities(0, lm.getLastMinBalance(0) - 1,
                                               0, 1));

                                            }

            SECTION("cannot make liabilities negative")
            {
                                REQUIRE(
                    addSellingLiabilities(0, lm.getLastMinBalance(0), 0, 0));
                REQUIRE(addSellingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0), 0));
                REQUIRE(
                    !addSellingLiabilities(0, lm.getLastMinBalance(0), 0, -1));
                REQUIRE(!addSellingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0), -1));

                                REQUIRE(addSellingLiabilities(0, lm.getLastMinBalance(0) + 1, 0,
                                              0));
                REQUIRE(addSellingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0) + 1, 0));
                REQUIRE(!addSellingLiabilities(0, lm.getLastMinBalance(0) + 1,
                                               0, -1));
                REQUIRE(!addSellingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0) + 1, -1));

                                REQUIRE(addSellingLiabilities(0, lm.getLastMinBalance(0) + 1, 1,
                                              -1));
                REQUIRE(!addSellingLiabilities(0, lm.getLastMinBalance(0) + 1,
                                               1, -2));

                                REQUIRE(addSellingLiabilities(0, lm.getLastMinBalance(0) + 2, 1,
                                              -1));
                REQUIRE(!addSellingLiabilities(0, lm.getLastMinBalance(0) + 2,
                                               1, -2));
            }

            SECTION("cannot increase liabilities above balance minus reserve")
            {
                                REQUIRE(
                    addSellingLiabilities(0, lm.getLastMinBalance(0), 0, 0));
                REQUIRE(addSellingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0), 0));
                REQUIRE(
                    !addSellingLiabilities(0, lm.getLastMinBalance(0), 0, 1));
                REQUIRE(!addSellingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0), 1));

                                REQUIRE(addSellingLiabilities(0, lm.getLastMinBalance(0) + 1, 0,
                                              1));
                REQUIRE(addSellingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0) + 1, 1));
                REQUIRE(!addSellingLiabilities(0, lm.getLastMinBalance(0) + 1,
                                               0, 2));
                REQUIRE(!addSellingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0) + 1, 2));

                                REQUIRE(addSellingLiabilities(0, lm.getLastMinBalance(0) + 1, 1,
                                              0));
                REQUIRE(!addSellingLiabilities(0, lm.getLastMinBalance(0) + 1,
                                               1, 1));

                                REQUIRE(addSellingLiabilities(0, lm.getLastMinBalance(0) + 2, 1,
                                              1));
                REQUIRE(!addSellingLiabilities(0, lm.getLastMinBalance(0) + 2,
                                               1, 2));
            }

            SECTION("limiting values")
            {
                                REQUIRE(addSellingLiabilities(
                    0, INT64_MAX, 0, INT64_MAX - lm.getLastMinBalance(0)));
                REQUIRE(!addSellingLiabilities(
                    0, INT64_MAX, 0, INT64_MAX - lm.getLastMinBalance(0) + 1));
                REQUIRE(!addSellingLiabilities(
                    0, INT64_MAX, 1, INT64_MAX - lm.getLastMinBalance(0)));

                                REQUIRE(addSellingLiabilities(
                    0, INT64_MAX, INT64_MAX - lm.getLastMinBalance(0), -1));
                REQUIRE(addSellingLiabilities(
                    0, INT64_MAX, INT64_MAX - lm.getLastMinBalance(0),
                    lm.getLastMinBalance(0) - INT64_MAX));
            }
        });
    }

    SECTION("add account buying liabilities")
    {
        auto addBuyingLiabilities = [&](uint32_t initNumSubEntries,
                                        int64_t initBalance,
                                        int64_t initBuyingLiabilities,
                                        int64_t deltaLiabilities) {
            AccountEntry ae = LedgerTestUtils::generateValidAccountEntry();
            ae.balance = initBalance;
            ae.numSubEntries = initNumSubEntries;
            if (ae.ext.v() < 1)
            {
                ae.ext.v(1);
            }
            ae.ext.v1().liabilities.buying = initBuyingLiabilities;
            int64_t initSellingLiabilities = ae.ext.v1().liabilities.selling;

            LedgerEntry le;
            le.data.type(ACCOUNT);
            le.data.account() = ae;

            LedgerTxn ltx(app->getLedgerTxnRoot());
            auto header = ltx.loadHeader();
            auto acc = ltx.create(le);
            bool res =
                viichain::addBuyingLiabilities(header, acc, deltaLiabilities);
            REQUIRE(acc.current().data.account().balance == initBalance);
            REQUIRE(getSellingLiabilities(header, acc) ==
                    initSellingLiabilities);
            if (res)
            {
                REQUIRE(getBuyingLiabilities(header, acc) ==
                        initBuyingLiabilities + deltaLiabilities);
            }
            else
            {
                REQUIRE(getBuyingLiabilities(header, acc) ==
                        initBuyingLiabilities);
                REQUIRE(acc.current() == le);
            }
            return res;
        };
        auto addBuyingLiabilitiesUninitialized = [&](uint32_t initNumSubEntries,
                                                     int64_t initBalance,
                                                     int64_t deltaLiabilities) {
            AccountEntry ae = LedgerTestUtils::generateValidAccountEntry();
            ae.balance = initBalance;
            ae.numSubEntries = initNumSubEntries;
            ae.ext.v(0);

            LedgerEntry le;
            le.data.type(ACCOUNT);
            le.data.account() = ae;

            LedgerTxn ltx(app->getLedgerTxnRoot());
            auto header = ltx.loadHeader();
            auto acc = ltx.create(le);
            bool res =
                viichain::addBuyingLiabilities(header, acc, deltaLiabilities);
            REQUIRE(acc.current().data.account().balance == initBalance);
            REQUIRE(getSellingLiabilities(header, acc) == 0);
            if (res)
            {
                REQUIRE(acc.current().data.account().ext.v() ==
                        ((deltaLiabilities != 0) ? 1 : 0));
                REQUIRE(getBuyingLiabilities(header, acc) == deltaLiabilities);
            }
            else
            {
                REQUIRE(getBuyingLiabilities(header, acc) == 0);
                REQUIRE(acc.current() == le);
            }
            return res;
        };

        for_versions_from(10, *app, [&] {
            SECTION("uninitialized liabilities")
            {
                                REQUIRE(!addBuyingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0),
                    INT64_MAX - (lm.getLastMinBalance(0) - 1)));

                                REQUIRE(addBuyingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0), 0));

                                REQUIRE(addBuyingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0), 1));
            }

            SECTION("below reserve")
            {
                                REQUIRE(addBuyingLiabilities(0, lm.getLastMinBalance(0) - 1, 1,
                                             -1));

                                REQUIRE(
                    addBuyingLiabilities(0, lm.getLastMinBalance(0) - 1, 0, 0));
                REQUIRE(
                    addBuyingLiabilities(0, lm.getLastMinBalance(0) - 1, 1, 0));

                                REQUIRE(
                    addBuyingLiabilities(0, lm.getLastMinBalance(0) - 1, 0, 1));
                REQUIRE(
                    addBuyingLiabilities(0, lm.getLastMinBalance(0) - 1, 1, 1));
            }

            SECTION("cannot make liabilities negative")
            {
                                REQUIRE(addBuyingLiabilities(0, lm.getLastMinBalance(0), 0, 0));
                REQUIRE(addBuyingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0), 0));
                REQUIRE(
                    !addBuyingLiabilities(0, lm.getLastMinBalance(0), 0, -1));
                REQUIRE(!addBuyingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0), -1));

                                REQUIRE(
                    addBuyingLiabilities(0, lm.getLastMinBalance(0), 1, -1));
                REQUIRE(
                    !addBuyingLiabilities(0, lm.getLastMinBalance(0), 1, -2));
            }

            SECTION("cannot increase liabilities above INT64_MAX minus balance")
            {
                                REQUIRE(addBuyingLiabilities(
                    0, lm.getLastMinBalance(0) - 1, 0,
                    INT64_MAX - (lm.getLastMinBalance(0) - 1)));
                REQUIRE(addBuyingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0) - 1,
                    INT64_MAX - (lm.getLastMinBalance(0) - 1)));
                REQUIRE(!addBuyingLiabilities(
                    0, lm.getLastMinBalance(0) - 1, 0,
                    INT64_MAX - (lm.getLastMinBalance(0) - 2)));
                REQUIRE(!addBuyingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0) - 1,
                    INT64_MAX - (lm.getLastMinBalance(0) - 2)));

                                REQUIRE(
                    addBuyingLiabilities(0, lm.getLastMinBalance(0) - 1, 1,
                                         INT64_MAX - lm.getLastMinBalance(0)));
                REQUIRE(!addBuyingLiabilities(
                    0, lm.getLastMinBalance(0) - 1, 1,
                    INT64_MAX - (lm.getLastMinBalance(0) - 1)));

                                REQUIRE(
                    addBuyingLiabilities(0, lm.getLastMinBalance(0), 0,
                                         INT64_MAX - lm.getLastMinBalance(0)));
                REQUIRE(addBuyingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0),
                    INT64_MAX - lm.getLastMinBalance(0)));
                REQUIRE(!addBuyingLiabilities(
                    0, lm.getLastMinBalance(0), 0,
                    INT64_MAX - (lm.getLastMinBalance(0) - 1)));
                REQUIRE(!addBuyingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0),
                    INT64_MAX - (lm.getLastMinBalance(0) - 1)));

                                REQUIRE(addBuyingLiabilities(
                    0, lm.getLastMinBalance(0), 1,
                    INT64_MAX - (lm.getLastMinBalance(0) + 1)));
                REQUIRE(
                    !addBuyingLiabilities(0, lm.getLastMinBalance(0), 1,
                                          INT64_MAX - lm.getLastMinBalance(0)));

                                REQUIRE(addBuyingLiabilities(
                    0, lm.getLastMinBalance(0) + 1, 0,
                    INT64_MAX - (lm.getLastMinBalance(0) + 1)));
                REQUIRE(addBuyingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0) + 1,
                    INT64_MAX - (lm.getLastMinBalance(0) + 1)));
                REQUIRE(
                    !addBuyingLiabilities(0, lm.getLastMinBalance(0) + 1, 0,
                                          INT64_MAX - lm.getLastMinBalance(0)));
                REQUIRE(!addBuyingLiabilitiesUninitialized(
                    0, lm.getLastMinBalance(0) + 1,
                    INT64_MAX - lm.getLastMinBalance(0)));

                                REQUIRE(addBuyingLiabilities(
                    0, lm.getLastMinBalance(0) + 1, 1,
                    INT64_MAX - (lm.getLastMinBalance(0) + 2)));
                REQUIRE(!addBuyingLiabilities(
                    0, lm.getLastMinBalance(0) + 1, 1,
                    INT64_MAX - (lm.getLastMinBalance(0) + 1)));
            }

            SECTION("limiting values")
            {
                REQUIRE(!addBuyingLiabilities(0, INT64_MAX, 0, 1));
                REQUIRE(addBuyingLiabilities(0, INT64_MAX - 1, 0, 1));

                REQUIRE(!addBuyingLiabilities(
                    0, lm.getLastMinBalance(0),
                    INT64_MAX - lm.getLastMinBalance(0), 1));
                REQUIRE(addBuyingLiabilities(
                    0, lm.getLastMinBalance(0),
                    INT64_MAX - lm.getLastMinBalance(0) - 1, 1));

                REQUIRE(!addBuyingLiabilities(UINT32_MAX, INT64_MAX / 2 + 1,
                                              INT64_MAX / 2, 1));
                REQUIRE(!addBuyingLiabilities(UINT32_MAX, INT64_MAX / 2,
                                              INT64_MAX / 2 + 1, 1));
                REQUIRE(addBuyingLiabilities(UINT32_MAX, INT64_MAX / 2,
                                             INT64_MAX / 2, 1));
            }
        });
    }

    SECTION("add trustline selling liabilities")
    {
        auto addSellingLiabilities = [&](int64_t initLimit, int64_t initBalance,
                                         int64_t initSellingLiabilities,
                                         int64_t deltaLiabilities) {
            TrustLineEntry tl = LedgerTestUtils::generateValidTrustLineEntry();
            tl.flags = AUTHORIZED_FLAG;
            tl.balance = initBalance;
            tl.limit = initLimit;
            if (tl.ext.v() < 1)
            {
                tl.ext.v(1);
            }
            tl.ext.v1().liabilities.selling = initSellingLiabilities;
            int64_t initBuyingLiabilities = tl.ext.v1().liabilities.buying;

            LedgerEntry le;
            le.data.type(TRUSTLINE);
            le.data.trustLine() = tl;

            LedgerTxn ltx(app->getLedgerTxnRoot());
            auto header = ltx.loadHeader();
            auto trust = ltx.create(le);
            bool res =
                viichain::addSellingLiabilities(header, trust, deltaLiabilities);
            REQUIRE(trust.current().data.trustLine().balance == initBalance);
            REQUIRE(getBuyingLiabilities(header, trust) ==
                    initBuyingLiabilities);
            if (res)
            {
                REQUIRE(getSellingLiabilities(header, trust) ==
                        initSellingLiabilities + deltaLiabilities);
            }
            else
            {
                REQUIRE(getSellingLiabilities(header, trust) ==
                        initSellingLiabilities);
                REQUIRE(trust.current() == le);
            }
            return res;
        };
        auto addSellingLiabilitiesUninitialized =
            [&](int64_t initLimit, int64_t initBalance,
                int64_t deltaLiabilities) {
                TrustLineEntry tl =
                    LedgerTestUtils::generateValidTrustLineEntry();
                tl.flags = AUTHORIZED_FLAG;
                tl.balance = initBalance;
                tl.limit = initLimit;
                tl.ext.v(0);

                LedgerEntry le;
                le.data.type(TRUSTLINE);
                le.data.trustLine() = tl;

                LedgerTxn ltx(app->getLedgerTxnRoot());
                auto header = ltx.loadHeader();
                auto trust = ltx.create(le);
                bool res = viichain::addSellingLiabilities(header, trust,
                                                          deltaLiabilities);
                REQUIRE(trust.current().data.trustLine().balance ==
                        initBalance);
                REQUIRE(getBuyingLiabilities(header, trust) == 0);
                if (res)
                {
                    REQUIRE(getSellingLiabilities(header, trust) ==
                            deltaLiabilities);
                }
                else
                {
                    REQUIRE(getSellingLiabilities(header, trust) == 0);
                    REQUIRE(trust.current() == le);
                }
                return res;
            };

        for_versions_from(10, *app, [&] {
            SECTION("uninitialized liabilities")
            {
                                REQUIRE(!addSellingLiabilitiesUninitialized(1, 0, 1));

                                REQUIRE(addSellingLiabilitiesUninitialized(1, 1, 0));

                                REQUIRE(addSellingLiabilitiesUninitialized(1, 1, 1));
            }

            SECTION("cannot make liabilities negative")
            {
                                REQUIRE(addSellingLiabilities(1, 0, 0, 0));
                REQUIRE(addSellingLiabilitiesUninitialized(1, 0, 0));
                REQUIRE(!addSellingLiabilities(1, 0, 0, -1));
                REQUIRE(!addSellingLiabilitiesUninitialized(1, 1, -1));

                                REQUIRE(addSellingLiabilities(1, 0, 1, -1));
                REQUIRE(!addSellingLiabilities(1, 0, 1, -2));
            }

            SECTION("cannot increase liabilities above balance")
            {
                                REQUIRE(addSellingLiabilities(2, 1, 0, 1));
                REQUIRE(addSellingLiabilitiesUninitialized(2, 1, 1));
                REQUIRE(!addSellingLiabilities(2, 1, 0, 2));
                REQUIRE(!addSellingLiabilitiesUninitialized(2, 1, 2));

                                REQUIRE(addSellingLiabilities(2, 2, 1, 1));
                REQUIRE(!addSellingLiabilities(2, 2, 1, 2));

                                REQUIRE(addSellingLiabilities(2, 0, 0, 0));
                REQUIRE(addSellingLiabilitiesUninitialized(2, 0, 0));
                REQUIRE(!addSellingLiabilities(2, 0, 0, 1));
                REQUIRE(!addSellingLiabilitiesUninitialized(2, 0, 1));

                                REQUIRE(addSellingLiabilities(2, 2, 2, 0));
                REQUIRE(!addSellingLiabilities(2, 2, 2, 1));
            }

            SECTION("limiting values")
            {
                REQUIRE(
                    addSellingLiabilities(INT64_MAX, INT64_MAX, 0, INT64_MAX));
                REQUIRE(!addSellingLiabilities(INT64_MAX, INT64_MAX - 1, 0,
                                               INT64_MAX));
                REQUIRE(addSellingLiabilities(INT64_MAX, INT64_MAX - 1, 0,
                                              INT64_MAX - 1));

                REQUIRE(
                    !addSellingLiabilities(INT64_MAX, INT64_MAX, INT64_MAX, 1));
                REQUIRE(addSellingLiabilities(INT64_MAX, INT64_MAX,
                                              INT64_MAX - 1, 1));
            }
        });
    }

    SECTION("add trustline buying liabilities")
    {
        auto addBuyingLiabilities = [&](int64_t initLimit, int64_t initBalance,
                                        int64_t initBuyingLiabilities,
                                        int64_t deltaLiabilities) {
            TrustLineEntry tl = LedgerTestUtils::generateValidTrustLineEntry();
            tl.flags = AUTHORIZED_FLAG;
            tl.balance = initBalance;
            tl.limit = initLimit;
            if (tl.ext.v() < 1)
            {
                tl.ext.v(1);
            }
            tl.ext.v1().liabilities.buying = initBuyingLiabilities;
            int64_t initSellingLiabilities = tl.ext.v1().liabilities.selling;

            LedgerEntry le;
            le.data.type(TRUSTLINE);
            le.data.trustLine() = tl;

            LedgerTxn ltx(app->getLedgerTxnRoot());
            auto header = ltx.loadHeader();
            auto trust = ltx.create(le);
            bool res =
                viichain::addBuyingLiabilities(header, trust, deltaLiabilities);
            REQUIRE(trust.current().data.trustLine().balance == initBalance);
            REQUIRE(getSellingLiabilities(header, trust) ==
                    initSellingLiabilities);
            if (res)
            {
                REQUIRE(getBuyingLiabilities(header, trust) ==
                        initBuyingLiabilities + deltaLiabilities);
            }
            else
            {
                REQUIRE(getBuyingLiabilities(header, trust) ==
                        initBuyingLiabilities);
                REQUIRE(trust.current() == le);
            }
            return res;
        };
        auto addBuyingLiabilitiesUninitialized = [&](int64_t initLimit,
                                                     int64_t initBalance,
                                                     int64_t deltaLiabilities) {
            TrustLineEntry tl = LedgerTestUtils::generateValidTrustLineEntry();
            tl.flags = AUTHORIZED_FLAG;
            tl.balance = initBalance;
            tl.limit = initLimit;
            tl.ext.v(0);

            LedgerEntry le;
            le.data.type(TRUSTLINE);
            le.data.trustLine() = tl;

            LedgerTxn ltx(app->getLedgerTxnRoot());
            auto header = ltx.loadHeader();
            auto trust = ltx.create(le);
            bool res =
                viichain::addBuyingLiabilities(header, trust, deltaLiabilities);
            REQUIRE(trust.current().data.trustLine().balance == initBalance);
            REQUIRE(getSellingLiabilities(header, trust) == 0);
            if (res)
            {
                REQUIRE(getBuyingLiabilities(header, trust) ==
                        deltaLiabilities);
            }
            else
            {
                REQUIRE(getBuyingLiabilities(header, trust) == 0);
                REQUIRE(trust.current() == le);
            }
            return res;
        };

        for_versions_from(10, *app, [&] {
            SECTION("uninitialized liabilities")
            {
                                REQUIRE(!addBuyingLiabilitiesUninitialized(1, 0, 2));

                                REQUIRE(addBuyingLiabilitiesUninitialized(1, 0, 0));

                                REQUIRE(addBuyingLiabilitiesUninitialized(1, 0, 1));
            }

            SECTION("cannot make liabilities negative")
            {
                                REQUIRE(addBuyingLiabilities(1, 0, 0, 0));
                REQUIRE(addBuyingLiabilitiesUninitialized(1, 0, 0));
                REQUIRE(!addBuyingLiabilities(1, 0, 0, -1));
                REQUIRE(!addBuyingLiabilitiesUninitialized(1, 1, -1));

                                REQUIRE(addBuyingLiabilities(1, 0, 1, -1));
                REQUIRE(!addBuyingLiabilities(1, 0, 1, -2));
            }

            SECTION("cannot increase liabilities above limit minus balance")
            {
                                REQUIRE(addBuyingLiabilities(2, 1, 0, 1));
                REQUIRE(addBuyingLiabilitiesUninitialized(2, 1, 1));
                REQUIRE(!addBuyingLiabilities(2, 1, 0, 2));
                REQUIRE(!addBuyingLiabilitiesUninitialized(2, 1, 2));

                                REQUIRE(addBuyingLiabilities(3, 1, 1, 1));
                REQUIRE(!addBuyingLiabilities(3, 1, 1, 2));

                                REQUIRE(addBuyingLiabilities(2, 2, 0, 0));
                REQUIRE(addBuyingLiabilitiesUninitialized(2, 2, 0));
                REQUIRE(!addBuyingLiabilities(2, 2, 0, 1));
                REQUIRE(!addBuyingLiabilitiesUninitialized(2, 2, 1));

                                REQUIRE(addBuyingLiabilities(3, 2, 1, 0));
                REQUIRE(!addBuyingLiabilities(3, 2, 1, 1));
            }

            SECTION("limiting values")
            {
                REQUIRE(!addBuyingLiabilities(INT64_MAX, INT64_MAX, 0, 1));
                REQUIRE(addBuyingLiabilities(INT64_MAX, INT64_MAX - 1, 0, 1));

                REQUIRE(!addBuyingLiabilities(INT64_MAX, 0, INT64_MAX, 1));
                REQUIRE(addBuyingLiabilities(INT64_MAX, 0, INT64_MAX - 1, 1));

                REQUIRE(!addBuyingLiabilities(INT64_MAX, INT64_MAX / 2 + 1,
                                              INT64_MAX / 2, 1));
                REQUIRE(!addBuyingLiabilities(INT64_MAX, INT64_MAX / 2,
                                              INT64_MAX / 2 + 1, 1));
                REQUIRE(addBuyingLiabilities(INT64_MAX, INT64_MAX / 2,
                                             INT64_MAX / 2, 1));
            }
        });
    }
}

TEST_CASE("balance with liabilities", "[ledger][liabilities]")
{
    VirtualClock clock;
    auto app = createTestApplication(clock, getTestConfig());
    auto& lm = app->getLedgerManager();
    app->start();

    SECTION("account add balance")
    {
        auto addBalance = [&](uint32_t initNumSubEntries, int64_t initBalance,
                              Liabilities initLiabilities,
                              int64_t deltaBalance) {
            AccountEntry ae = LedgerTestUtils::generateValidAccountEntry();
            ae.balance = initBalance;
            ae.numSubEntries = initNumSubEntries;
            ae.ext.v(1);
            ae.ext.v1().liabilities = initLiabilities;

            LedgerEntry le;
            le.data.type(ACCOUNT);
            le.data.account() = ae;

            LedgerTxn ltx(app->getLedgerTxnRoot());
            auto header = ltx.loadHeader();
            auto acc = ltx.create(le);
            bool res = viichain::addBalance(header, acc, deltaBalance);
            REQUIRE(getSellingLiabilities(header, acc) ==
                    initLiabilities.selling);
            REQUIRE(getBuyingLiabilities(header, acc) ==
                    initLiabilities.buying);
            if (res)
            {
                REQUIRE(acc.current().data.account().balance ==
                        initBalance + deltaBalance);
            }
            else
            {
                REQUIRE(acc.current() == le);
            }
            return res;
        };

        for_versions_from(10, *app, [&] {
            SECTION("can increase balance from below minimum")
            {
                                REQUIRE(addBalance(0, lm.getLastMinBalance(0) - 1,
                                   Liabilities{0, 0}, 0));

                                REQUIRE(addBalance(0, lm.getLastMinBalance(0) - 2,
                                   Liabilities{0, 0}, 1));

                                REQUIRE(addBalance(0, lm.getLastMinBalance(0) - 1,
                                   Liabilities{0, 0}, 1));

                                REQUIRE(addBalance(0, lm.getLastMinBalance(0) - 1,
                                   Liabilities{0, 0}, 2));
            }

            SECTION("cannot decrease balance below reserve plus selling "
                    "liabilities")
            {
                                REQUIRE(addBalance(0, lm.getLastMinBalance(0) - 1,
                                   Liabilities{0, 0}, 0));
                REQUIRE(!addBalance(0, lm.getLastMinBalance(0) - 1,
                                    Liabilities{0, 0}, -1));

                                REQUIRE(addBalance(0, lm.getLastMinBalance(0),
                                   Liabilities{0, 0}, 0));
                REQUIRE(!addBalance(0, lm.getLastMinBalance(0),
                                    Liabilities{0, 0}, -1));

                                REQUIRE(addBalance(0, lm.getLastMinBalance(0) + 1,
                                   Liabilities{0, 0}, -1));
                REQUIRE(!addBalance(0, lm.getLastMinBalance(0) + 1,
                                    Liabilities{0, 0}, -2));

                                REQUIRE(addBalance(0, lm.getLastMinBalance(0) + 1,
                                   Liabilities{0, 1}, 0));
                REQUIRE(!addBalance(0, lm.getLastMinBalance(0) + 1,
                                    Liabilities{0, 1}, -1));
            }

            SECTION("cannot increase balance above INT64_MAX minus buying "
                    "liabilities")
            {
                                REQUIRE(addBalance(0, INT64_MAX, Liabilities{0, 0}, 0));
                REQUIRE(!addBalance(0, INT64_MAX, Liabilities{0, 0}, 1));

                                REQUIRE(addBalance(0, INT64_MAX - 1, Liabilities{0, 0}, 1));
                REQUIRE(!addBalance(0, INT64_MAX - 1, Liabilities{0, 0}, 2));

                                REQUIRE(addBalance(0, INT64_MAX - 1, Liabilities{1, 0}, 0));
                REQUIRE(!addBalance(0, INT64_MAX - 1, Liabilities{1, 0}, 1));
            }
        });
    }

    SECTION("account add subentries")
    {
        auto addSubEntries = [&](uint32_t initNumSubEntries,
                                 int64_t initBalance,
                                 int64_t initSellingLiabilities,
                                 int32_t deltaNumSubEntries) {
            AccountEntry ae = LedgerTestUtils::generateValidAccountEntry();
            ae.balance = initBalance;
            ae.numSubEntries = initNumSubEntries;
            if (ae.ext.v() == 0)
            {
                ae.ext.v(1);
            }
            ae.ext.v1().liabilities.selling = initSellingLiabilities;
            int64_t initBuyingLiabilities = ae.ext.v1().liabilities.buying;

            LedgerEntry le;
            le.data.type(ACCOUNT);
            le.data.account() = ae;

            LedgerTxn ltx(app->getLedgerTxnRoot());
            auto header = ltx.loadHeader();
            auto acc = ltx.create(le);
            bool res =
                viichain::addNumEntries(header, acc, deltaNumSubEntries) ==
                AddSubentryResult::SUCCESS;
            REQUIRE(getSellingLiabilities(header, acc) ==
                    initSellingLiabilities);
            REQUIRE(getBuyingLiabilities(header, acc) == initBuyingLiabilities);
            REQUIRE(acc.current().data.account().balance == initBalance);
            if (res)
            {
                if (deltaNumSubEntries > 0)
                {
                    REQUIRE(getAvailableBalance(header, acc) >= 0);
                }
                REQUIRE(acc.current().data.account().numSubEntries ==
                        initNumSubEntries + deltaNumSubEntries);
            }
            else
            {
                REQUIRE(acc.current() == le);
            }
            return res;
        };

        for_versions_from(10, *app, [&] {
            SECTION("can decrease sub entries when below min balance")
            {
                                REQUIRE(addSubEntries(1, 0, 0, -1));
                REQUIRE(addSubEntries(1, lm.getLastMinBalance(0) - 1, 0, -1));

                                REQUIRE(addSubEntries(1, lm.getLastMinBalance(0), 0, -1));

                                REQUIRE(addSubEntries(1, lm.getLastMinBalance(1) - 1, 0, -1));
                REQUIRE(addSubEntries(1, lm.getLastMinBalance(0) + 1, 0, -1));
            }

            SECTION("cannot add sub entry without sufficient balance")
            {
                                REQUIRE(!addSubEntries(0, lm.getLastMinBalance(0) - 1, 0, 1));

                                REQUIRE(!addSubEntries(0, lm.getLastMinBalance(0), 0, 1));

                                REQUIRE(!addSubEntries(0, lm.getLastMinBalance(0) + 1, 0, 1));
                REQUIRE(!addSubEntries(0, lm.getLastMinBalance(1) - 1, 0, 1));

                                REQUIRE(!addSubEntries(0, lm.getLastMinBalance(0) + 2, 1, 1));
                REQUIRE(!addSubEntries(0, lm.getLastMinBalance(1), 1, 1));

                                REQUIRE(addSubEntries(0, lm.getLastMinBalance(1), 0, 1));

                                REQUIRE(addSubEntries(0, lm.getLastMinBalance(1) + 1, 1, 1));

                                REQUIRE(addSubEntries(0, lm.getLastMinBalance(1) + 1, 0, 1));

                                REQUIRE(addSubEntries(0, lm.getLastMinBalance(1) + 1, 1, 1));
                REQUIRE(!addSubEntries(0, lm.getLastMinBalance(1) + 1, 2, 1));
            }
        });
    }

    SECTION("trustline add balance")
    {
        auto addBalance = [&](int64_t initLimit, int64_t initBalance,
                              Liabilities initLiabilities,
                              int64_t deltaBalance) {
            TrustLineEntry tl = LedgerTestUtils::generateValidTrustLineEntry();
            tl.balance = initBalance;
            tl.limit = initLimit;
            tl.flags = AUTHORIZED_FLAG;
            tl.ext.v(1);
            tl.ext.v1().liabilities = initLiabilities;

            LedgerEntry le;
            le.data.type(TRUSTLINE);
            le.data.trustLine() = tl;

            LedgerTxn ltx(app->getLedgerTxnRoot());
            auto header = ltx.loadHeader();
            auto trust = ltx.create(le);
            bool res = viichain::addBalance(header, trust, deltaBalance);
            REQUIRE(getSellingLiabilities(header, trust) ==
                    initLiabilities.selling);
            REQUIRE(getBuyingLiabilities(header, trust) ==
                    initLiabilities.buying);
            if (res)
            {
                REQUIRE(trust.current().data.trustLine().balance ==
                        initBalance + deltaBalance);
            }
            else
            {
                REQUIRE(trust.current() == le);
            }
            return res;
        };

        for_versions_from(10, *app, [&] {
            SECTION("cannot decrease balance below selling liabilities")
            {
                                REQUIRE(addBalance(2, 0, Liabilities{0, 0}, 0));
                REQUIRE(!addBalance(2, 0, Liabilities{0, 0}, -1));

                                REQUIRE(addBalance(2, 1, Liabilities{0, 0}, -1));
                REQUIRE(!addBalance(2, 1, Liabilities{0, 0}, -2));

                                REQUIRE(addBalance(2, 2, Liabilities{0, 1}, -1));
                REQUIRE(!addBalance(2, 2, Liabilities{0, 1}, -2));
            }

            SECTION(
                "cannot increase balance above limit minus buying liabilities")
            {
                                REQUIRE(addBalance(2, 2, Liabilities{0, 0}, 0));
                REQUIRE(!addBalance(2, 2, Liabilities{0, 0}, 1));

                                REQUIRE(addBalance(2, 1, Liabilities{0, 0}, 1));
                REQUIRE(!addBalance(2, 1, Liabilities{0, 0}, 2));

                                REQUIRE(addBalance(3, 1, Liabilities{1, 0}, 1));
                REQUIRE(!addBalance(3, 1, Liabilities{1, 0}, 2));
            }
        });
    }
}

TEST_CASE("available balance and limit", "[ledger][liabilities]")
{
    VirtualClock clock;
    auto app = createTestApplication(clock, getTestConfig());
    auto& lm = app->getLedgerManager();
    app->start();

    SECTION("account available balance")
    {
        auto checkAvailableBalance = [&](uint32_t initNumSubEntries,
                                         int64_t initBalance,
                                         int64_t initSellingLiabilities) {
            AccountEntry ae = LedgerTestUtils::generateValidAccountEntry();
            ae.balance = initBalance;
            ae.numSubEntries = initNumSubEntries;
            if (ae.ext.v() < 1)
            {
                ae.ext.v(1);
            }
            ae.ext.v1().liabilities = Liabilities{0, initSellingLiabilities};

            LedgerEntry le;
            le.data.type(ACCOUNT);
            le.data.account() = ae;

            LedgerTxn ltx(app->getLedgerTxnRoot());
            auto header = ltx.loadHeader();
            auto acc = ltx.create(le);
            auto availableBalance =
                std::max({int64_t(0), getAvailableBalance(header, acc)});
            REQUIRE(!viichain::addBalance(header, acc, -availableBalance - 1));
            REQUIRE(viichain::addBalance(header, acc, -availableBalance));
        };

        for_versions_from(10, *app, [&] {
                        checkAvailableBalance(0, 0, 0);
            checkAvailableBalance(0, lm.getLastMinBalance(0) - 1, 0);

                        checkAvailableBalance(0, lm.getLastMinBalance(0), 0);

                        checkAvailableBalance(0, lm.getLastMinBalance(0) + 1, 0);
            checkAvailableBalance(0, INT64_MAX, 0);

                        checkAvailableBalance(0, lm.getLastMinBalance(0) + 1, 1);
            checkAvailableBalance(0, INT64_MAX,
                                  INT64_MAX - lm.getLastMinBalance(0));

                        checkAvailableBalance(0, lm.getLastMinBalance(0) + 2, 1);
            checkAvailableBalance(0, INT64_MAX,
                                  INT64_MAX - lm.getLastMinBalance(0) - 1);
        });
    }

    SECTION("account available limit")
    {
        auto checkAvailableLimit = [&](uint32_t initNumSubEntries,
                                       int64_t initBalance,
                                       int64_t initBuyingLiabilities) {
            AccountEntry ae = LedgerTestUtils::generateValidAccountEntry();
            ae.balance = initBalance;
            ae.numSubEntries = initNumSubEntries;
            if (ae.ext.v() < 1)
            {
                ae.ext.v(1);
            }
            ae.ext.v1().liabilities = Liabilities{initBuyingLiabilities, 0};

            LedgerEntry le;
            le.data.type(ACCOUNT);
            le.data.account() = ae;

            LedgerTxn ltx(app->getLedgerTxnRoot());
            auto header = ltx.loadHeader();
            auto acc = ltx.create(le);
            auto availableLimit =
                std::max({int64_t(0), getMaxAmountReceive(header, acc)});
            if (availableLimit < INT64_MAX)
            {
                REQUIRE(!viichain::addBalance(header, acc, availableLimit + 1));
            }
            REQUIRE(viichain::addBalance(header, acc, availableLimit));
        };

        for_versions_from(10, *app, [&] {
                        checkAvailableLimit(0, 0, 0);
            checkAvailableLimit(0, lm.getLastMinBalance(0) - 1, 0);

                        checkAvailableLimit(0, 0, INT64_MAX);
            checkAvailableLimit(0, lm.getLastMinBalance(0) - 1,
                                INT64_MAX - lm.getLastMinBalance(0) + 1);

                        checkAvailableLimit(0, 0, INT64_MAX - 1);
            checkAvailableLimit(0, lm.getLastMinBalance(0) - 1,
                                INT64_MAX - lm.getLastMinBalance(0));

                        checkAvailableLimit(0, lm.getLastMinBalance(0), 0);

                        checkAvailableLimit(0, lm.getLastMinBalance(0),
                                INT64_MAX - lm.getLastMinBalance(0));

                        checkAvailableLimit(0, lm.getLastMinBalance(0),
                                INT64_MAX - lm.getLastMinBalance(0) - 1);

                        checkAvailableLimit(0, lm.getLastMinBalance(0) + 1, 0);
            checkAvailableLimit(0, INT64_MAX, 0);

                        checkAvailableLimit(0, lm.getLastMinBalance(0) + 1,
                                INT64_MAX - lm.getLastMinBalance(0) - 1);
            checkAvailableLimit(0, INT64_MAX - 1, 1);

                        checkAvailableLimit(0, lm.getLastMinBalance(0) + 1,
                                INT64_MAX - lm.getLastMinBalance(0) - 2);
            checkAvailableLimit(0, INT64_MAX - 2, 1);
        });
    }

    SECTION("trustline available balance")
    {
        auto checkAvailableBalance = [&](int64_t initLimit, int64_t initBalance,
                                         int64_t initSellingLiabilities) {
            TrustLineEntry tl = LedgerTestUtils::generateValidTrustLineEntry();
            tl.flags = AUTHORIZED_FLAG;
            tl.balance = initBalance;
            tl.limit = initLimit;
            if (tl.ext.v() < 1)
            {
                tl.ext.v(1);
            }
            tl.ext.v1().liabilities = Liabilities{0, initSellingLiabilities};

            LedgerEntry le;
            le.data.type(TRUSTLINE);
            le.data.trustLine() = tl;

            LedgerTxn ltx(app->getLedgerTxnRoot());
            auto header = ltx.loadHeader();
            auto trust = ltx.create(le);
            auto availableBalance =
                std::max({int64_t(0), getAvailableBalance(header, trust)});
            REQUIRE(!viichain::addBalance(header, trust, -availableBalance - 1));
            REQUIRE(viichain::addBalance(header, trust, -availableBalance));
        };

        for_versions_from(10, *app, [&] {
                        checkAvailableBalance(1, 0, 0);
            checkAvailableBalance(1, 1, 0);
            checkAvailableBalance(INT64_MAX, INT64_MAX, 0);

                        checkAvailableBalance(1, 1, 1);
            checkAvailableBalance(2, 2, 2);
            checkAvailableBalance(INT64_MAX, INT64_MAX, INT64_MAX);

                        checkAvailableBalance(2, 2, 1);
            checkAvailableBalance(INT64_MAX, INT64_MAX, 1);
            checkAvailableBalance(INT64_MAX, INT64_MAX, INT64_MAX - 1);
        });
    }

    SECTION("trustline available limit")
    {
        auto checkAvailableLimit = [&](int64_t initLimit, int64_t initBalance,
                                       int64_t initBuyingLiabilities) {
            TrustLineEntry tl = LedgerTestUtils::generateValidTrustLineEntry();
            tl.flags = AUTHORIZED_FLAG;
            tl.balance = initBalance;
            tl.limit = initLimit;
            if (tl.ext.v() < 1)
            {
                tl.ext.v(1);
            }
            tl.ext.v1().liabilities = Liabilities{initBuyingLiabilities, 0};

            LedgerEntry le;
            le.data.type(TRUSTLINE);
            le.data.trustLine() = tl;

            LedgerTxn ltx(app->getLedgerTxnRoot());
            auto header = ltx.loadHeader();
            auto trust = ltx.create(le);
            auto availableLimit =
                std::max({int64_t(0), getMaxAmountReceive(header, trust)});
            if (availableLimit < INT64_MAX)
            {
                REQUIRE(
                    !viichain::addBalance(header, trust, availableLimit + 1));
            }
            REQUIRE(viichain::addBalance(header, trust, availableLimit));
        };

        for_versions_from(10, *app, [&] {
                        checkAvailableLimit(1, 0, 0);
            checkAvailableLimit(1, 1, 0);
            checkAvailableLimit(INT64_MAX, INT64_MAX, 0);

                        checkAvailableLimit(1, 1, INT64_MAX - 1);
            checkAvailableLimit(INT64_MAX - 1, INT64_MAX - 1, 1);

                        checkAvailableLimit(1, 1, 1);
            checkAvailableLimit(1, 1, INT64_MAX - 2);
            checkAvailableLimit(INT64_MAX - 2, INT64_MAX - 2, 1);
        });
    }

    SECTION("trustline minimum limit")
    {
        auto checkMinimumLimit = [&](int64_t initBalance,
                                     int64_t initBuyingLiabilities) {
            TrustLineEntry tl = LedgerTestUtils::generateValidTrustLineEntry();
            tl.flags = AUTHORIZED_FLAG;
            tl.balance = initBalance;
            tl.limit = INT64_MAX;
            if (tl.ext.v() < 1)
            {
                tl.ext.v(1);
            }
            tl.ext.v1().liabilities = Liabilities{initBuyingLiabilities, 0};

            LedgerEntry le;
            le.data.type(TRUSTLINE);
            le.data.trustLine() = tl;

            LedgerTxn ltx(app->getLedgerTxnRoot());
            auto header = ltx.loadHeader();
            auto trust = ltx.create(le);
            trust.current().data.trustLine().limit =
                getMinimumLimit(header, trust);
            REQUIRE(getMaxAmountReceive(header, trust) == 0);
        };

        for_versions_from(10, *app, [&] {
                        checkMinimumLimit(0, 0);
            checkMinimumLimit(1, 0);
            checkMinimumLimit(10, 0);
            checkMinimumLimit(INT64_MAX, 0);

                        checkMinimumLimit(1, INT64_MAX - 1);
            checkMinimumLimit(10, INT64_MAX - 10);
            checkMinimumLimit(INT64_MAX - 1, 1);

                        checkMinimumLimit(1, 1);
            checkMinimumLimit(1, INT64_MAX - 2);
            checkMinimumLimit(INT64_MAX - 2, 1);
        });
    }
}


#include "crypto/Hex.h"
#include "crypto/SHA.h"
#include "herder/QuorumIntersectionChecker.h"
#include "lib/catch.hpp"
#include "main/Config.h"
#include "test/test.h"
#include "util/Logging.h"
#include "util/Math.h"
#include "xdrpp/marshal.h"
#include <lib/util/format.h>
#include <xdrpp/autocheck.h>

using namespace viichain;

using QS = SCPQuorumSet;
using VQ = xdr::xvector<QS>;
using VK = xdr::xvector<PublicKey>;
using std::make_shared;

TEST_CASE("quorum intersection basic 4-node", "[herder][quorumintersection]")
{
    QuorumTracker::QuorumMap qm;

    PublicKey pkA = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkB = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkC = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkD = SecretKey::pseudoRandomForTesting().getPublicKey();

    qm[pkA] = make_shared<QS>(2, VK({pkB, pkC, pkD}), VQ{});
    qm[pkB] = make_shared<QS>(2, VK({pkA, pkC, pkD}), VQ{});
    qm[pkC] = make_shared<QS>(2, VK({pkA, pkB, pkD}), VQ{});
    qm[pkD] = make_shared<QS>(2, VK({pkA, pkB, pkC}), VQ{});

    Config cfg(getTestConfig());
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum intersection 6-node with subquorums",
          "[herder][quorumintersection]")
{
    QuorumTracker::QuorumMap qm;

    PublicKey pkA = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkB = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkC = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkD = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkE = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkF = SecretKey::pseudoRandomForTesting().getPublicKey();

    SCPQuorumSet qsABC(2, VK({pkA, pkB, pkC}), VQ{});
    SCPQuorumSet qsABD(2, VK({pkA, pkB, pkD}), VQ{});
    SCPQuorumSet qsABE(2, VK({pkA, pkB, pkE}), VQ{});
    SCPQuorumSet qsABF(2, VK({pkA, pkB, pkF}), VQ{});

    SCPQuorumSet qsACD(2, VK({pkA, pkC, pkD}), VQ{});
    SCPQuorumSet qsACE(2, VK({pkA, pkC, pkE}), VQ{});
    SCPQuorumSet qsACF(2, VK({pkA, pkC, pkF}), VQ{});

    SCPQuorumSet qsADE(2, VK({pkA, pkD, pkE}), VQ{});
    SCPQuorumSet qsADF(2, VK({pkA, pkD, pkF}), VQ{});

    SCPQuorumSet qsBDC(2, VK({pkB, pkD, pkC}), VQ{});
    SCPQuorumSet qsBDE(2, VK({pkB, pkD, pkE}), VQ{});
    SCPQuorumSet qsCDE(2, VK({pkC, pkD, pkE}), VQ{});

    qm[pkA] = make_shared<QS>(2, VK{}, VQ({qsBDC, qsBDE, qsCDE}));
    qm[pkB] = make_shared<QS>(2, VK{}, VQ({qsACD, qsACE, qsACF}));
    qm[pkC] = make_shared<QS>(2, VK{}, VQ({qsABD, qsABE, qsABF}));
    qm[pkD] = make_shared<QS>(2, VK{}, VQ({qsABC, qsABE, qsABF}));
    qm[pkE] = make_shared<QS>(2, VK{}, VQ({qsABC, qsABD, qsABF}));
    qm[pkF] = make_shared<QS>(2, VK{}, VQ({qsABC, qsABD, qsABE}));

    Config cfg(getTestConfig());
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum non intersection basic 6-node",
          "[herder][quorumintersection]")
{
    QuorumTracker::QuorumMap qm;

    PublicKey pkA = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkB = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkC = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkD = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkE = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkF = SecretKey::pseudoRandomForTesting().getPublicKey();

    qm[pkA] = make_shared<QS>(2, VK({pkB, pkC, pkD, pkE, pkF}), VQ{});
    qm[pkB] = make_shared<QS>(2, VK({pkA, pkC, pkD, pkE, pkF}), VQ{});
    qm[pkC] = make_shared<QS>(2, VK({pkA, pkB, pkD, pkE, pkF}), VQ{});
    qm[pkD] = make_shared<QS>(2, VK({pkA, pkB, pkC, pkE, pkF}), VQ{});
    qm[pkE] = make_shared<QS>(2, VK({pkA, pkB, pkC, pkD, pkF}), VQ{});
    qm[pkF] = make_shared<QS>(2, VK({pkA, pkB, pkC, pkD, pkE}), VQ{});

    Config cfg(getTestConfig());
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(!qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum non intersection 6-node with subquorums",
          "[herder][quorumintersection]")
{
    QuorumTracker::QuorumMap qm;

    PublicKey pkA = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkB = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkC = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkD = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkE = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkF = SecretKey::pseudoRandomForTesting().getPublicKey();

    SCPQuorumSet qsABC(2, VK({pkA, pkB, pkC}), VQ{});
    SCPQuorumSet qsABD(2, VK({pkA, pkB, pkD}), VQ{});
    SCPQuorumSet qsABE(2, VK({pkA, pkB, pkE}), VQ{});
    SCPQuorumSet qsABF(2, VK({pkA, pkB, pkF}), VQ{});

    SCPQuorumSet qsACD(2, VK({pkA, pkC, pkD}), VQ{});
    SCPQuorumSet qsACE(2, VK({pkA, pkC, pkE}), VQ{});
    SCPQuorumSet qsACF(2, VK({pkA, pkC, pkF}), VQ{});

    SCPQuorumSet qsADE(2, VK({pkA, pkD, pkE}), VQ{});
    SCPQuorumSet qsADF(2, VK({pkA, pkD, pkF}), VQ{});

    SCPQuorumSet qsBDC(2, VK({pkB, pkD, pkC}), VQ{});
    SCPQuorumSet qsBDE(2, VK({pkB, pkD, pkE}), VQ{});
    SCPQuorumSet qsBDF(2, VK({pkB, pkD, pkF}), VQ{});
    SCPQuorumSet qsCDE(2, VK({pkC, pkD, pkE}), VQ{});
    SCPQuorumSet qsCDF(2, VK({pkC, pkD, pkF}), VQ{});

    qm[pkA] = make_shared<QS>(2, VK{}, VQ({qsABC, qsABD, qsABE}));
    qm[pkB] = make_shared<QS>(2, VK{}, VQ({qsBDC, qsABD, qsABF}));
    qm[pkC] = make_shared<QS>(2, VK{}, VQ({qsACD, qsACD, qsACF}));

    qm[pkD] = make_shared<QS>(2, VK{}, VQ({qsCDE, qsADE, qsBDE}));
    qm[pkE] = make_shared<QS>(2, VK{}, VQ({qsCDE, qsADE, qsBDE}));
    qm[pkF] = make_shared<QS>(2, VK{}, VQ({qsABF, qsADF, qsBDF}));

    Config cfg(getTestConfig());
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(!qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum plausible non intersection", "[herder][quorumintersection]")
{
    QuorumTracker::QuorumMap qm;

    PublicKey pkSDF1 = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkSDF2 = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkSDF3 = SecretKey::pseudoRandomForTesting().getPublicKey();

    PublicKey pkLOBSTR1 = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkLOBSTR2 = SecretKey::pseudoRandomForTesting().getPublicKey();

    PublicKey pkSatoshi1 = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkSatoshi2 = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkSatoshi3 = SecretKey::pseudoRandomForTesting().getPublicKey();

    PublicKey pkCOINQVEST1 = SecretKey::pseudoRandomForTesting().getPublicKey();
    PublicKey pkCOINQVEST2 = SecretKey::pseudoRandomForTesting().getPublicKey();

    Config cfg(getTestConfig());
    cfg.VALIDATOR_NAMES[KeyUtils::toStrKey(pkSDF1)] = "SDF1";
    cfg.VALIDATOR_NAMES[KeyUtils::toStrKey(pkSDF2)] = "SDF2";
    cfg.VALIDATOR_NAMES[KeyUtils::toStrKey(pkSDF3)] = "SDF3";
    cfg.VALIDATOR_NAMES[KeyUtils::toStrKey(pkLOBSTR1)] = "LOBSTR1_Europe";
    cfg.VALIDATOR_NAMES[KeyUtils::toStrKey(pkLOBSTR2)] = "LOBSTR2_Europe";
    cfg.VALIDATOR_NAMES[KeyUtils::toStrKey(pkSatoshi1)] =
        "SatoshiPay_DE_Frankfurt";
    cfg.VALIDATOR_NAMES[KeyUtils::toStrKey(pkSatoshi2)] =
        "SatoshiPay_SG_Singapore";
    cfg.VALIDATOR_NAMES[KeyUtils::toStrKey(pkSatoshi3)] = "SatoshiPay_US_Iowa";
    cfg.VALIDATOR_NAMES[KeyUtils::toStrKey(pkCOINQVEST1)] = "COINQVEST_Germany";
    cfg.VALIDATOR_NAMES[KeyUtils::toStrKey(pkCOINQVEST2)] = "COINQVEST_Finland";

    
    SCPQuorumSet qs1of2LOBSTR(1, VK({pkLOBSTR1, pkLOBSTR2}), VQ{});
    SCPQuorumSet qs1of2COINQVEST(1, VK({pkCOINQVEST1, pkCOINQVEST2}), VQ{});

    SCPQuorumSet qs2of3SDF(1, VK({pkSDF1, pkSDF2, pkSDF3}), VQ{});

    SCPQuorumSet qs2of3SatoshiPay(2, VK({pkSatoshi1, pkSatoshi2, pkSatoshi3}),
                                  VQ{});

        auto qsSDF = make_shared<QS>(3, VK({pkSDF1, pkSDF2, pkSDF3}),
                                 VQ({qs1of2LOBSTR, qs2of3SatoshiPay}));
    qm[pkSDF1] = qsSDF;
    qm[pkSDF2] = qsSDF;
    qm[pkSDF3] = qsSDF;

        auto qsSatoshiPay =
        make_shared<QS>(4, VK({pkSatoshi1, pkSatoshi2, pkSatoshi3}),
                        VQ({qs2of3SDF, qs1of2LOBSTR, qs1of2COINQVEST}));
    qm[pkSatoshi1] = qsSatoshiPay;
    qm[pkSatoshi2] = qsSatoshiPay;
    qm[pkSatoshi3] = qsSatoshiPay;

        auto qsLOBSTR = make_shared<QS>(
        5, VK({pkSDF1, pkSDF2, pkSDF3, pkSatoshi1, pkSatoshi2, pkSatoshi3}),
        VQ{});
    qm[pkLOBSTR1] = qsLOBSTR;
    qm[pkLOBSTR2] = qsLOBSTR;

        auto qsCOINQVEST =
        make_shared<QS>(3, VK({pkCOINQVEST1, pkCOINQVEST2}),
                        VQ({qs2of3SDF, qs2of3SatoshiPay, qs1of2LOBSTR}));
    qm[pkCOINQVEST1] = qsCOINQVEST;
    qm[pkCOINQVEST2] = qsCOINQVEST;

    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(!qic->networkEnjoysQuorumIntersection());
}

uint32
roundUpPct(size_t n, size_t pct)
{
    return static_cast<uint32>(size_t(1) +
                               (((n * pct) - size_t(1)) / size_t(100)));
}

static xdr::xvector<xdr::xvector<PublicKey>>
generateOrgs(size_t n_orgs, std::vector<size_t> sizes = {3, 5})
{
    xdr::xvector<xdr::xvector<PublicKey>> ret;

    for (size_t i = 0; i < n_orgs; ++i)
    {
        ret.emplace_back();
        size_t n_nodes = sizes.at(i % sizes.size());
        for (size_t j = 0; j < n_nodes; ++j)
        {
            ret.back().emplace_back(
                SecretKey::pseudoRandomForTesting().getPublicKey());
        }
    }
    return ret;
}

static Config
configureShortNames(Config const& cfgIn,
                    xdr::xvector<xdr::xvector<PublicKey>> const& orgs)
{
    Config cfgOut(cfgIn);
    for (size_t i = 0; i < orgs.size(); ++i)
    {
        auto const& org = orgs.at(i);
        for (size_t j = 0; j < org.size(); ++j)
        {
            auto n = KeyUtils::toStrKey(org.at(j));
            auto s = fmt::format("org{}.n{}", i, j);
            cfgOut.VALIDATOR_NAMES[n] = s;
        }
    }
    return cfgOut;
}

static QuorumTracker::QuorumMap
interconnectOrgs(xdr::xvector<xdr::xvector<PublicKey>> const& orgs,
                 std::function<bool(size_t i, size_t j)> shouldDepend,
                 size_t ownThreshPct = 67, size_t innerThreshPct = 51)
{
    QuorumTracker::QuorumMap qm;
    xdr::xvector<SCPQuorumSet> emptySet;
    for (size_t i = 0; i < orgs.size(); ++i)
    {
        auto const& org = orgs.at(i);
        auto qs = std::make_shared<SCPQuorumSet>();
        qs->validators = org;
        for (auto const& pk : org)
        {
            qm[pk] = qs;
        }
        auto& depOrgs = qs->innerSets;
        for (size_t j = 0; j < orgs.size(); ++j)
        {
            if (i == j)
            {
                continue;
            }
            if (shouldDepend(i, j))
            {
                CLOG(DEBUG, "SCP") << "dep: org#" << i << " => org#" << j;
                auto& otherOrg = orgs.at(j);
                auto thresh = roundUpPct(otherOrg.size(), innerThreshPct);
                depOrgs.emplace_back(thresh, otherOrg, emptySet);
            }
        }
        qs->threshold = roundUpPct(qs->validators.size() + qs->innerSets.size(),
                                   ownThreshPct);
    }
    return qm;
}

static QuorumTracker::QuorumMap
interconnectOrgsUnidir(xdr::xvector<xdr::xvector<PublicKey>> const& orgs,
                       std::vector<std::pair<size_t, size_t>> edges,
                       size_t ownThreshPct = 67, size_t innerThreshPct = 51)
{
    return interconnectOrgs(orgs,
                            [&edges](size_t i, size_t j) {
                                for (auto const& e : edges)
                                {
                                    if (e.first == i && e.second == j)
                                    {
                                        return true;
                                    }
                                }
                                return false;
                            },
                            ownThreshPct, innerThreshPct);
}

static QuorumTracker::QuorumMap
interconnectOrgsBidir(xdr::xvector<xdr::xvector<PublicKey>> const& orgs,
                      std::vector<std::pair<size_t, size_t>> edges,
                      size_t ownThreshPct = 67, size_t innerThreshPct = 51)
{
    return interconnectOrgs(orgs,
                            [&edges](size_t i, size_t j) {
                                for (auto const& e : edges)
                                {
                                    if ((e.first == i && e.second == j) ||
                                        (e.first == j && e.second == i))
                                    {
                                        return true;
                                    }
                                }
                                return false;
                            },
                            ownThreshPct, innerThreshPct);
}

TEST_CASE("quorum intersection 4-org fully-connected, elide all minquorums",
          "[herder][quorumintersection]")
{
                    auto orgs = generateOrgs(4);
    auto qm = interconnectOrgs(orgs, [](size_t i, size_t j) { return true; });
    Config cfg(getTestConfig());
    cfg = configureShortNames(cfg, orgs);
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum intersection 3-org 3-node open line",
          "[herder][quorumintersection]")
{
                            auto orgs = generateOrgs(3, {3});
    auto qm = interconnectOrgsBidir(orgs, {{0, 1}, {1, 2}});
    Config cfg(getTestConfig());
    cfg = configureShortNames(cfg, orgs);
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(!qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum intersection 3-org 2-node open line",
          "[herder][quorumintersection]")
{
                        auto orgs = generateOrgs(3, {2});
    auto qm = interconnectOrgsBidir(orgs, {{0, 1}, {1, 2}});
    Config cfg(getTestConfig());
    cfg = configureShortNames(cfg, orgs);
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum intersection 3-org 3-node closed ring",
          "[herder][quorumintersection]")
{
                                    auto orgs = generateOrgs(3, {3});
    auto qm = interconnectOrgsBidir(orgs, {{0, 1}, {1, 2}, {0, 2}});
    Config cfg(getTestConfig());
    cfg = configureShortNames(cfg, orgs);
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum intersection 3-org 3-node closed one-way ring",
          "[herder][quorumintersection]")
{
                                        auto orgs = generateOrgs(3, {3});
    auto qm = interconnectOrgsUnidir(orgs, {
                                               {0, 1},
                                               {1, 2},
                                               {2, 0},
                                           });
    Config cfg(getTestConfig());
    cfg = configureShortNames(cfg, orgs);
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(!qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum intersection 3-org 2-node closed one-way ring",
          "[herder][quorumintersection]")
{
                                        auto orgs = generateOrgs(3, {2});
    auto qm = interconnectOrgsUnidir(orgs, {
                                               {0, 1},
                                               {1, 2},
                                               {2, 0},
                                           });
    Config cfg(getTestConfig());
    cfg = configureShortNames(cfg, orgs);
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum intersection 3-org 2-node 2-of-3 asymmetric",
          "[herder][quorumintersection]")
{
                                                auto orgs = generateOrgs(3, {3});
    auto qm = interconnectOrgsUnidir(orgs, {
                                               {0, 1},
                                               {0, 2},
                                               {1, 0},
                                               {1, 2},
                                               {2, 0},
                                               {2, 1},
                                           });
    Config cfg(getTestConfig());
    cfg = configureShortNames(cfg, orgs);
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum intersection 8-org core-and-periphery dangling",
          "[herder][quorumintersection]")
{
                                                                                auto orgs = generateOrgs(8, {3, 3, 3, 3, 2, 2, 2, 2});
    auto qm = interconnectOrgsBidir(
        orgs,
        {// 4 core orgs 0, 1, 2, 3 (with 3 nodes each) which fully depend on one
                  {0, 1},
         {0, 2},
         {0, 3},
         {1, 2},
         {1, 3},
         {2, 3},
                                    {0, 4},
         {1, 5},
         {2, 6},
         {3, 7}});
    Config cfg(getTestConfig());
    cfg = configureShortNames(cfg, orgs);
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(!qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum intersection 8-org core-and-periphery balanced",
          "[herder][quorumintersection]")
{
                                                                                        auto orgs = generateOrgs(8, {3, 3, 3, 3, 2, 2, 2, 2});
    auto qm = interconnectOrgsBidir(
        orgs,
        {// 4 core orgs 0, 1, 2, 3 (with 3 nodes each) which fully depend on one
                  {0, 1},
         {0, 2},
         {0, 3},
         {1, 2},
         {1, 3},
         {2, 3},
                                    {0, 4},
         {1, 4},
         {1, 5},
         {3, 5},
         {2, 6},
         {0, 6},
         {3, 7},
         {2, 7}});
    Config cfg(getTestConfig());
    cfg = configureShortNames(cfg, orgs);
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum intersection 8-org core-and-periphery unbalanced",
          "[herder][quorumintersection]")
{
                                                                    auto orgs = generateOrgs(8, {3, 3, 3, 3, 2, 2, 2, 2});
    auto qm = interconnectOrgsBidir(
        orgs,
        {// 4 core orgs 0, 1, 2, 3 (with 3 nodes each) which fully depend on one
                  {0, 1},
         {0, 2},
         {0, 3},
         {1, 2},
         {1, 3},
         {2, 3},
                                    {0, 4},
         {1, 4},
         {0, 5},
         {1, 5},
         {2, 6},
         {3, 6},
         {2, 7},
         {3, 7}});
    Config cfg(getTestConfig());
    cfg = configureShortNames(cfg, orgs);
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(!qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum intersection 6-org 1-node 4-null qsets",
          "[herder][quorumintersection]")
{
                                                                                            
    auto orgs = generateOrgs(6, {1});
    auto qm = interconnectOrgsUnidir(orgs, {
                                               {0, 1},
                                               {1, 0},
                                               {0, 2},
                                               {0, 3},
                                               {1, 4},
                                               {1, 5},
                                           });

        for (size_t i = 2; i < orgs.size(); ++i)
    {
        for (auto const& node : orgs.at(i))
        {
            qm[node] = nullptr;
        }
    }

    Config cfg(getTestConfig());
    cfg = configureShortNames(cfg, orgs);
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(qic->networkEnjoysQuorumIntersection());
    REQUIRE(qic->getMaxQuorumsFound() == 0);
}

TEST_CASE("quorum intersection 4-org 1-node 4-null qsets",
          "[herder][quorumintersection]")
{
                                                            
    auto orgs = generateOrgs(4, {1});
    auto qm = interconnectOrgsUnidir(orgs, {
                                               {0, 1},
                                               {1, 0},
                                               {0, 2},
                                               {0, 3},
                                               {1, 2},
                                               {1, 3},
                                           });

        for (size_t i = 2; i < orgs.size(); ++i)
    {
        for (auto const& node : orgs.at(i))
        {
            qm[node] = nullptr;
        }
    }

    Config cfg(getTestConfig());
    cfg = configureShortNames(cfg, orgs);
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(qic->networkEnjoysQuorumIntersection());
    REQUIRE(qic->getMaxQuorumsFound() == 0);
}

TEST_CASE("quorum intersection 6-org 3-node fully-connected",
          "[herder][quorumintersection]")
{
    auto orgs = generateOrgs(6, {3});
    auto qm = interconnectOrgs(orgs, [](size_t i, size_t j) { return true; });
    Config cfg(getTestConfig());
    cfg = configureShortNames(cfg, orgs);
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(qic->networkEnjoysQuorumIntersection());
}

TEST_CASE("quorum intersection scaling test",
          "[herder][quorumintersectionbench][!hide]")
{
            auto orgs = generateOrgs(6);
    auto qm = interconnectOrgs(orgs, [](size_t i, size_t j) { return true; });
    Config cfg(getTestConfig());
    cfg = configureShortNames(cfg, orgs);
    auto qic = QuorumIntersectionChecker::create(qm, cfg);
    REQUIRE(qic->networkEnjoysQuorumIntersection());
}


#include "bucket/Bucket.h"
#include "ledger/test/LedgerTestUtils.h"
#include "lib/catch.hpp"
#include "util/XDRStream.h"

using namespace viichain;

TEST_CASE("XDROutputFileStream fail modes", "[xdrstream]")
{
    XDROutputFileStream out;
    auto filename = "someFile";

    SECTION("open throws")
    {
        REQUIRE_NOTHROW(out.open(filename));
                REQUIRE_THROWS_AS(out.open(filename), std::runtime_error);
        std::remove(filename);
    }
    SECTION("write throws")
    {
        auto hasher = SHA256::create();
        size_t bytes = 0;
        auto ledgerEntries = LedgerTestUtils::generateValidLedgerEntries(1);
        auto bucketEntries =
            Bucket::convertToBucketEntry(false, {}, ledgerEntries, {});

        REQUIRE_THROWS_AS(out.writeOne(bucketEntries[0], hasher.get(), &bytes),
                          std::ios_base::failure);
    }
    SECTION("close throws")
    {
        REQUIRE_THROWS_AS(out.close(), std::runtime_error);
    }
}

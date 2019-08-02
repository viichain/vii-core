
#include "history/HistoryArchive.h"
#include "lib/catch.hpp"

#include <fstream>
#include <string>

using namespace viichain;

TEST_CASE("Serialization round trip", "[history][serialize]")
{
    std::vector<std::string> testFiles = {
        "vii-history.testnet.6714239.json",
        "vii-history.livenet.15686975.json"};
    for (auto const& fn : testFiles)
    {
        std::string fnPath = "testdata/";
        fnPath += fn;
        SECTION("Serialize " + fnPath)
        {
            std::ifstream in(fnPath);
            std::string fromFile((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());

            HistoryArchiveState has;
            has.fromString(fromFile);
            REQUIRE(fromFile == has.toString());
        }
    }
}

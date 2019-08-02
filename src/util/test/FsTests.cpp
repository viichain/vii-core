
#include "lib/catch.hpp"
#include "util/Fs.h"
#include <tuple>

using namespace viichain::fs;

TEST_CASE("path splitter", "[fs]")
{
    using DataValue =
        std::tuple<std::string, std::string, std::vector<std::string>>;
    auto data = std::vector<DataValue>{
        DataValue{"root", "/", {"/"}},
        DataValue{"simple dir", "simple", {"simple"}},
        DataValue{
            "nested dir", "nested1/nested2/", {"nested1", "nested1/nested2"}},
        DataValue{"nested dir with multiple slashes",
                  "nested1///nested2/",
                  {"nested1", "nested1/nested2"}},
        DataValue{"nested dir starting at root",
                  "/nested1/nested2/",
                  {"/", "/nested1", "/nested1/nested2"}},
        DataValue{"nested dir starting at rootwith multiple slashes",
                  "/nested1///nested2/",
                  {"/", "/nested1", "/nested1/nested2"}}};

    using ExtensionValue = std::pair<std::string, std::string>;
    auto extensions = std::vector<ExtensionValue>{
        ExtensionValue{"without slash at end", ""},
        ExtensionValue{"with slash at end", "/"},
        ExtensionValue{"with multiple slashes at end", "//"}};

    SECTION("empty")
    {
        auto ps = PathSplitter{""};
        REQUIRE(!ps.hasNext());
    }

    for (auto const& i : data)
    {
        for (auto const& e : extensions)
        {
            SECTION(std::get<0>(i) + " " + e.first)
            {
                auto ps = PathSplitter{std::get<1>(i) + e.second};
                for (auto const& p : std::get<2>(i))
                {
                    REQUIRE(ps.hasNext());
                    REQUIRE(ps.next() == p);
                }
                REQUIRE(!ps.hasNext());
            }
        }
    }
}

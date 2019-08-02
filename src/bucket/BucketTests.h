
#include <cstdint>
#include <functional>
#include <memory>

namespace viichain
{
class Application;
class Config;
class Bucket;
}

namespace BucketTests
{
uint32_t getAppLedgerVersion(viichain::Application& app);

uint32_t getAppLedgerVersion(std::shared_ptr<viichain::Application> app);
void for_versions_with_differing_bucket_logic(
    viichain::Config const& cfg,
    std::function<void(viichain::Config const&)> const& f);

struct EntryCounts
{
    size_t nMeta{0};
    size_t nInit{0};
    size_t nLive{0};
    size_t nDead{0};
    size_t
    sum() const
    {
        return nLive + nInit + nDead;
    }
    size_t
    sumIncludingMeta() const
    {
        return nLive + nInit + nDead + nMeta;
    }
    EntryCounts(std::shared_ptr<viichain::Bucket> bucket);
};

size_t countEntries(std::shared_ptr<viichain::Bucket> bucket);
}

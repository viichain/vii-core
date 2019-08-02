
#include "Application.h"
#include "ApplicationImpl.h"
#include "util/format.h"

namespace viichain
{
using namespace std;

void
validateNetworkPassphrase(Application::pointer app)
{
    std::string networkPassphrase = app->getConfig().NETWORK_PASSPHRASE;
    if (networkPassphrase.empty())
    {
        throw std::invalid_argument("NETWORK_PASSPHRASE not configured");
    }

    auto& persistentState = app->getPersistentState();
    std::string prevNetworkPassphrase =
        persistentState.getState(PersistentState::kNetworkPassphrase);
    if (prevNetworkPassphrase.empty())
    {
        persistentState.setState(PersistentState::kNetworkPassphrase,
                                 networkPassphrase);
    }
    else if (networkPassphrase != prevNetworkPassphrase)
    {
        throw std::invalid_argument(
            fmt::format("NETWORK_PASSPHRASE \"{}\" does not match"
                        " previous NETWORK_PASSPHRASE \"{}\"",
                        networkPassphrase, prevNetworkPassphrase));
    }
}

Application::pointer
Application::create(VirtualClock& clock, Config const& cfg, bool newDB)
{
    return create<ApplicationImpl>(clock, cfg, newDB);
}
}

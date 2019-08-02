#pragma once


#include "lib/http/server.hpp"
#include <string>

namespace viichain
{
class Application;

class CommandHandler
{
    typedef std::function<void(CommandHandler*, std::string const&,
                               std::string&)>
        HandlerRoute;

    Application& mApp;
    std::unique_ptr<http::server::server> mServer;

    void addRoute(std::string const& name, HandlerRoute route);
    void safeRouter(HandlerRoute route, std::string const& params,
                    std::string& retStr);

  public:
    CommandHandler(Application& app);

    void manualCmd(std::string const& cmd);

    void fileNotFound(std::string const& params, std::string& retStr);

    void bans(std::string const& params, std::string& retStr);
    void checkdb(std::string const& params, std::string& retStr);
    void connect(std::string const& params, std::string& retStr);
    void dropcursor(std::string const& params, std::string& retStr);
    void dropPeer(std::string const& params, std::string& retStr);
    void info(std::string const& params, std::string& retStr);
    void ll(std::string const& params, std::string& retStr);
    void logRotate(std::string const& params, std::string& retStr);
    void maintenance(std::string const& params, std::string& retStr);
    void manualClose(std::string const& params, std::string& retStr);
    void metrics(std::string const& params, std::string& retStr);
    void clearMetrics(std::string const& params, std::string& retStr);
    void peers(std::string const& params, std::string& retStr);
    void quorum(std::string const& params, std::string& retStr);
    void setcursor(std::string const& params, std::string& retStr);
    void getcursor(std::string const& params, std::string& retStr);
    void scpInfo(std::string const& params, std::string& retStr);
    void tx(std::string const& params, std::string& retStr);
    void unban(std::string const& params, std::string& retStr);
    void upgrades(std::string const& params, std::string& retStr);

#ifdef BUILD_TESTS
    void generateLoad(std::string const& params, std::string& retStr);
    void testAcc(std::string const& params, std::string& retStr);
    void testTx(std::string const& params, std::string& retStr);
#endif
};
}

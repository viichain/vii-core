#pragma once


#include "util/asio.h"

#include <string>
#include <map>
#include <functional>
#include "connection.hpp"
#include "connection_manager.hpp"

namespace http {
namespace server {

class server
{

public:
    typedef std::function<void(const std::string&, std::string&)> routeHandler;
    server(const server&) = delete;
    server& operator=(const server&) = delete;

        server(asio::io_service& io_service);

        explicit server(asio::io_service& io_service,
                    const std::string& address, unsigned short port, int maxClient);
    ~server();

    void addRoute(const std::string& routeName, routeHandler callback);
    void add404(routeHandler callback);

    void handle_request(const request& req, reply& rep);

    static void parseParams(const std::string& params, std::map<std::string, std::string>& retMap);

private:
        void do_accept();

            static bool url_decode(const std::string& in, std::string& out);

        asio::io_service& io_service_;

        asio::signal_set signals_;

        asio::ip::tcp::acceptor acceptor_;

        connection_manager connection_manager_;

        asio::ip::tcp::socket socket_;

    std::map<std::string, routeHandler> mRoutes;
};

} // namespace server
} // namespace http

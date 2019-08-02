#pragma once


#include "util/asio.h"

#include <array>
#include <memory>
#include "reply.hpp"
#include "request.hpp"
#include "request_parser.hpp"

namespace http {
namespace server {

class connection_manager;
class server;

class connection
  : public std::enable_shared_from_this<connection>
{
public:
  connection(const connection&) = delete;
  connection& operator=(const connection&) = delete;

    explicit connection(asio::ip::tcp::socket socket,
      connection_manager& manager, server& handler);

    void start();

    void stop();

private:
    void do_read();

    void do_write();

    asio::ip::tcp::socket socket_;

    connection_manager& connection_manager_;

    server& request_handler_;

    std::array<char, 8192> buffer_;

    size_t received_count_;

    request request_;

    request_parser request_parser_;

    reply reply_;
};

typedef std::shared_ptr<connection> connection_ptr;

} // namespace server
} // namespace http

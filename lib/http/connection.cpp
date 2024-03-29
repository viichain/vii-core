
#include "connection.hpp"
#include <utility>
#include <vector>
#include "connection_manager.hpp"
#include "server.hpp"

#define MAX_REQUEST_SIZE (1024 * 1024 * 10) // 10 MB

namespace http
{
namespace server
{

connection::connection(asio::ip::tcp::socket socket,
                       connection_manager& manager, server& handler)
    : socket_(std::move(socket))
    , connection_manager_(manager)
    , request_handler_(handler)
    , received_count_(0)
{
}

void
connection::start()
{
    do_read();
}

void
connection::stop()
{
    socket_.close();
}

void
connection::do_read()
{
    auto self(shared_from_this());
    socket_.async_read_some(asio::buffer(buffer_),
                            [this, self](asio::error_code ec,
                                         std::size_t bytes_transferred)
                            {
        if (!ec)
        {
            request_parser::result_type result;
            std::tie(result, std::ignore) = request_parser_.parse(
                request_, buffer_.data(), buffer_.data() + bytes_transferred);
            received_count_ += bytes_transferred;

            if (result == request_parser::bad || received_count_ > MAX_REQUEST_SIZE)
            {
                reply_ = reply::stock_reply(reply::bad_request);
                do_write();
            }
            else if (result == request_parser::good)
            {
                request_handler_.handle_request(request_, reply_);
                do_write();
            }
            else
            {
                do_read();
            }
        }
        else if (ec != asio::error::operation_aborted)
        {
            connection_manager_.stop(shared_from_this());
        }
    });
}

void
connection::do_write()
{
    auto self(shared_from_this());
    asio::async_write(socket_, reply_.to_buffers(),
                      [this, self](asio::error_code ec, std::size_t)
                      {
        if (!ec)
        {
                        asio::error_code ignored_ec;
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
        }

        if (ec != asio::error::operation_aborted)
        {
            connection_manager_.stop(shared_from_this());
        }
        asio::error_code closeEc;
        socket_.close(closeEc);
    });
}

} // namespace server
} // namespace http

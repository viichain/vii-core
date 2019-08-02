#include "util/asio.h"

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include "util/Logging.h"

using asio::ip::tcp;

int
http_request(std::string domain, std::string path, unsigned short port, std::string& ret)
{
    try
    {
        std::ostringstream retSS;
        asio::io_service io_service;

        asio::ip::tcp::socket socket(io_service);
        asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(domain),
                                         port);
        socket.connect(endpoint);

                                                asio::streambuf request;
        std::ostream request_stream(&request);
        request_stream << "GET " << path << " HTTP/1.0\r\n";
        request_stream << "Host: " << domain << "\r\n";
        request_stream << "Accept: */*\r\n";
        request_stream << "Connection: close\r\n\r\n";

                asio::write(socket, request);

                                                asio::streambuf response;
        asio::read_until(socket, response, "\r\n");

                std::istream response_stream(&response);
        std::string http_version;
        response_stream >> http_version;
        unsigned int status_code;
        response_stream >> status_code;
        std::string status_message;
        std::getline(response_stream, status_message);
        if (!response_stream || http_version.substr(0, 5) != "HTTP/")
        {
            LOG(DEBUG) << "Invalid response\n";
            return 1;
        }
        if (status_code != 200)
        {
            LOG(DEBUG) << "Response returned with status code " << status_code
                       << "\n";
            return status_code;
        }

                asio::read_until(socket, response, "\r\n\r\n");

                std::string header;
        while (std::getline(response_stream, header) && header != "\r")
            std::cout << header << "\n";
        std::cout << "\n";

                if (response.size() > 0)
            retSS << &response;

                asio::error_code error;
        while (asio::read(socket, response, asio::transfer_at_least(1), error))
            retSS << &response;
        if (error != asio::error::eof)
            throw asio::system_error(error);

        ret = retSS.str();
        return 200;
    }
    catch (std::exception& e)
    {
        LOG(DEBUG) << "Exception: " << e.what() << "\n";
        return 1;
    }
}

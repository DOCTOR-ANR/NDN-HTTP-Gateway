/*
Copyright (C) 2015-2017  Xavier MARCHAL
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "boost/asio.hpp"

#include <memory>

#include "global.h"
#include "module.h"
#include "http_sink.h"
#include "http_request.h"
#include "http_response.h"

class HttpServer : public Module {
private:
    static boost::posix_time::seconds DEFAULT_TIMEOUT_READ_HTTP_HEADER;
    static boost::posix_time::seconds DEFAULT_TIMEOUT_READ_HTTP_BODY;

    class HttpSession : public std::enable_shared_from_this<HttpSession> {
    private:
        HttpServer &_http_server;
        boost::asio::strand _strand;
        std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
        boost::asio::streambuf _buffer;
        std::shared_ptr<HttpRequest> _http_request;
        boost::asio::deadline_timer _read_timer;
        std::shared_ptr<HttpResponse> _http_response;
        boost::asio::deadline_timer _write_timer;

    public:
        HttpSession(HttpServer &http_server, std::shared_ptr<boost::asio::ip::tcp::socket> socket);

        ~HttpSession();

        void start();

        void read_request_header();

        void read_request_header_handler(const boost::system::error_code &err, size_t bytes_transferred);

        void read_request_body(size_t remaining_bytes);

        void read_request_body_handler(const boost::system::error_code &err, size_t bytes_transferred, size_t remaining_bytes);

        void read_request_body_chuncked(long chunck_size = -1);

        void read_request_body_chuncked_handler(const boost::system::error_code &err, size_t bytes_transferred, long chunck_size);

        void write_response();

        void write_response_handler(const boost::system::error_code &err, size_t bytes_transferred);

        void timer_handler(const boost::system::error_code &err);
    };

    boost::asio::ip::tcp::acceptor _acceptor;

    HttpSink *_http_sink;

public:
    enum method_type {
        CONNECT,
        DELETE,
        GET,
        HEAD,
        OPTIONS,
        POST,
        PUT,
    };

    static std::map<std::string, method_type> available_methods;

    HttpServer(unsigned short port, size_t concurrency = 1);

    ~HttpServer();

    virtual void run() override;

    void accept();

    void accept_handler(const boost::system::error_code &err, std::shared_ptr<boost::asio::ip::tcp::socket> socket);

    void attach_http_sink(HttpSink *http_sink);
};
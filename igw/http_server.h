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
#include <mutex>
#include <unordered_map>
#include <fstream>
#include <chrono>

#include "global.h"
#include "module.h"
#include "http_source.h"
#include "http_request.h"
#include "http_response.h"

class HttpServer : public Module, public HttpSource {
private:
    class HttpSession : public std::enable_shared_from_this<HttpSession> {
    private:
        HttpServer &_http_server;

        boost::asio::strand _strand;
        boost::asio::deadline_timer _read_timer;
        boost::asio::deadline_timer _write_timer;
        std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
        boost::asio::streambuf _read_buffer;
        char _write_buffer[global::DEFAULT_BUFFER_SIZE];

        std::shared_ptr<HttpRequest> _http_request;
        std::shared_ptr<HttpResponse> _http_response;

        std::chrono::steady_clock::time_point _start;

    public:
        HttpSession(HttpServer &http_server, const std::shared_ptr<boost::asio::ip::tcp::socket> &socket);

        ~HttpSession() = default;

        void setHttpResponse(const std::shared_ptr<HttpResponse> &http_response);

        void start();

        void notify();

    private:
        void read_request_header();

        void read_request_header_handler(const boost::system::error_code &err, size_t bytes_transferred);

        void read_request_body(size_t remaining_bytes);

        void read_request_body_handler(const boost::system::error_code &err, size_t bytes_transferred, size_t remaining_bytes);

        void read_request_body_chunk(long chunk_size = -1);

        void read_request_body_chunk_handler(const boost::system::error_code &err, size_t bytes_transferred, long chunk_size);

        void write_response();

        void write_response_header();

        void write_response_header_handler(const boost::system::error_code &err, size_t bytes_transferred);

        void write_response_body(size_t total_bytes_transferred = 0);

        void write_response_body_handler(const boost::system::error_code &err, size_t bytes_transferred,
                                                 size_t total_bytes_transferred);

        void timer_handler(const boost::system::error_code &err);
    };

    std::mutex _map_mutex;
    std::unordered_map<std::shared_ptr<HttpRequest>, std::shared_ptr<HttpSession>> _waiting_sessions;

    boost::asio::ip::tcp::acceptor _acceptor;

    std::mutex _file_mutex;
    std::ofstream _file;
    std::chrono::steady_clock::time_point _start;

public:
    explicit HttpServer(unsigned short port = 8080, size_t concurrency = 1);

    ~HttpServer() override = default;

    void run() override;

    void accept();

    void fromHttpSink(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response) override;

    void log(const std::string &line);
private:
    void accept_handler(const boost::system::error_code &err, const std::shared_ptr<boost::asio::ip::tcp::socket> &socket);

    void fromHttpSinkHandler(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response);
};
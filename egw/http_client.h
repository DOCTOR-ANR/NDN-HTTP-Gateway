/*
Copyright (C) 2015-2018  Xavier MARCHAL
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

#include <memory>
#include <atomic>

#include "global.h"
#include "module.h"
#include "http_sink.h"
#include "http_request.h"
#include "http_response.h"

class HttpClient : public Module, public HttpSink {
private:
    class HttpSession : public std::enable_shared_from_this<HttpSession> {
    private:
#ifndef NDEBUG
        static std::atomic<size_t> count;
#endif

        HttpClient &_http_client;

        boost::asio::deadline_timer _timer;
        boost::asio::strand _strand;
        boost::asio::ip::tcp::socket _socket;
        std::shared_ptr<HttpRequest> _http_request;
        char _write_buffer[global::DEFAULT_BUFFER_SIZE];
        std::shared_ptr<HttpResponse> _http_response;
        boost::asio::streambuf _read_buffer;

        boost::chrono::steady_clock::time_point _time_point;

    public:
        HttpSession(HttpClient &http_client, const std::shared_ptr<HttpRequest> &http_request);

        ~HttpSession();

        void start();

        void resolve_domain();

        void resolve_domain_handler(const boost::system::error_code &err, boost::asio::ip::tcp::resolver::iterator iterator);

        void connect(boost::asio::ip::tcp::resolver::iterator iterator);

        void connect_handler(const boost::system::error_code &err, boost::asio::ip::tcp::resolver::iterator iterator);

        void write_request_header();

        void write_request_header_handler(const boost::system::error_code &err, size_t bytes_transferred);

        void write_request_body(size_t total_bytes_transferred);

        void write_request_body_handler(const boost::system::error_code &err, size_t bytes_transferred, size_t total_bytes_transferred);

        void read_response_header();

        void read_response_header_handler(const boost::system::error_code &err, size_t bytes_transferred);

        void read_response_body(long remaining_bytes);

        void read_response_body_handler(const boost::system::error_code &err, size_t bytes_transferred, long remaining_bytes);

        void read_response_body_chunk(long chunk_size);

        void read_response_body_chunk_handler(const boost::system::error_code &err, size_t bytes_transferred, long chunk_size);

        void read_response_body_old();

        void read_response_body_old_handler(const boost::system::error_code &err, size_t bytes_transferred);

        void timer_handler(const boost::system::error_code &err);
    };

    boost::asio::ip::tcp::resolver _resolver;

public:
    explicit HttpClient(size_t concurrency = 1);

    ~HttpClient() override = default;

    void run() override;

    void fromHttpSource(const std::shared_ptr<HttpRequest> &http_request) override;

    void fromHttpSourceHandler(const std::shared_ptr<HttpRequest> &http_request);
};
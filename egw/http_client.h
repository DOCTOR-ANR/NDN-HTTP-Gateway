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

#include <memory>

#include "global.h"
#include "module.h"
#include "http_sink.h"
#include "http_source.h"
#include "http_request.h"
#include "http_response.h"

class HttpClient : public Module, public HttpSink {
private:
    class HttpSession : public std::enable_shared_from_this<HttpSession> {
    private:
        HttpClient &_http_client;
        boost::asio::strand _strand;
        std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
        std::shared_ptr<HttpRequest> _http_request;
        char _write_buffer[global::DEFAULT_BUFFER_SIZE];
        boost::asio::deadline_timer _write_timer;
        std::shared_ptr<HttpResponse> _http_response;
        boost::asio::streambuf _read_buffer;
        boost::asio::deadline_timer _read_timer;

        boost::chrono::steady_clock::time_point _time_point;

    public:
        HttpSession(HttpClient &http_client, const std::shared_ptr<boost::asio::ip::tcp::socket> &socket,
                    const std::shared_ptr<HttpRequest> &http_request);

        ~HttpSession();

        void start();

        void write_request_header();

        void write_request_header_handler(const boost::system::error_code &err, size_t bytes_transferred);

        void write_request_body();

        void write_request_body_handler(const boost::system::error_code &err, size_t bytes_transferred);

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

    boost::asio::deadline_timer _timer;

    HttpSource *_http_source;
public:
    HttpClient(size_t concurrency = 1);

    ~HttpClient();

    virtual void run() override;

    virtual void give(std::shared_ptr<HttpRequest> http_request) override;

    void solve_handler(const std::shared_ptr<HttpRequest> &http_request);

    void resolve(const std::shared_ptr<HttpRequest> &http_request);

    void resolve_handler(const boost::system::error_code &err, boost::asio::ip::tcp::resolver::iterator iterator,
                         const std::shared_ptr<HttpRequest> &http_request);

    void connect(boost::asio::ip::tcp::resolver::iterator iterator, const std::shared_ptr<HttpRequest> &http_request);

    void connect_handler(const boost::system::error_code &err, boost::asio::ip::tcp::resolver::iterator iterator, std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                         const std::shared_ptr<HttpRequest> &http_request);

    void timer_handler(const boost::system::error_code &err,
                           const std::shared_ptr<boost::asio::ip::tcp::socket> &socket);

    void attach_http_source(HttpSource *http_source);
};
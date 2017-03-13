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
#include "http_request.h"
#include "http_response.h"

class HttpClient : public Module, public HttpSink {
private:
    static boost::posix_time::seconds DEFAULT_TIMEOUT_READ_HTTP_HEADER;
    static boost::posix_time::seconds DEFAULT_TIMEOUT_READ_HTTP_BODY;

    class HttpSession : public std::enable_shared_from_this<HttpSession> {
    private:
        HttpClient &_http_client;
        boost::asio::strand _strand;
        std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
        boost::asio::streambuf _buffer;
        std::shared_ptr<HttpRequest> _http_request;
        boost::asio::deadline_timer _write_timer;
        std::shared_ptr<HttpResponse> _http_response;
        boost::asio::deadline_timer _read_timer;

    public:
        HttpSession(HttpClient &http_client, const std::shared_ptr<boost::asio::ip::tcp::socket> &socket,
                    const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response);

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

        void read_response_body_chunk(long chunk_size = -1);

        void read_response_body_chunk_handler(const boost::system::error_code &err, size_t bytes_transferred, long chunk_size);

        void read_response_body_old();

        void read_response_body_old_handler(const boost::system::error_code &err, size_t bytes_transferred);

        void timer_handler(const boost::system::error_code &err);
    };

    boost::asio::ip::tcp::resolver _resolver;

public:
    HttpClient(size_t concurrency = 1);

    ~HttpClient();

    virtual void run() override;

    virtual void solve(std::shared_ptr<HttpRequest> http_request, std::shared_ptr<HttpResponse> http_response) override;

    void solve_handler(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response);

    void resolve(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response);

    void resolve_handler(const boost::system::error_code &err, boost::asio::ip::tcp::resolver::iterator iterator,
                         const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response);

    void connect(boost::asio::ip::tcp::resolver::iterator iterator, const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response);

    void connect_handler(const boost::system::error_code &err, boost::asio::ip::tcp::resolver::iterator iterator, std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                         const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response);
};
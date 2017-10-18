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

#include "http_server.h"

#include <boost/bind.hpp>

#include <iostream>
#include <algorithm>
#include <regex>

enum method_type {
    CONNECT,
    DELETE,
    GET,
    HEAD,
    OPTIONS,
    PATCH,
    POST,
    PUT,
    TRACE,
};

std::map<std::string, method_type> available_methods {
        {"DELETE", DELETE},
        {"HEAD", HEAD},
        {"GET", GET},
        {"OPTIONS", OPTIONS},
        {"PATCH", PATCH},
        {"POST", POST},
        {"PUT", PUT},
        {"TRACE", TRACE}
};

HttpServer::HttpSession::HttpSession(HttpServer &http_server, const std::shared_ptr<boost::asio::ip::tcp::socket> &socket)
        : _http_server(http_server)
        , _strand(http_server._ios)
        , _socket(socket)
        , _read_timer(http_server._ios)
        , _write_timer(http_server._ios) {
}

void HttpServer::HttpSession::setHttpResponse(const std::shared_ptr<HttpResponse> &http_response) {
    _http_response = http_response;
}

void HttpServer::HttpSession::start() {
    _start = std::chrono::steady_clock::now();
    _http_request = std::make_shared<HttpRequest>();
    read_request_header();
}

void HttpServer::HttpSession::notify() {
    _write_timer.cancel();
}

void HttpServer::HttpSession::read_request_header() {
    _read_timer.expires_from_now(global::DEFAULT_TIMEOUT_READ_HTTP_HEADER);
    _read_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), _1)));
    boost::asio::async_read_until(*_socket, _read_buffer, "\r\n\r\n",
                                  _strand.wrap(boost::bind(&HttpSession::read_request_header_handler,
                                                           shared_from_this(), _1, _2)));
}

void HttpServer::HttpSession::read_request_header_handler(const boost::system::error_code &err, size_t bytes_transferred) {
    if (!err) {
        _read_timer.cancel();
        size_t additional_bytes = _read_buffer.size() - bytes_transferred;

        std::istream is(&_read_buffer);
        std::string header_line;
        size_t delimiter_index;

        std::string url;
        std::getline(is, header_line);
        if ((delimiter_index = header_line.find(' ')) != std::string::npos) {
            _http_request->set_method(header_line.substr(0, delimiter_index));
            size_t last_delimiter_index = delimiter_index + 1;
            if ((delimiter_index = header_line.find(' ', last_delimiter_index)) != std::string::npos) {
                url = header_line.substr(last_delimiter_index, delimiter_index - last_delimiter_index);
                last_delimiter_index = delimiter_index + 1;
                if ((delimiter_index = header_line.find('\r', last_delimiter_index)) != std::string::npos) {
                    _http_request->set_version(header_line.substr(last_delimiter_index, delimiter_index - last_delimiter_index));
                } else {
                    return;
                }
            } else {
                return;
            }
        } else {
            return;
        }

        std::regex pattern("^(?:https?://)?(?:[^/]+?(?::\\d+)?)?(/.*?(\\.[^.?]+?)?)(\\?.*)?$", std::regex_constants::icase);
        std::smatch results;
        if (url.size() <= 4096 && std::regex_search(url, results, pattern)) {
            _http_request->set_path(results[1]);
            _http_request->set_extension(results[2]);
            _http_request->set_query(results[3]);
        } else {
            return;
        }

        std::getline(is, header_line);
        while ((delimiter_index = header_line.find(':')) != std::string::npos) {
            std::string name = header_line.substr(0, delimiter_index);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            std::string value = header_line.substr(delimiter_index + 1, header_line.size() - delimiter_index - 2);
            value.erase(0, value.find_first_not_of(' '));
            _http_request->set_field(name, value);
            std::getline(is, header_line);
        }

        if (_http_request->has_minimal_requirements()) {
            _http_request->is_parsed(true);
            auto it = available_methods.find(_http_request->get_method());
            if (it != available_methods.end()) {
                _http_server._http_sink->fromHttpSource(_http_request);
                switch (it->second) {
                    default:
                    case method_type::DELETE:
                    case method_type::HEAD:
                    case method_type::GET:
                    case method_type::OPTIONS:
                        { // block for RAII
                            std::lock_guard<std::mutex> lock(_http_server._map_mutex);
                            _http_server._waiting_sessions.emplace(_http_request, shared_from_this());
                        }
                        _write_timer.expires_from_now(global::DEFAULT_TIMEOUT);
                        _write_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::write_response, shared_from_this())));
                        _http_request->getRawStream()->is_completed(true);
                        break;
                    case method_type::PATCH:
                    case method_type::POST:
                    case method_type::PUT:
                    case method_type::TRACE:
                        if (!_http_request->get_field("content-length").empty()) {
                            if(additional_bytes > 0) {
                                _http_request->getRawStream()->append_raw_data(is);
                            }
                            read_request_body(std::stoul(_http_request->get_field("content-length")) - additional_bytes);
                        } else if(_http_request->get_field("transfer-encoding").find("chunked") != std::string::npos){
                            read_request_body_chunk();
                        } else {
                            { // block for RAII
                                std::lock_guard<std::mutex> lock(_http_server._map_mutex);
                                _http_server._waiting_sessions.emplace(_http_request, shared_from_this());
                            }
                            _write_timer.expires_from_now(global::DEFAULT_TIMEOUT);
                            _write_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::write_response, shared_from_this())));
                            _http_request->getRawStream()->is_completed(true);
                        }
                        break;
                }
            } else {
                _http_response = std::make_shared<HttpResponse>();
                std::string body = "Sorry, this method is not implemented";
                _http_response->set_version("HTTP/1.1");
                _http_response->set_status_code("501");
                _http_response->set_reason("Not Implemented");
                _http_response->set_field("connection", "close");
                _http_response->set_field("content-type", "text/plain");
                _http_response->getRawStream()->append_raw_data(body);
                _http_response->set_field("content-length", std::to_string(body.size()));
                _http_response->is_parsed(true);
                _http_response->getRawStream()->is_completed(true);
                write_response();
            }
        }
    }
}

void HttpServer::HttpSession::read_request_body(size_t remaining_bytes) {
    if (remaining_bytes > 0) {
        _read_timer.expires_from_now(global::DEFAULT_TIMEOUT_READ_HTTP_BODY);
        _read_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), _1)));
        boost::asio::async_read(*_socket, _read_buffer, boost::asio::transfer_at_least(1),
                                _strand.wrap(boost::bind(&HttpSession::read_request_body_handler, shared_from_this(),
                                                         _1, _2, remaining_bytes)));
    } else {
        { // block for RAII
            std::lock_guard<std::mutex> lock(_http_server._map_mutex);
            _http_server._waiting_sessions.emplace(std::shared_ptr<HttpRequest>(_http_request), shared_from_this());
        }
        _write_timer.expires_from_now(global::DEFAULT_TIMEOUT);
        _write_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::write_response, shared_from_this())));
        _http_request->getRawStream()->is_completed(true);
    }
}

void HttpServer::HttpSession::read_request_body_handler(const boost::system::error_code &err, size_t bytes_transferred, size_t remaining_bytes) {
    if (!err) {
        _read_timer.cancel();
        _write_timer.expires_from_now(global::DEFAULT_TIMEOUT);
        _http_request->getRawStream()->append_raw_data(&_read_buffer);
        read_request_body(remaining_bytes - bytes_transferred);
    } else {
        _http_request->getRawStream()->is_aborted(true);
    }
}

void HttpServer::HttpSession::read_request_body_chunk(long chunk_size) {
    if(chunk_size > 0) {
        //handler need more data
        _read_timer.expires_from_now(global::DEFAULT_TIMEOUT_READ_HTTP_BODY);
        _read_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), _1)));
        boost::asio::async_read(*_socket, _read_buffer, boost::asio::transfer_at_least(1),
                                _strand.wrap(boost::bind(&HttpSession::read_request_body_chunk_handler,
                                                         shared_from_this(), _1, _2, chunk_size)));
    } else if(chunk_size < 0) {
        //find next chunck size
        _read_timer.expires_from_now(global::DEFAULT_TIMEOUT_READ_HTTP_BODY);
        _read_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), _1)));
        boost::asio::async_read_until(*_socket, _read_buffer, "\r\n",
                                      _strand.wrap(boost::bind(&HttpSession::read_request_body_chunk_handler,
                                                               shared_from_this(), _1, _2, chunk_size)));
    } else {
        //only possible when chunck size = 0, meaning the end of the body
        { // block for RAII
            std::lock_guard<std::mutex> lock(_http_server._map_mutex);
            _http_server._waiting_sessions.emplace(_http_request, shared_from_this());
        }
        _write_timer.expires_from_now(global::DEFAULT_TIMEOUT);
        _write_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::write_response, shared_from_this())));
        _http_request->getRawStream()->is_completed(true);
    }
}

void HttpServer::HttpSession::read_request_body_chunk_handler(const boost::system::error_code &err, size_t bytes_transferred, long chunk_size) {
    if (!err) {
        _read_timer.cancel();
        std::istream is(&_read_buffer);
        std::string line;

        if(chunk_size < 0) {
            std::getline(is, line);
            chunk_size = std::stol(line, 0, 16);
            line += "\n";
            _http_request->getRawStream()->append_raw_data(line);
        }

        if(_read_buffer.size() >= chunk_size + 2) {
            char buffer[chunk_size + 2];
            is.read(buffer, chunk_size + 2);
            _http_request->getRawStream()->append_raw_data(buffer, chunk_size + 2);
            read_request_body_chunk(-chunk_size);
        } else {
            read_request_body_chunk(chunk_size);
        }
    } else {
        _http_request->getRawStream()->is_aborted(true);
    }
}

void HttpServer::HttpSession::write_response() {
    if (_http_response) {
        write_response_header();
    } else {
        _http_response = std::make_shared<HttpResponse>();
        std::string body = _http_request->get_field("host") + _http_request->get_path() + " takes too much time";
        _http_response->set_version("HTTP/1.1");
        _http_response->set_status_code("504");
        _http_response->set_reason("Gateway Time-out");
        _http_response->set_field("connection", "close");
        _http_response->set_field("content-type", "text/plain");
        _http_response->getRawStream()->append_raw_data(body);
        _http_response->set_field("content-length", std::to_string(body.size()));
        _http_response->is_parsed(true);
        _http_response->getRawStream()->is_completed(true);
        write_response_header();
    }
}

void HttpServer::HttpSession::write_response_header() {
    if (_http_response->is_parsed()) {
        boost::asio::async_write(*_socket, boost::asio::buffer(_http_response->make_header()),
                                 _strand.wrap(boost::bind(&HttpSession::write_response_header_handler,
                                                          shared_from_this(), _1, _2)));
    } else if (!_http_response->getRawStream()->is_aborted()) {
        _write_timer.expires_from_now(global::DEFAULT_WAIT_REDO);
        _write_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::write_response_header, shared_from_this())));
    }
}

void HttpServer::HttpSession::write_response_header_handler(const boost::system::error_code &err, size_t bytes_transferred) {
    if(!err) {
        write_response_body(0);
    } else {
        _http_response->getRawStream()->is_aborted(true);
    }
}

void HttpServer::HttpSession::write_response_body(size_t total_bytes_transferred) {
    long read_bytes = _http_response->getRawStream()->readRawData(total_bytes_transferred, _write_buffer, global::DEFAULT_BUFFER_SIZE);
    if (read_bytes > 0) {
        boost::asio::async_write(*_socket, boost::asio::buffer(_write_buffer, read_bytes),
                                 _strand.wrap(boost::bind(&HttpSession::write_response_body_handler, shared_from_this(),
                                                          _1, _2, total_bytes_transferred)));
    } else if (read_bytes < 0) { //not enough data in response stream
        _write_timer.expires_from_now(global::DEFAULT_WAIT_REDO);
        _write_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::write_response_body, shared_from_this(), total_bytes_transferred)));
    } else { //response completed
        _http_server.log(_http_request->get_method() + "\t" + _http_response->get_status_code() + "\t" +
                         _http_request->get_field("host") + _http_request->get_path() + _http_request->get_query() + "\t" +
                         std::to_string(total_bytes_transferred) + "\t" +
                         std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _start).count()));
        if(_http_response->get_field("connection") != "close") {
            start();
        }
    }
}

void HttpServer::HttpSession::write_response_body_handler(const boost::system::error_code &err, size_t bytes_transferred, size_t total_bytes_transferred) {
    if (!err) {
        write_response_body(total_bytes_transferred + bytes_transferred);
    } else {
        _http_response->getRawStream()->is_aborted(true);
    }
}

void HttpServer::HttpSession::timer_handler(const boost::system::error_code &err) {
    if (!err) {
        _http_request->getRawStream()->is_aborted(true);
        if(_http_response) {
            _http_response->getRawStream()->is_aborted(true);
        }
        _socket->close();
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

HttpServer::HttpServer(unsigned short port, size_t concurrency)
        : Module(concurrency)
        , _file("http_server_logs.txt", std::ofstream::out | std::ofstream::trunc)
        , _start(std::chrono::steady_clock::now())
        , _acceptor(_ios, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port), true) {

}

void HttpServer::run() {
    accept();
}

void HttpServer::accept() {
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(_ios);
    _acceptor.async_accept(*socket, boost::bind(&HttpServer::accept_handler, this, _1, socket));
}

void HttpServer::fromHttpSink(const std::shared_ptr<HttpRequest> &http_request,
                              const std::shared_ptr<HttpResponse> &http_response) {
    _ios.post(boost::bind(&HttpServer::fromHttpSinkHandler, this, http_request, http_response));
}

void HttpServer::log(const std::string &line) {
    std::lock_guard<std::mutex> lock(_file_mutex);
    _file << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _start).count()
          << "\t" << line << std::endl;
}

void HttpServer::accept_handler(const boost::system::error_code &err, const std::shared_ptr<boost::asio::ip::tcp::socket> &socket) {
    if (!err) {
        accept();
        std::make_shared<HttpSession>(*this, socket)->start();
    }
}

void HttpServer::fromHttpSinkHandler(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response) {
    std::lock_guard<std::mutex> lock(_map_mutex);
    auto it = _waiting_sessions.find(http_request);
    if (it != _waiting_sessions.end()) {
        it->second->setHttpResponse(http_response);
        it->second->notify();
        _waiting_sessions.erase(it);
    }
}

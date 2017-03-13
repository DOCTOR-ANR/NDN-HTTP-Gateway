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

std::map<std::string, HttpServer::method_type> HttpServer::available_methods = {
        {"HEAD", method_type::HEAD},
        {"GET",  method_type::GET},
        {"POST", method_type::POST},
        {"PUT",  method_type::PUT},
};

HttpServer::HttpSession::HttpSession(HttpServer &http_server, std::shared_ptr<boost::asio::ip::tcp::socket> socket)
        : _http_server(http_server)
        , _strand(http_server._ios)
        , _socket(socket)
        , _read_timer(http_server._ios)
        , _write_timer(http_server._ios) {
    _http_request = std::make_shared<HttpRequest>();
    _http_response = std::make_shared<HttpResponse>();
}

HttpServer::HttpSession::~HttpSession() {

}

void HttpServer::HttpSession::start() {
    read_request_header();
}

void HttpServer::HttpSession::read_request_header() {
    _read_timer.expires_from_now(DEFAULT_TIMEOUT_READ_HTTP_HEADER);
    _read_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), boost::asio::placeholders::error)));
    boost::asio::async_read_until(*_socket, _buffer, "\r\n\r\n", _strand.wrap(boost::bind(&HttpSession::read_request_header_handler,
                                                                                                         shared_from_this(),
                                                                                                         boost::asio::placeholders::error,
                                                                                                         boost::asio::placeholders::bytes_transferred)));
}

void HttpServer::HttpSession::read_request_header_handler(const boost::system::error_code &err, size_t bytes_transferred) {
    if (!err) {
        _read_timer.cancel();
        size_t additional_bytes = _buffer.size() - bytes_transferred;

        std::istream is(&_buffer);
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

        std::regex pattern("^(?:https?://)?(?:[^/]+?(?::\\d+)?)?(/.*?(\\..+?)?)(\\?.*)?$",
                           std::regex_constants::icase);
        std::smatch results;
        if (std::regex_search(url, results, pattern)) {
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
                _http_server._http_sink->solve(_http_request, _http_response);
                switch (it->second) {
                    default:
                    case method_type::HEAD:
                    case method_type::GET:
                        _http_request->get_raw_stream()->is_completed(true);
                        break;
                    case method_type::POST:
                    case method_type::PUT:
                        if (!_http_request->get_field("content-length").empty()) {
                            if(additional_bytes > 0) {
                                _http_request->get_raw_stream()->append_raw_data(is);
                            }
                            read_request_body(std::stoul(_http_request->get_field("content-length")) - additional_bytes);
                        } else if(_http_request->get_field("transfer-encoding").find("chunked") != std::string::npos){
                            read_request_body_chuncked();
                        } else {
                            _http_request->get_raw_stream()->is_completed(true);
                        }
                        break;
                }
            } else {
                _http_response->get_raw_stream()->append_raw_data("HTTP/1.1 501 Not Implemented\r\nContent-Length: 37\r\nContent-Type: text/plain\r\n\r\n"
                                                                         "Sorry, this method is not implemented");
                _http_response->get_raw_stream()->is_completed(true);
            }
            write_response();
        }
    }
}

void HttpServer::HttpSession::read_request_body(size_t remaining_bytes) {
    if (remaining_bytes > 0) {
        _read_timer.expires_from_now(DEFAULT_TIMEOUT_READ_HTTP_BODY);
        _read_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), boost::asio::placeholders::error)));
        boost::asio::async_read(*_socket, _buffer, boost::asio::transfer_at_least(1), _strand.wrap(boost::bind(&HttpSession::read_request_body_handler,
                                                                                                              shared_from_this(),
                                                                                                              boost::asio::placeholders::error,
                                                                                                              boost::asio::placeholders::bytes_transferred,
                                                                                                              remaining_bytes)));
    } else {
        _http_request->get_raw_stream()->is_completed(true);
    }
}

void HttpServer::HttpSession::read_request_body_handler(const boost::system::error_code &err, size_t bytes_transferred,
                                                        size_t remaining_bytes) {
    if (!err) {
        _read_timer.cancel();
        std::istream is(&_buffer);
        _http_request->get_raw_stream()->append_raw_data(is);
        read_request_body(remaining_bytes - bytes_transferred);
    } else {
        _http_request->get_raw_stream()->is_aborted(true);
    }
}

void HttpServer::HttpSession::read_request_body_chuncked(long chunck_size) {
    if(chunck_size > 0) {
        //handler need more data
        _read_timer.expires_from_now(DEFAULT_TIMEOUT_READ_HTTP_BODY);
        _read_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), boost::asio::placeholders::error)));
        boost::asio::async_read(*_socket, _buffer, boost::asio::transfer_at_least(1), _strand.wrap(boost::bind(&HttpSession::read_request_body_chuncked_handler,
                                                                                                               shared_from_this(),
                                                                                                               boost::asio::placeholders::error,
                                                                                                               boost::asio::placeholders::bytes_transferred,
                                                                                                               chunck_size)));
    } else if(chunck_size == -1) {
        //find next chunck size
        _read_timer.expires_from_now(DEFAULT_TIMEOUT_READ_HTTP_BODY);
        _read_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), boost::asio::placeholders::error)));
        boost::asio::async_read_until(*_socket, _buffer, "\r\n", _strand.wrap(boost::bind(&HttpSession::read_request_body_chuncked_handler,
                                                                                          shared_from_this(),
                                                                                          boost::asio::placeholders::error,
                                                                                          boost::asio::placeholders::bytes_transferred,
                                                                                          chunck_size)));
    } else {
        //only possible when chunck size = 0, meaning the end of the body
        _http_request->get_raw_stream()->is_completed(true);
    }
}

void HttpServer::HttpSession::read_request_body_chuncked_handler(const boost::system::error_code &err, size_t bytes_transferred, long chunck_size) {
    if (!err) {
        _read_timer.cancel();
        std::istream is(&_buffer);
        std::string line;

        if(chunck_size == -1) {
            std::getline(is, line);
            chunck_size = std::stol(line, 0, 16);
            line += "\n";
            _http_request->get_raw_stream()->append_raw_data(line);
        }

        if(_buffer.size() >= chunck_size + 2) {
            char buffer[chunck_size + 2];
            is.read(buffer, chunck_size + 2);
            _http_request->get_raw_stream()->append_raw_data(buffer, chunck_size + 2);
            read_request_body_chuncked();
        } else {
            read_request_body_chuncked(chunck_size);
        }
    } else {
        _http_request->get_raw_stream()->is_aborted(true);
    }
}

void HttpServer::HttpSession::write_response() {
    char buffer[4096];
    long read_bytes = _http_response->get_raw_stream()->read_raw_data(buffer, 4096);
    if (read_bytes > 0) {
        boost::asio::async_write(*_socket, boost::asio::buffer(buffer, read_bytes), _strand.wrap(boost::bind(&HttpSession::write_response_handler,
                                                                                       shared_from_this(),
                                                                                       boost::asio::placeholders::error,
                                                                                       boost::asio::placeholders::bytes_transferred)));
    } else if (read_bytes < 0) { //not enough data in response stream
        _write_timer.expires_from_now(global::DEFAULT_WAIT_REDO);
        _write_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::write_response, shared_from_this())));
    } else { //request completed
        return;
    }
}

void HttpServer::HttpSession::write_response_handler(const boost::system::error_code &err, size_t bytes_transferred) {
    if (!err) {
        write_response();
    }
}

void HttpServer::HttpSession::timer_handler(const boost::system::error_code &err) {
    if (!err) {
        _socket->close();
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

boost::posix_time::seconds HttpServer::DEFAULT_TIMEOUT_READ_HTTP_HEADER = boost::posix_time::seconds(10);

boost::posix_time::seconds HttpServer::DEFAULT_TIMEOUT_READ_HTTP_BODY = boost::posix_time::seconds(2);

HttpServer::HttpServer(unsigned short port, size_t concurrency)
        : Module(concurrency), _acceptor(_ios, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port), true) {

}

HttpServer::~HttpServer() {

}

void HttpServer::run() {
    accept();
}

void HttpServer::accept() {
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(_ios);
    _acceptor.async_accept(*socket, boost::bind(&HttpServer::accept_handler, this, boost::asio::placeholders::error, socket));
}

void HttpServer::accept_handler(const boost::system::error_code &err, std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
    if (!err) {
        accept();
        std::make_shared<HttpSession>(*this, socket)->start();
    }
}

void HttpServer::attach_http_sink(HttpSink *http_sink) {
    _http_sink = http_sink;
}

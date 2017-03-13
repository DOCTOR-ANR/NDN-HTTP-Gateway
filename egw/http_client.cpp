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

#include "http_client.h"

HttpClient::HttpSession::HttpSession(HttpClient &http_client, const std::shared_ptr<boost::asio::ip::tcp::socket> &socket,
                                     const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response)
        : _http_client(http_client)
        , _strand(http_client._ios)
        , _socket(socket)
        , _http_request(http_request)
        , _write_timer(http_client._ios)
        , _http_response(http_response)
        , _read_timer(http_client._ios) {

}

HttpClient::HttpSession::~HttpSession() {

}

void HttpClient::HttpSession::start() {
    write_request_body();
}

void HttpClient::HttpSession::write_request_header() {
    if(_http_request->is_parsed()) {
        boost::asio::async_write(*_socket, boost::asio::buffer(_http_request->make_header()), _strand.wrap(boost::bind(&HttpSession::write_request_header_handler,
                                                                                                                       shared_from_this(),
                                                                                                                       boost::asio::placeholders::error,
                                                                                                                       boost::asio::placeholders::bytes_transferred)));
    } else {
        _write_timer.expires_from_now(global::DEFAULT_WAIT_REDO);
        _write_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::write_request_header, shared_from_this())));
    }
}

void HttpClient::HttpSession::write_request_header_handler(const boost::system::error_code &err, size_t bytes_transferred) {
    if(!err){
        write_request_body();
    } else {
        _http_response->get_raw_stream()->is_aborted(true);
    }
}

void HttpClient::HttpSession::write_request_body() {
    char buffer[global::DEFAULT_BUFFER_SIZE];
    //std::string buffer;
    long read_bytes = _http_request->get_raw_stream()->read_raw_data(buffer, global::DEFAULT_BUFFER_SIZE);
    if (read_bytes > 0) {
        boost::asio::async_write(*_socket, boost::asio::buffer(buffer, read_bytes), _strand.wrap(boost::bind(&HttpSession::write_request_body_handler,
                                                                                                            shared_from_this(),
                                                                                                            boost::asio::placeholders::error,
                                                                                                            boost::asio::placeholders::bytes_transferred)));
    } else if (read_bytes < 0) { //not enough data in response stream
        _write_timer.expires_from_now(global::DEFAULT_WAIT_REDO);
        _write_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::write_request_body, shared_from_this())));
    } else { //request completed
        read_response_header();
    }
}

void HttpClient::HttpSession::write_request_body_handler(const boost::system::error_code &err, size_t bytes_transferred) {
    if (!err) {
        write_request_body();
    } else {
        _http_response->get_raw_stream()->is_aborted(true);
    }
}

void HttpClient::HttpSession::read_response_header() {
    _read_timer.expires_from_now(DEFAULT_TIMEOUT_READ_HTTP_HEADER);
    _read_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, this, boost::asio::placeholders::error)));
    boost::asio::async_read_until(*_socket, _buffer, "\r\n\r\n", _strand.wrap(boost::bind(&HttpSession::read_response_header_handler,
                                                                                                        shared_from_this(),
                                                                                                        boost::asio::placeholders::error,
                                                                                                        boost::asio::placeholders::bytes_transferred)));
}

void HttpClient::HttpSession::read_response_header_handler(const boost::system::error_code &err, size_t bytes_transferred) {
    if(!err){
        _read_timer.cancel();
        size_t additional_bytes = _buffer.size() - bytes_transferred;

        std::istream is(&_buffer);
        std::string header_line;
        size_t delimiter_index;

        std::getline(is, header_line);
        if ((delimiter_index = header_line.find(' ')) != std::string::npos) {
            _http_response->set_version(header_line.substr(0, delimiter_index));
            size_t last_delimiter_index = delimiter_index + 1;
            if ((delimiter_index = header_line.find(' ', last_delimiter_index)) != std::string::npos) {
                _http_response->set_status_code(header_line.substr(last_delimiter_index, delimiter_index - last_delimiter_index));
                last_delimiter_index = delimiter_index + 1;
                if ((delimiter_index = header_line.find('\r', last_delimiter_index)) != std::string::npos) {
                    _http_response->set_reason(header_line.substr(last_delimiter_index, delimiter_index - last_delimiter_index));
                } else {
                    _http_response->get_raw_stream()->is_aborted(true);
                    return;
                }
            } else {
                _http_response->get_raw_stream()->is_aborted(true);
                return;
            }
        } else {
            _http_response->get_raw_stream()->is_aborted(true);
            return;
        }

        std::getline(is, header_line);
        while ((delimiter_index = header_line.find(':')) != std::string::npos) {
            std::string name = header_line.substr(0, delimiter_index);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            std::string value = header_line.substr(delimiter_index + 1, header_line.size() - delimiter_index - 2);
            value.erase(0, value.find_first_not_of(' '));
            _http_response->set_field(name, value);
            std::getline(is, header_line);
        }

        if(_http_response->has_minimal_requirements()){
            _http_response->is_parsed(true);
            if(_http_request->get_method() != "HEAD") {
                if (!_http_response->get_field("content-length").empty()) {
                    if(additional_bytes > 0) {
                        _http_response->get_raw_stream()->append_raw_data(is);
                    }
                    read_response_body(std::stoul(_http_response->get_field("content-length")) - additional_bytes);
                } else if (_http_response->get_field("transfer-encoding").find("chunked") != std::string::npos) {
                    read_response_body_chunk();
                } else if (_http_response->get_version() == "HTTP/1.0" || _http_response->get_field("connection") == "close") {
                    if(additional_bytes > 0) {
                        _http_response->get_raw_stream()->append_raw_data(is);
                    }
                    read_response_body_old();
                } else {
                    _http_response->get_raw_stream()->is_completed(true);
                }
            } else {
                _http_response->get_raw_stream()->is_completed(true);
            }
        } else {
            _http_response->get_raw_stream()->is_aborted(true);
        }
    } else {
        _http_response->get_raw_stream()->is_aborted(true);
    }
}

void HttpClient::HttpSession::read_response_body(long remaining_bytes) {
    if (remaining_bytes > 0) {
        _read_timer.expires_from_now(DEFAULT_TIMEOUT_READ_HTTP_BODY);
        _read_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), boost::asio::placeholders::error)));
        boost::asio::async_read(*_socket, _buffer, boost::asio::transfer_at_least(1), _strand.wrap(boost::bind(&HttpSession::read_response_body_handler,
                                                                                                               shared_from_this(),
                                                                                                               boost::asio::placeholders::error,
                                                                                                               boost::asio::placeholders::bytes_transferred,
                                                                                                               remaining_bytes)));
    } else {
        _http_response->get_raw_stream()->is_completed(true);
    }
}

void HttpClient::HttpSession::read_response_body_handler(const boost::system::error_code &err, size_t bytes_transferred, long remaining_bytes) {
    if (!err) {
        _read_timer.cancel();
        std::istream is(&_buffer);
        _http_response->get_raw_stream()->append_raw_data(is);
        read_response_body(remaining_bytes - bytes_transferred);
    } else {
        _http_response->get_raw_stream()->is_aborted(true);
    }
}

void HttpClient::HttpSession::read_response_body_chunk(long chunk_size) {
    if(chunk_size > 0) {
        //handler need more data
        _read_timer.expires_from_now(DEFAULT_TIMEOUT_READ_HTTP_BODY);
        _read_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), boost::asio::placeholders::error)));
        boost::asio::async_read(*_socket, _buffer, boost::asio::transfer_at_least(1), _strand.wrap(boost::bind(&HttpSession::read_response_body_chunk_handler,
                                                                                                               shared_from_this(),
                                                                                                               boost::asio::placeholders::error,
                                                                                                               boost::asio::placeholders::bytes_transferred,
                                                                                                               chunk_size)));
    } else if(chunk_size < 0) {
        //find next chunk size
        _read_timer.expires_from_now(DEFAULT_TIMEOUT_READ_HTTP_BODY);
        _read_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), boost::asio::placeholders::error)));
        boost::asio::async_read_until(*_socket, _buffer, "\r\n", _strand.wrap(boost::bind(&HttpSession::read_response_body_chunk_handler,
                                                                                          shared_from_this(),
                                                                                          boost::asio::placeholders::error,
                                                                                          boost::asio::placeholders::bytes_transferred,
                                                                                          chunk_size)));
    } else {
        //only possible when chunk size = 0, meaning the end of the body
        _http_response->get_raw_stream()->is_completed(true);
    }
}

void HttpClient::HttpSession::read_response_body_chunk_handler(const boost::system::error_code &err, size_t bytes_transferred, long chunk_size) {
    if (!err) {
        _read_timer.cancel();
        std::istream stream(&_buffer);
        std::string line;

        if(chunk_size == -1) {
            std::getline(stream, line);
            chunk_size = std::stol(line, 0, 16);
            line += "\n";
            _http_response->get_raw_stream()->append_raw_data(line);
        }

        if(chunk_size == 0 || _buffer.size() < chunk_size + 2) {
            read_response_body_chunk(chunk_size);
        } else {
            char buffer[chunk_size + 2];
            stream.read(buffer, chunk_size + 2);
            _http_response->get_raw_stream()->append_raw_data(buffer, chunk_size + 2);
            read_response_body_chunk();
        }
    } else {
        _http_response->get_raw_stream()->is_aborted(true);
    }
}

void HttpClient::HttpSession::read_response_body_old() {
    _read_timer.expires_from_now(DEFAULT_TIMEOUT_READ_HTTP_BODY);
    _read_timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), boost::asio::placeholders::error)));
    boost::asio::async_read(*_socket, _buffer, boost::asio::transfer_at_least(1), _strand.wrap(boost::bind(&HttpSession::read_response_body_old_handler,
                                                                                                           shared_from_this(),
                                                                                                           boost::asio::placeholders::error,
                                                                                                           boost::asio::placeholders::bytes_transferred)));
}

void HttpClient::HttpSession::read_response_body_old_handler(const boost::system::error_code &err, size_t bytes_transferred) {
    if (!err) {
        _read_timer.cancel();
        std::istream is(&_buffer);
        _http_response->get_raw_stream()->append_raw_data(is);
        read_response_body_old();
    } else if(err == boost::asio::error::eof){
        _http_response->get_raw_stream()->is_completed(true);
    } else {
        _http_response->get_raw_stream()->is_aborted(true);
    }
}

void HttpClient::HttpSession::timer_handler(const boost::system::error_code &err) {
    if (!err) {
        _http_response->get_raw_stream()->is_aborted(true);
        _socket->close();
    }
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------

boost::posix_time::seconds HttpClient::DEFAULT_TIMEOUT_READ_HTTP_HEADER = boost::posix_time::seconds(10);

boost::posix_time::seconds HttpClient::DEFAULT_TIMEOUT_READ_HTTP_BODY = boost::posix_time::seconds(2);

HttpClient::HttpClient(size_t concurrency) : Module(concurrency), _resolver(_ios) {

}

HttpClient::~HttpClient() {

}

void HttpClient::run() {

}

void HttpClient::solve(std::shared_ptr<HttpRequest> http_request, std::shared_ptr<HttpResponse> http_response) {
    _ios.post(boost::bind(&HttpClient::solve_handler, this, http_request, http_response));
}

void HttpClient::solve_handler(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response) {
    resolve(http_request, http_response);
}

void HttpClient::resolve(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response) {
    std::string host = http_request->get_field("host");
    if (!host.empty()) {
        auto delimiter = host.find(":");
        std::string domain = host.substr(0, delimiter);
        std::string port = delimiter != std::string::npos ? host.substr(delimiter + 1) : "http";
        boost::asio::ip::tcp::resolver::query query(domain, port);
        _resolver.async_resolve(query, boost::bind(&HttpClient::resolve_handler,
                                                   this,
                                                   boost::asio::placeholders::error,
                                                   boost::asio::placeholders::iterator,
                                                   http_request,
                                                   http_response));
    } else {
        http_response->get_raw_stream()->is_aborted(true);
    }
}

void HttpClient::resolve_handler(const boost::system::error_code &err, boost::asio::ip::tcp::resolver::iterator iterator,
                            const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response) {
    if(!err) {
        connect(iterator, http_request, http_response);
    } else {
        http_response->get_raw_stream()->is_aborted(true);
    }
}

void HttpClient::connect(boost::asio::ip::tcp::resolver::iterator iterator, const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response) {
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(_ios);
    boost::asio::async_connect(*socket, iterator, boost::bind(&HttpClient::connect_handler,
                                                              this,
                                                              boost::asio::placeholders::error,
                                                              boost::asio::placeholders::iterator,
                                                              socket,
                                                              http_request,
                                                              http_response));
}

void HttpClient::connect_handler(const boost::system::error_code &err, boost::asio::ip::tcp::resolver::iterator iterator, std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                            const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response) {
    if(!err) {
        std::make_shared<HttpSession>(*this, socket, http_request, http_response)->start();
    } else {
        http_response->get_raw_stream()->is_aborted(true);
    }
}

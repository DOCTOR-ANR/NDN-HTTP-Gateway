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

#include "http_client.h"

#include <fstream>

#ifndef NDEBUG
std::atomic<size_t> HttpClient::HttpSession::count {0};
#endif

HttpClient::HttpSession::HttpSession(HttpClient &http_client, const std::shared_ptr<HttpRequest> &http_request)
        : _http_client(http_client)
        , _timer(http_client._ios)
        , _strand(http_client._ios)
        , _socket(http_client._ios)
        , _http_request(http_request) {
#ifndef NDEBUG
    std::cout << "new session (" << ++count << " active session(s))" << std::endl;
#endif
}

HttpClient::HttpSession::~HttpSession() {
#ifndef NDEBUG
    std::cout << "session destroyed (" << --count << " remaining session(s))" << std::endl;
#endif
}

void HttpClient::HttpSession::start() {
    // use custom HTTP header fields
    //_http_request->set_field("user-agent", "Mozilla/5.0 (X11; Ubuntu) NDN_Gateway/0.1");
    //_http_request->set_field("accept", "*/*");
    //std::cout << _http_request->get_field("host") << _http_request->get_path() << std::endl;
    //_http_request->set_field("connection", "close");
    _http_response = std::make_shared<HttpResponse>();
    resolve_domain();
}

void HttpClient::HttpSession::resolve_domain() {
    std::string host = _http_request->get_field("host");
    if (!host.empty()) {
        auto delimiter = host.find(':');
        std::string domain = host.substr(0, delimiter);
        std::string port = delimiter != std::string::npos ? host.substr(delimiter + 1) : "80";
        boost::asio::ip::tcp::resolver::query query(domain, port, boost::asio::ip::tcp::resolver::query::numeric_service);
        _http_client._resolver.async_resolve(query, boost::bind(&HttpSession::resolve_domain_handler, shared_from_this(), _1, _2));
    } else {
        std::cerr << "HTTP request with empty host field" << std::endl;
        auto http_response = std::make_shared<HttpResponse>();
        http_response->getRawStream()->is_aborted(true);
        _http_client._http_source->fromHttpSink(_http_request, http_response);
    }
}

void HttpClient::HttpSession::resolve_domain_handler(const boost::system::error_code &err, boost::asio::ip::tcp::resolver::iterator iterator) {
    if(!err) {
        connect(iterator);
    } else {
        std::cerr << _http_request->get_field("host") << _http_request->get_path() << " -> error while resolving domain" << std::endl;
        auto http_response = std::make_shared<HttpResponse>();
        http_response->getRawStream()->is_aborted(true);
        _http_client._http_source->fromHttpSink(_http_request, http_response);
    }
}

void HttpClient::HttpSession::connect(boost::asio::ip::tcp::resolver::iterator iterator) {
    _timer.expires_from_now(global::DEFAULT_TIMEOUT_CONNECT);
    _timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler,shared_from_this(), _1)));
    boost::asio::async_connect(_socket, iterator, _strand.wrap(boost::bind(&HttpSession::connect_handler,shared_from_this(), _1, _2)));
}

void HttpClient::HttpSession::connect_handler(const boost::system::error_code &err, boost::asio::ip::tcp::resolver::iterator iterator) {
    _timer.cancel();
    if(!err) {
        write_request_header();
    } else if (iterator != boost::asio::ip::tcp::resolver::iterator()) {
        _socket.close();
        connect(++iterator);
    } else {
        std::cerr << _http_request->get_field("host") << _http_request->get_path() << " -> error while connecting to " << _http_request->get_field("host") << std::endl;
        auto http_response = std::make_shared<HttpResponse>();
        http_response->getRawStream()->is_aborted(true);
        _http_client._http_source->fromHttpSink(_http_request, http_response);
    }
}

void HttpClient::HttpSession::write_request_header() {
    if (!_http_request->getRawStream()->is_aborted()) {
        boost::asio::async_write(_socket, boost::asio::buffer(_http_request->make_header()),
                                 boost::bind(&HttpSession::write_request_header_handler, shared_from_this(), _1, _2));
    } else {
        std::cerr << _http_request->get_field("host") << _http_request->get_path() << "is aborted" << std::endl;
        _http_response->getRawStream()->is_aborted(true);
        _http_client._http_source->fromHttpSink(_http_request, _http_response);
    }
}

void HttpClient::HttpSession::write_request_header_handler(const boost::system::error_code &err, size_t bytes_transferred) {
    if(!err){
        write_request_body(0);
    } else {
        std::cerr << _http_request->get_field("host") << _http_request->get_path() << " -> error while sending header" << std::endl;
        _http_response->getRawStream()->is_aborted(true);
        _http_client._http_source->fromHttpSink(_http_request, _http_response);
    }
}

void HttpClient::HttpSession::write_request_body(size_t total_bytes_transferred) {
    if (!_http_request->getRawStream()->is_aborted()) {
        long read_bytes = _http_request->getRawStream()->readRawData(total_bytes_transferred, _write_buffer, global::DEFAULT_BUFFER_SIZE);
        if (read_bytes > 0) {
            boost::asio::async_write(_socket, boost::asio::buffer(_write_buffer, read_bytes),
                                     boost::bind(&HttpSession::write_request_body_handler, shared_from_this(), _1, _2, total_bytes_transferred));
        } else if (read_bytes < 0) {
            //not enough data in request stream
            _timer.expires_from_now(global::DEFAULT_WAIT_REDO);
            _timer.async_wait(boost::bind(&HttpSession::write_request_body, shared_from_this(), total_bytes_transferred));
        } else { //request completed
            read_response_header();
        }
    } else {
        std::cerr << _http_request->get_field("host") << _http_request->get_path() << " is aborted" << std::endl;
        _http_response->getRawStream()->is_aborted(true);
        _http_client._http_source->fromHttpSink(_http_request, _http_response);
    }
}

void HttpClient::HttpSession::write_request_body_handler(const boost::system::error_code &err, size_t bytes_transferred, size_t total_bytes_transferred) {
    if (!err) {
        write_request_body(total_bytes_transferred + bytes_transferred);
    } else {
        std::cerr << _http_request->get_field("host") << _http_request->get_path() << " -> error while sending body" << std::endl;
        _http_response->getRawStream()->is_aborted(true);
        _http_client._http_source->fromHttpSink(_http_request, _http_response);
    }
}

void HttpClient::HttpSession::read_response_header() {
    _timer.expires_from_now(global::DEFAULT_TIMEOUT_READ_HTTP_HEADER);
    _timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), _1)));
    boost::asio::async_read_until(_socket, _read_buffer, "\r\n\r\n",
                                  _strand.wrap(boost::bind(&HttpSession::read_response_header_handler, shared_from_this(), _1, _2)));
}

void
HttpClient::HttpSession::read_response_header_handler(const boost::system::error_code &err, size_t bytes_transferred) {
    _timer.cancel();
    if(!err){
        size_t additional_bytes = _read_buffer.size() - bytes_transferred;

        std::istream is(&_read_buffer);
        std::string header_line;
        size_t delimiter_index;

        std::getline(is, header_line);
        if ((delimiter_index = header_line.find(' ')) != std::string::npos) {
            _http_response->set_version(header_line.substr(0, delimiter_index));
            size_t last_delimiter_index = delimiter_index + 1;
            if ((delimiter_index = header_line.find(' ', last_delimiter_index)) != std::string::npos) {
                _http_response->set_status_code(header_line.substr(last_delimiter_index, delimiter_index - last_delimiter_index));
                last_delimiter_index = delimiter_index + 1;
                _http_response->set_reason(header_line.substr(last_delimiter_index, (header_line.length() - last_delimiter_index) - 1));
            } else {
                _http_response->getRawStream()->is_aborted(true);
                _http_client._http_source->fromHttpSink(_http_request, _http_response);
                return;
            }
        } else {
            _http_response->getRawStream()->is_aborted(true);
            _http_client._http_source->fromHttpSink(_http_request, _http_response);
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

        if(_http_response->has_minimal_requirements()) {
            _http_response->is_parsed(true);
            _http_client._http_source->fromHttpSink(_http_request, _http_response);
            if(_http_request->get_method() != "HEAD") {
                if (!_http_response->get_field("content-length").empty()) {
                    if(additional_bytes > 0) {
                        _http_response->getRawStream()->append_raw_data(is);
                    }
                    read_response_body(std::stoul(_http_response->get_field("content-length")) - additional_bytes);
                } else if (_http_response->get_field("transfer-encoding") == "chunked") {
                    read_response_body_chunk(-1);
                } else if (_http_response->get_version() == "HTTP/1.0" || _http_response->get_field("connection") == "close") {
                    if(additional_bytes > 0) {
                        _http_response->getRawStream()->append_raw_data(is);
                    }
                    read_response_body_old();
                    // response without body
                } else {
                    _http_response->getRawStream()->is_completed(true);
                }
                // response to HEAD request
            } else {
                _http_response->getRawStream()->is_completed(true);
            }
            // response header does not fit requirements
        } else {
            std::cerr << _http_request->get_field("host") << _http_request->get_path() << " -> ill-formed response header " << std::endl;
            _http_response->getRawStream()->is_aborted(true);
            _http_client._http_source->fromHttpSink(_http_request, _http_response);
        }
    } else {
        std::cerr << _http_request->get_field("host") << _http_request->get_path() << " -> error while receiving header" << std::endl;
        _http_response->getRawStream()->is_aborted(true);
        _http_client._http_source->fromHttpSink(_http_request, _http_response);
    }
}

void HttpClient::HttpSession::read_response_body(long remaining_bytes) {
    if (remaining_bytes > 0) {
        _timer.expires_from_now(global::DEFAULT_TIMEOUT_READ_HTTP_BODY);
        _timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), _1)));
        boost::asio::async_read(_socket, _read_buffer, boost::asio::transfer_at_least(1),
                                _strand.wrap(boost::bind(&HttpSession::read_response_body_handler, shared_from_this(), _1, _2, remaining_bytes)));
    } else {
        _http_response->getRawStream()->is_completed(true);
    }
}

void HttpClient::HttpSession::read_response_body_handler(const boost::system::error_code &err, size_t bytes_transferred, long remaining_bytes) {
    _timer.cancel();
    if (!err) {
        _http_response->getRawStream()->append_raw_data(&_read_buffer);
        read_response_body(remaining_bytes - bytes_transferred);
    } else {
        _http_response->getRawStream()->is_aborted(true);
    }
}

void HttpClient::HttpSession::read_response_body_chunk(long chunk_size) {
    if(chunk_size > 0) {
        // handler needs more data
        _timer.expires_from_now(global::DEFAULT_TIMEOUT_READ_HTTP_BODY);
        _timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), _1)));
        boost::asio::async_read(_socket, _read_buffer, boost::asio::transfer_at_least(1),
                                _strand.wrap(boost::bind(&HttpSession::read_response_body_chunk_handler, shared_from_this(), _1, _2, chunk_size)));
    } else if(chunk_size < 0) {
        // find next chunk size
        _timer.expires_from_now(global::DEFAULT_TIMEOUT_READ_HTTP_BODY);
        _timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), _1)));
        boost::asio::async_read_until(_socket, _read_buffer, "\r\n",
                                      _strand.wrap(boost::bind(&HttpSession::read_response_body_chunk_handler, shared_from_this(), _1, _2, chunk_size)));
    } else {
        // only possible when chunk size = 0, meaning the end of the body
        _http_response->getRawStream()->is_completed(true);
    }
}

void HttpClient::HttpSession::read_response_body_chunk_handler(const boost::system::error_code &err, size_t bytes_transferred, long chunk_size) {
    _timer.cancel();
    if (!err) {
        std::istream stream(&_read_buffer);
        std::string line;

        if(chunk_size < 0) {
            std::getline(stream, line);
            chunk_size = std::stol(line, 0, 16);
            line += "\n";
            _http_response->getRawStream()->append_raw_data(line);
        }

        if(_read_buffer.size() >= chunk_size + 2) {
            char buffer[chunk_size + 2];
            stream.read(buffer, chunk_size + 2);
            _http_response->getRawStream()->append_raw_data(buffer, chunk_size + 2);
            read_response_body_chunk(-chunk_size);
        } else {
            read_response_body_chunk(chunk_size);
        }
    } else {
        _http_response->getRawStream()->is_aborted(true);
    }
}

void HttpClient::HttpSession::read_response_body_old() {
    _timer.expires_from_now(global::DEFAULT_TIMEOUT_READ_HTTP_BODY);
    _timer.async_wait(_strand.wrap(boost::bind(&HttpSession::timer_handler, shared_from_this(), boost::asio::placeholders::error)));
    boost::asio::async_read(_socket, _read_buffer, boost::asio::transfer_at_least(1),
                            _strand.wrap(boost::bind(&HttpSession::read_response_body_old_handler, shared_from_this(), _1, _2)));
}

void HttpClient::HttpSession::read_response_body_old_handler(const boost::system::error_code &err, size_t bytes_transferred) {
    _timer.cancel();
    if (!err) {
        _http_response->getRawStream()->append_raw_data(&_read_buffer);
        read_response_body_old();
    } else if(err == boost::asio::error::eof){
        _http_response->getRawStream()->is_completed(true);
    } else {
        _http_response->getRawStream()->is_aborted(true);
    }
}

void HttpClient::HttpSession::timer_handler(const boost::system::error_code &err) {
    if (!err) {
        _socket.cancel();
        _socket.close();
    }
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------

HttpClient::HttpClient(size_t concurrency) : Module(concurrency), _resolver(_ios) {

}

void HttpClient::run() {

}

void HttpClient::fromHttpSource(const std::shared_ptr<HttpRequest> &http_request) {
    _ios.post(boost::bind(&HttpClient::fromHttpSourceHandler, this, http_request));
}

void HttpClient::fromHttpSourceHandler(const std::shared_ptr<HttpRequest> &http_request) {
    std::make_shared<HttpSession>(*this, http_request)->start();
}
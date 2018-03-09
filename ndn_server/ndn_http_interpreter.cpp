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

#include "ndn_http_interpreter.h"

#include <regex>
#include <algorithm>

const char SAFE[256] = {
/*      0 1 2 3  4 5 6 7  8 9 A B  C D E F */
/* 0 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* 1 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* 2 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* 3 */ 1,1,1,1, 1,1,1,1, 1,1,0,0, 0,0,0,0,

/* 4 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
/* 5 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,0,0,
/* 6 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
/* 7 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,0,0,

/* 8 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* 9 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* A */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* B */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

/* C */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* D */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* E */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
/* F */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
};

std::string uri_encode(const std::string & sSrc){
    const char DEC2HEX[16 + 1] = "0123456789ABCDEF";
    const unsigned char * pSrc = (const unsigned char *)sSrc.c_str();
    const int SRC_LEN = sSrc.length();
    unsigned char * const pStart = new unsigned char[SRC_LEN * 3];
    unsigned char * pEnd = pStart;
    const unsigned char * const SRC_END = pSrc + SRC_LEN;
    for (; pSrc < SRC_END; ++pSrc){
        if (SAFE[*pSrc])*pEnd++ = *pSrc;
        else{
            // escape this char
            *pEnd++ = '%';
            *pEnd++ = DEC2HEX[*pSrc >> 4];
            *pEnd++ = DEC2HEX[*pSrc & 0x0F];
        }
    }
    std::string sResult((char *)pStart, (char *)pEnd);
    delete [] pStart;
    return sResult;
}

NdnHttpInterpreter::NdnHttpInterpreter(size_t concurrency) : Module(concurrency) {

}

void NdnHttpInterpreter::run() {

}

void NdnHttpInterpreter::fromNdnSource(const std::shared_ptr<NdnContent> &ndn_content) {
    _ios.post(boost::bind(&NdnHttpInterpreter::fromNdnSourceHandler, this, ndn_content));
}

void NdnHttpInterpreter::fromHttpSink(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response) {
    _ios.post(boost::bind(&NdnHttpInterpreter::fromHttpSinkHandler, this, http_request, http_response));
}

void NdnHttpInterpreter::fromNdnSourceHandler(const std::shared_ptr<NdnContent> &ndn_content) {
    auto http_request = std::make_shared<HttpRequest>(ndn_content->getRawStream());
    { // block for RAII
        std::lock_guard<std::mutex> lock(_map_mutex);
        _pending_requests.emplace(http_request, ndn_content->getName().get(-1).toUri());
#ifndef NDEBUG
        std::cout << _pending_requests.size() << " remaining request(s)" << std::endl;
#endif
    }
    auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
    get_http_request_header(http_request, timer);
}

void NdnHttpInterpreter::get_http_request_header(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<boost::asio::deadline_timer> &timer) {
    if(!http_request->getRawStream()->is_aborted()) {
        // keep track of completion before doing the job because if set to true and no HTTP header found then discard
        bool is_complete = http_request->getRawStream()->is_completed();

        // look for a HTTP header
        std::string raw_data = http_request->getRawStream()->raw_data_as_string();
        size_t delimiter_index = raw_data.find("\r\n\r\n");
        if (delimiter_index != std::string::npos) {
            // remove the HTTP header from the stream
            http_request->getRawStream()->removeFirstBytes(delimiter_index + 4);

            std::stringstream header(raw_data);
            std::string header_line;

            // parse the first line (METHOD URL Version)
            std::string url;
            std::getline(header, header_line);
            if ((delimiter_index = header_line.find(' ')) != std::string::npos) {
                http_request->set_method(header_line.substr(0, delimiter_index));
                size_t last_delimiter_index = delimiter_index + 1;
                if ((delimiter_index = header_line.find(' ', last_delimiter_index)) != std::string::npos) {
                    url = header_line.substr(last_delimiter_index, delimiter_index - last_delimiter_index);
                    last_delimiter_index = delimiter_index + 1;
                    http_request->set_version(header_line.substr(last_delimiter_index, (header_line.length() - last_delimiter_index) - 1));
                } else {
                    http_request->getRawStream()->is_aborted(true);
                    std::lock_guard<std::mutex> lock(_map_mutex);
                    _pending_requests.erase(http_request);
                    return;
                }
            } else {
                http_request->getRawStream()->is_aborted(true);
                std::lock_guard<std::mutex> lock(_map_mutex);
                _pending_requests.erase(http_request);
                return;
            }

            // only look for a path, it is not a proxy this time
            std::regex pattern("^(/.*?(\\.[^.?]+?)?)(\\?.*)?$", std::regex_constants::icase);
            std::smatch results;
            if (std::regex_search(url, results, pattern)) {
                http_request->set_path(results[1]);
                http_request->set_extension(results[2]);
                http_request->set_query(results[3]);
            } else {
                http_request->getRawStream()->is_aborted(true);
                std::lock_guard<std::mutex> lock(_map_mutex);
                _pending_requests.erase(http_request);
                return;
            }

            // get all HTTP header fields
            std::getline(header, header_line);
            while ((delimiter_index = header_line.find(':')) != std::string::npos) {
                std::string name = header_line.substr(0, delimiter_index);
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                std::string value = header_line.substr(delimiter_index + 1, header_line.size() - delimiter_index - 2);
                value.erase(0, value.find_first_not_of(' '));
                http_request->set_field(name, value);
                std::getline(header, header_line);
            }

            if (http_request->has_minimal_requirements()) {
                http_request->is_parsed(true);
                _http_sink->fromHttpSource(http_request);
            } else {
                http_request->getRawStream()->is_aborted(true);
                std::lock_guard<std::mutex> lock(_map_mutex);
                _pending_requests.erase(http_request);
                return;
            }
        // only redo if message was not complete before HTTP header check
        } else if (!is_complete) {
            timer->expires_from_now(global::DEFAULT_WAIT_REDO);
            timer->async_wait(boost::bind(&NdnHttpInterpreter::get_http_request_header, this, http_request, timer));
        }
    } else {
        std::lock_guard<std::mutex> lock(_map_mutex);
        _pending_requests.erase(http_request);
    }
}

void NdnHttpInterpreter::fromHttpSinkHandler(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response) {
    if (!http_response->getRawStream()->is_aborted()) {
        // prepare NDN server message
        auto ndn_message = std::make_shared<NdnContent>(http_response->getRawStream());

        ndn::Name name("http");
        //tokenize domain
        std::string host = http_request->get_field("host");
        auto host_it = host.find(':');
        std::stringstream domain(host_it != std::string::npos ? host.substr(0, host_it) : host);
        std::string domain_token;
        std::vector<std::string> domain_tokens;
        while (std::getline(domain, domain_token, '.')) {
            domain_tokens.emplace_back(domain_token);
        }
        auto domain_it = domain_tokens.rbegin();
        while (domain_it != domain_tokens.rend()) {
            name.append(*domain_it++);
        }

        //tokenize path
        std::stringstream path(http_request->get_path());
        std::string path_token;
        std::vector<std::string> path_tokens;
        while (std::getline(path, path_token, '/')) {
            if (!path_token.empty() && (path_token != "." && path_token != "..")) {
                path_tokens.emplace_back(path_token);
            }
        }
        auto path_it = path_tokens.begin();
        while (path_it != path_tokens.end()) {
            name.append(uri_encode(*path_it++));
        }

        { // block for RAII
            std::lock_guard<std::mutex> lock(_map_mutex);
            auto request_it = _pending_requests.find(http_request);
            if(request_it != _pending_requests.end()) {
                name.append(request_it->second);
                _pending_requests.erase(request_it);
            }
        }

        ndn_message->setName(name);
        http_response->add_header_to_raw_stream();

        setNdnMessageCachability(ndn_message, http_response);
        //std::cout << ndn_message->get_name() << std::endl;

        // send the message to the ndn module
        _ndn_source->fromNdnSink(ndn_message);
    } else {
        std::lock_guard<std::mutex> lock(_map_mutex);
        _pending_requests.erase(http_request);
    }
}

void NdnHttpInterpreter::setNdnMessageCachability(const std::shared_ptr<NdnContent> &ndn_message, const std::shared_ptr<HttpResponse> &http_response) {
    std::string cache_control = http_response->get_field("cache-control");
    unsigned long delimiter;
    // default freshness value
    ndn::time::milliseconds freshness = ndn::time::milliseconds(0);
    // follow the wish of the HTTP server
    if (http_response->get_field("pragma") == "no-cache" || cache_control.find("no-store") != std::string::npos ||
            cache_control.find("no-cache") != std::string::npos || cache_control.find("private") != std::string::npos) {
        freshness = ndn::time::milliseconds(0);
    } else if ((delimiter = cache_control.find("s-maxage")) != std::string::npos || (delimiter = cache_control.find("max-age")) != std::string::npos) {
        try {
            std::string sub = cache_control.substr(delimiter);
            freshness = ndn::time::milliseconds(1000 * std::stol(sub.substr(sub.find('=') + 1)));
        } catch (const std::exception &e) {}
    } else if (!http_response->get_field("expires").empty()) {
        try {
            freshness = ndn::time::duration_cast<ndn::time::milliseconds>(
                    ndn::time::fromString(http_response->get_field("expires"), "%a, %d %b %Y %H:%M:%S %Z") - ndn::time::system_clock::now());
        } catch (const std::exception &e) {}
    }

    // it is possible to have a negative value
    if (freshness.count() < 0) {
        freshness = ndn::time::milliseconds(0);
    }
    ndn_message->setFreshness(freshness);

    // use the version of the server if it specifies one
    try {
        ndn_message->setTimestamp(ndn::time::fromString(http_response->get_field("last-modified"), "%a, %d %b %Y %H:%M:%S %Z"));
    } catch (const std::exception &e) {
        ndn_message->setTimestamp(ndn::time::system_clock::now());
    }
}

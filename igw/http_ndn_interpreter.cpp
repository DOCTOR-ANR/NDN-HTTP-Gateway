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

#include "http_ndn_interpreter.h"

#include <iostream>

#include "sha1.h"

static const std::set<std::string> STATIC_EXTENSIONS {
        ".css", ".js", ".jpg", ".jpeg", ".gif", ".ico", ".png", ".bmp", ".pict", ".csv", ".doc", ".pdf", ".pls", ".ppt",
        ".tif", ".tiff", ".eps", ".ejs", ".swf", ".midi", ".mid", ".ttf", ".eot", ".woff", ".woff2", ".otf", ".svg",
        ".svgz", ".webp", ".docx", ".xlsx", ".xls", ".pptx", ".ps", ".class", ".jar"
};


static const char SAFE[256] {
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

static std::string uri_encode(const std::string & sSrc){
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

HttpNdnInterpreter::HttpNdnInterpreter(size_t concurrency)
        : Module(concurrency) {

}

void HttpNdnInterpreter::run() {

}

void HttpNdnInterpreter::fromHttpSource(const std::shared_ptr<HttpRequest> &http_request) {
    _ios.post(boost::bind(&HttpNdnInterpreter::fromHttpSourceHandler, this, http_request));
}

void HttpNdnInterpreter::fromNdnSink(const std::shared_ptr<NdnContent> &content) {
    _ios.post(boost::bind(&HttpNdnInterpreter::fromNdnSinkHandler, this, content));
}

void HttpNdnInterpreter::fromHttpSourceHandler(const std::shared_ptr<HttpRequest> &http_request) {
    auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
    computeNames(http_request, timer);
}

void HttpNdnInterpreter::fromNdnSinkHandler(const std::shared_ptr<NdnContent> &content) {
    auto http_response = std::make_shared<HttpResponse>(content->getRawStream());
    auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
    getHttpResponseHeader(content->getName().get(-1).toUri(), http_response, timer);
}

void HttpNdnInterpreter::computeNames(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<boost::asio::deadline_timer> &timer) {
    if(!http_request->getRawStream()->is_aborted()) {
        if (http_request->is_parsed() && (http_request->getRawStream()->is_completed() ||
                http_request->getRawStream()->raw_data_as_string().size() >= 1024)) {
            //std::cout << http_request->make_header() << std::endl;
            //std::exit(0);

            auto delimiter = http_request->get_field("accept-encoding").find(", sdch");
            if (delimiter != std::string::npos) {
                http_request->set_field("accept-encoding", http_request->get_field("accept-encoding").substr(delimiter, 6));
            }

            if(!http_request->get_field("proxy-connection").empty()) {
                http_request->set_field("connection", http_request->get_field("proxy-connection"));
                http_request->unset_field("proxy-connection");
            }

            std::string body = http_request->getRawStream()->raw_data_as_string().substr(0, 1024);
            http_request->add_header_to_raw_stream();
            if (STATIC_EXTENSIONS.find(http_request->get_extension()) != STATIC_EXTENSIONS.end()) {
                http_request->unset_field("user-agent");
                http_request->unset_field("accept");
                http_request->unset_field("accept-language");
                http_request->unset_field("cookie");
            }

            std::string sha1 = SHA1{}(http_request->make_header() + body);


            { // block for RAII
                std::lock_guard<std::mutex> lock(_pending_requests_mutex);
                auto it = _pending_requests.find(sha1);
                if (it == _pending_requests.end()) {
                    _pending_requests.emplace(sha1, std::unordered_set<std::shared_ptr<HttpRequest>>{http_request});
                } else {
                    it->second.insert(http_request);
                    //std::cout << http_request->get_field("host") << http_request->get_path()
                    //          << " (" << sha1 << ") count = " << it->second.size() << std::endl;
                    return;
                }
            }

            ndn::Name name("http");
            // tokenize domain
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

            // tokenize path
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

            name.append(sha1);

            auto ndn_content = std::make_shared<NdnContent>(http_request->getRawStream());
            ndn_content->setName(name);
            _ndn_sink->fromNdnSource(ndn_content);
        } else {
            timer->expires_from_now(global::DEFAULT_WAIT_REDO);
            timer->async_wait(boost::bind(&HttpNdnInterpreter::computeNames, this, http_request, timer));
        }
    }
}

void HttpNdnInterpreter::getHttpResponseHeader(const std::string &sha1, const std::shared_ptr<HttpResponse> &http_response,
                                               const std::shared_ptr<boost::asio::deadline_timer> &timer) {
    if(!http_response->getRawStream()->is_aborted()) {
        // keep track of completion before doing the job because if set to true and no HTTP header found then discard
        bool is_complete = http_response->getRawStream()->is_completed();

        std::string raw_data = http_response->getRawStream()->raw_data_as_string();
        size_t delimiter_index = raw_data.find("\r\n\r\n");
        if (delimiter_index != std::string::npos) {
            http_response->getRawStream()->removeFirstBytes(delimiter_index + 4);
            std::stringstream header(raw_data);
            std::string header_line;

            std::getline(header, header_line);
            if ((delimiter_index = header_line.find(' ')) != std::string::npos) {
                http_response->set_version(header_line.substr(0, delimiter_index));
                size_t last_delimiter_index = delimiter_index + 1;
                if ((delimiter_index = header_line.find(' ', last_delimiter_index)) != std::string::npos) {
                    http_response->set_status_code(header_line.substr(last_delimiter_index, delimiter_index - last_delimiter_index));
                    last_delimiter_index = delimiter_index + 1;
                    http_response->set_reason(header_line.substr(last_delimiter_index, (header_line.length() - last_delimiter_index) - 1));
                }
            }

            std::getline(header, header_line);
            while ((delimiter_index = header_line.find(':')) != std::string::npos) {
                std::string name = header_line.substr(0, delimiter_index);
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                name.erase(0, name.find_first_not_of(' '));
                name.erase(name.find_last_not_of(' ') + 1);
                std::string value = header_line.substr(delimiter_index + 1, header_line.size() - delimiter_index - 2);
                value.erase(0, value.find_first_not_of(' '));
                value.erase(value.find_last_not_of(' ') + 1);
                http_response->set_field(name, value);
                std::getline(header, header_line);
            }

            if (http_response->has_minimal_requirements()) {
                http_response->is_parsed(true);
            } else {
                http_response->getRawStream()->is_aborted(true);
            }

            std::unordered_set<std::shared_ptr<HttpRequest>> set;
            { // block for RAII
                std::lock_guard<std::mutex> lock(_pending_requests_mutex);
                set = std::move(_pending_requests.at(sha1));
                _pending_requests.erase(sha1);
            }
            for (const auto& req : set) {
                _http_source->fromHttpSink(req, http_response);
            }
        } else if (!is_complete) {
            timer->expires_from_now(global::DEFAULT_WAIT_REDO);
            timer->async_wait(boost::bind(&HttpNdnInterpreter::getHttpResponseHeader, this, sha1, http_response, timer));
        } else {
            std::unordered_set<std::shared_ptr<HttpRequest>> set;
            { // block for RAII
                std::lock_guard<std::mutex> lock(_pending_requests_mutex);
                set = std::move(_pending_requests.at(sha1));
                _pending_requests.erase(sha1);
            }
            for (const auto& req : set) {
                _http_source->fromHttpSink(req, http_response);
            }
        }
    } else {
        std::unordered_set<std::shared_ptr<HttpRequest>> set;
        { // block for RAII
            std::lock_guard<std::mutex> lock(_pending_requests_mutex);
            set = std::move(_pending_requests.at(sha1));
            _pending_requests.erase(sha1);
        }
        for (const auto& req : set) {
            _http_source->fromHttpSink(req, http_response);
        }
    }
}

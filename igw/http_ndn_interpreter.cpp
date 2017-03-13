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

#include "http_ndn_interpreter.h"

#include <iostream>

#include "sha1.h"

const char HttpNdnInterpreter::SAFE[256] = {
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

const std::set<std::string> HttpNdnInterpreter::STATIC_EXTENSIONS = {
        ".css", ".js", ".jpg", ".jpeg", ".gif", ".ico", ".png", ".bmp", ".pict", ".csv", ".doc", ".pdf", ".pls", ".ppt",
        ".tif", ".tiff", ".eps", ".ejs", ".swf", ".midi", ".mid", ".ttf", ".eot", ".woff", ".woff2", ".otf", ".svg", ".svgz"
        ".webp", ".docx", ".xlsx", ".xls", ".pptx", ".ps", ".class", ".jar"
};

HttpNdnInterpreter::HttpNdnInterpreter(ndn::Name prefix, size_t concurrency)
        : Module(concurrency)
        , _prefix(prefix) {

}

void HttpNdnInterpreter::run() {

}

void HttpNdnInterpreter::solve(std::shared_ptr<HttpRequest> http_request, std::shared_ptr<HttpResponse> http_response) {
    _ios.post(boost::bind(&HttpNdnInterpreter::resolve_handler, this, http_request, http_response));
}

void HttpNdnInterpreter::resolve_handler(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response) {
    auto ndn_client_message = std::make_shared<NdnMessage>(http_response->get_raw_stream());
    auto ndn_server_message = std::make_shared<NdnMessage>(http_request->get_raw_stream());

    auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
    _ios.post(boost::bind(&HttpNdnInterpreter::compute_names, this, http_request, ndn_client_message, ndn_server_message, timer));
}

std::string HttpNdnInterpreter::uri_encode(const std::string & sSrc){
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

void HttpNdnInterpreter::compute_names(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<NdnMessage> &ndn_client_message,
                                  const std::shared_ptr<NdnMessage> &ndn_server_message, const std::shared_ptr<boost::asio::deadline_timer> &timer) {
    if(http_request->is_parsed()) {
        /*auto delimiter = http_request->get_field("accept-encoding").find("sdch");
        if(delimiter != std::string::npos){
            http_request->set_field("accept-encoding", http_request->get_field("accept-encoding").substr(delimiter, 4));
        }*/
        http_request->add_header_to_raw_stream();
        if (STATIC_EXTENSIONS.find(http_request->get_extension()) != STATIC_EXTENSIONS.end()) {
            http_request->unset_field("user-agent");
            http_request->unset_field("accept");
            http_request->unset_field("accept-language");
        }

        SHA1 sha1;
        ndn_server_message->set_name(ndn::Name(_prefix).append(sha1(http_request->make_header())));
        _ndn_server->solve(ndn_server_message);

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
            if (!path_token.empty()) {
                path_tokens.emplace_back(path_token);
            }
        }
        auto path_it = path_tokens.begin();
        while (path_it != path_tokens.end()) {
            name.append(uri_encode(*path_it++));
        }

        //add prefix of igw
        ndn_client_message->set_name(name.appendVersion(0).append(_prefix).append(sha1(http_request->make_header())));
        _ndn_client->solve(ndn_client_message);
    } else {
        timer->expires_from_now(global::DEFAULT_WAIT_REDO);
        _ios.post(boost::bind(&HttpNdnInterpreter::compute_names, this, http_request, ndn_client_message, ndn_server_message, timer));
    }
}

void HttpNdnInterpreter::attach_ndn_sinks(NdnSink *ndn_client, NdnSink *ndn_server) {
    _ndn_client = ndn_client;
    _ndn_server = ndn_server;
}

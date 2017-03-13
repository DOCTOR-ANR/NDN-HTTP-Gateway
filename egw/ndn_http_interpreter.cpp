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

#include "ndn_http_interpreter.h"

#include <regex>
#include <algorithm>

NdnHttpInterpreter::NdnHttpInterpreter(size_t concurrency) : Module(concurrency) {

}

void NdnHttpInterpreter::run() {

}

void NdnHttpInterpreter::solve(std::shared_ptr<NdnMessage> ndn_message) {
    _ios.post(boost::bind(&NdnHttpInterpreter::solve_handler, this, ndn_message));
}

void NdnHttpInterpreter::solve_handler(const std::shared_ptr<NdnMessage> &ndn_message) {
    auto http_request = std::make_shared<HttpRequest>();
    auto http_response = std::make_shared<HttpResponse>();
    auto ndn_client_message = std::make_shared<NdnMessage>(http_request->get_raw_stream());
    auto ndn_server_message = std::make_shared<NdnMessage>(http_response->get_raw_stream());

    int n=0;
    while(!ndn_message->get_name().get(++n).isVersion());
    ndn_client_message->set_name(ndn_message->get_name().getSubName(n+1));
    ndn::Name name(ndn_message->get_name().getPrefix(n));
    ndn_server_message->set_name(name.append(ndn_message->get_name().get(-1)));
    _ndn_client->solve(ndn_client_message);

    auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
    _ios.post(boost::bind(&NdnHttpInterpreter::get_http_request_header, this, http_request, http_response, ndn_server_message, timer));
}

void NdnHttpInterpreter::get_http_request_header(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response,
                                                 const std::shared_ptr<NdnMessage> &ndn_message, const std::shared_ptr<boost::asio::deadline_timer> &timer){
    if(!http_request->get_raw_stream()->is_aborted()) {
        std::string raw_data = http_request->get_raw_stream()->raw_data_as_string();
        size_t delimiter_index = raw_data.find("\r\n\r\n");
        if (delimiter_index != std::string::npos) {
            //http_request->get_raw_stream()->remove_first_bytes(delimiter_index + 4);
            std::stringstream header(raw_data);
            std::string header_line;

            std::string url;
            std::getline(header, header_line);
            if ((delimiter_index = header_line.find(' ')) != std::string::npos) {
                http_request->set_method(header_line.substr(0, delimiter_index));
                size_t last_delimiter_index = delimiter_index + 1;
                if ((delimiter_index = header_line.find(' ', last_delimiter_index)) != std::string::npos) {
                    url = header_line.substr(last_delimiter_index, delimiter_index - last_delimiter_index);
                    last_delimiter_index = delimiter_index + 1;
                    if ((delimiter_index = header_line.find('\r', last_delimiter_index)) != std::string::npos) {
                        http_request->set_version(header_line.substr(last_delimiter_index, delimiter_index - last_delimiter_index));
                    } else {
                        http_request->get_raw_stream()->is_aborted(true);
                        return;
                    }
                } else {
                    http_request->get_raw_stream()->is_aborted(true);
                    return;
                }
            } else {
                http_request->get_raw_stream()->is_aborted(true);
                return;
            }

            std::regex pattern("^(?:https?://)?(?:[^/]+?(?::\\d+)?)?(/.*?(\\..+?)?)(\\?.*)?$",
                               std::regex_constants::icase);
            std::smatch results;
            if (std::regex_search(url, results, pattern)) {
                http_request->set_path(results[1]);
                http_request->set_extension(results[2]);
                http_request->set_query(results[3]);
            } else {
                return;
            }

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
                _http_client->solve(http_request, http_response);
                _ios.post(boost::bind(&NdnHttpInterpreter::set_ndn_message_cachability, this, http_response, ndn_message, timer));
            } else {
                http_request->get_raw_stream()->is_aborted(true);
            }
        } else {
            timer->expires_from_now(global::DEFAULT_WAIT_REDO);
            timer->async_wait(boost::bind(&NdnHttpInterpreter::get_http_request_header, this, http_request, http_response, ndn_message, timer));
        }
    }
}

void NdnHttpInterpreter::set_ndn_message_cachability(const std::shared_ptr<HttpResponse> &http_response, const std::shared_ptr<NdnMessage> &ndn_message,
                                                     const std::shared_ptr<boost::asio::deadline_timer> &timer) {
    if (!http_response->get_raw_stream()->is_aborted()) {
        if (http_response->is_parsed()) {
            http_response->add_header_to_raw_stream();
            std::string cache_control = http_response->get_field("cache-control");
            unsigned long delimiter;
            ndn::time::milliseconds freshness = ndn::time::milliseconds(3600000);
            if (http_response->get_field("pragma") == "no-cache" || cache_control.find("no-store") != std::string::npos ||
                    cache_control.find("no-cache") != std::string::npos || cache_control.find("private") != std::string::npos) {
                freshness = ndn::time::milliseconds(0);
            } else if((delimiter = cache_control.find("s-maxage")) != std::string::npos || (delimiter = cache_control.find("max-age")) != std::string::npos){
                try {
                    std::string sub = cache_control.substr(delimiter);
                    freshness = ndn::time::milliseconds(std::stol(sub.substr(sub.find('=') + 1)));
                } catch(const std::exception &e){}
            } else if(!http_response->get_field("expires").empty()) {
                try {
                    freshness = ndn::time::duration_cast<ndn::time::milliseconds>(
                            ndn::time::fromString(http_response->get_field("expires"), "%a, %d %b %Y %H:%M:%S %Z") - ndn::time::system_clock::now());
                } catch(const std::exception &e){}

            }
            ndn_message->set_freshness(freshness);
            _ndn_server->solve(ndn_message);
        } else {
            timer->expires_from_now(global::DEFAULT_WAIT_REDO);
            timer->async_wait(boost::bind(&NdnHttpInterpreter::set_ndn_message_cachability, this, http_response, ndn_message, timer));
        }
    }
}

void NdnHttpInterpreter::attach_ndn_sinks(NdnSink *ndn_client, NdnSink *ndn_server) {
    _ndn_client = ndn_client;
    _ndn_server = ndn_server;
}

void NdnHttpInterpreter::attach_http_sink(HttpSink *http_sink) {
    _http_client = http_sink;
}
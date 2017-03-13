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

#include <ndn-cxx/name.hpp>

#include <boost/thread.hpp>

#include <memory>
#include <set>

#include "global.h"
#include "module.h"
#include "http_sink.h"
#include "ndn_sink.h"
#include "http_request.h"
#include "http_response.h"

class HttpNdnInterpreter : public Module, public HttpSink {
private:
    static const char SAFE[256];
    static const std::set<std::string> STATIC_EXTENSIONS;

    ndn::Name _prefix;

    NdnSink *_ndn_client;
    NdnSink *_ndn_server;

public:
    HttpNdnInterpreter(ndn::Name prefix, size_t concurrency = 1);

    virtual void run() override;

    virtual void solve(std::shared_ptr<HttpRequest> http_request, std::shared_ptr<HttpResponse> http_response) override;

    void resolve_handler(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response);

    std::string uri_encode(const std::string &sSrc);

    void compute_names(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<NdnMessage> &ndn_client_message,
                       const std::shared_ptr<NdnMessage> &ndn_server_message, const std::shared_ptr<boost::asio::deadline_timer> &timer);

    void attach_ndn_sinks(NdnSink *ndn_client, NdnSink *ndn_server);
};

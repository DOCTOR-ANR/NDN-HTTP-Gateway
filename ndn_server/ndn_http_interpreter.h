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

#pragma once

#include <ndn-cxx/name.hpp>

#include <memory>
#include <mutex>
#include <unordered_map>

#include "global.h"
#include "module.h"
#include "ndn_sink.h"
#include "http_source.h"
#include "ndn_content.h"
#include "http_request.h"
#include "http_response.h"

class NdnHttpInterpreter : public Module, public NdnSink, public HttpSource {
private:
    std::mutex _map_mutex;
    std::unordered_map<std::shared_ptr<HttpRequest>, std::string> _pending_requests;

public:
    explicit NdnHttpInterpreter(size_t concurrency = 1);

    ~NdnHttpInterpreter() override = default;

    void run() override;

    void fromNdnSource(const std::shared_ptr<NdnContent> &ndn_content) override;

    void fromHttpSink(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response) override;

private:
    void fromNdnSourceHandler(const std::shared_ptr<NdnContent> &ndn_content);

    void get_http_request_header(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<boost::asio::deadline_timer> &timer);

    void fromHttpSinkHandler(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response);

    void setNdnMessageCachability(const std::shared_ptr<NdnContent> &ndn_message, const std::shared_ptr<HttpResponse> &http_response);
};

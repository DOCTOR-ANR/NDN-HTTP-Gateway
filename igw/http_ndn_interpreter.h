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

#include <boost/thread.hpp>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

#include "global.h"
#include "module.h"
#include "http_sink.h"
#include "ndn_source.h"
#include "http_request.h"
#include "http_response.h"
#include "ndn_content.h"

class HttpNdnInterpreter : public Module, public HttpSink, public NdnSource {
private:
    // mandatory, ndn-cxx lib throws exception when it sends burst of interest with same name
    std::mutex _pending_requests_mutex;
    std::map<std::string, std::unordered_set<std::shared_ptr<HttpRequest>>> _pending_requests;

public:
    explicit HttpNdnInterpreter(size_t concurrency = 1);

    ~HttpNdnInterpreter() override = default;

    void run() override;

    void fromHttpSource(const std::shared_ptr<HttpRequest> &http_request) override;

    void fromNdnSink(const std::shared_ptr<NdnContent> &content) override;

private:
    void fromHttpSourceHandler(const std::shared_ptr<HttpRequest> &http_request);

    void fromNdnSinkHandler(const std::shared_ptr<NdnContent> &content);

    void computeNames(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<boost::asio::deadline_timer> &timer);

    void getHttpResponseHeader(const std::string &sha1, const std::shared_ptr<HttpResponse> &http_response,
                               const std::shared_ptr<boost::asio::deadline_timer> &timer);
};

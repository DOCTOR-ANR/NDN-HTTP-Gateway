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

#include <memory>

#include "global.h"
#include "module.h"
#include "http_sink.h"
#include "http_request.h"
#include "http_response.h"

class HttpEngine : public Module, public HttpSink {
private:
    std::map<std::string, std::string> _mime_types;

public:
    HttpEngine(size_t concurrency = 1);

    ~HttpEngine() = default;

private:
    void run() override;

    void fromHttpSource(const std::shared_ptr<HttpRequest> &http_request) override;

    void resolve(const std::shared_ptr<HttpRequest> &http_request);

    std::shared_ptr<HttpResponse> get(const std::shared_ptr<HttpRequest> &http_request, const std::shared_ptr<HttpResponse> &http_response, bool skip_body = false);
};
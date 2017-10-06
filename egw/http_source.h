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

#include <memory>

#include "http_sink.h"
#include "http_request.h"
#include "http_response.h"

class HttpSink;

class HttpSource {
protected:
    HttpSink *_http_sink;

public:
    virtual void takeBack(std::shared_ptr<HttpRequest> http_request, std::shared_ptr<HttpResponse> http_response) = 0;

    void attachHttpSink(HttpSink *http_sink) {
        _http_sink = http_sink;
    }
};
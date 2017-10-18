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
#include <atomic>
#include <mutex>
#include <string>
#include <map>

#include "message.h"
#include "seekable_raw_stream.h"

class HttpResponse : public Message {
private:
    std::atomic<bool> _parsed {false};

    std::mutex _mutex;

    std::string _version;
    std::string _status_code;
    std::string _reason;
    std::map<std::string, std::string> _fields;

public:
    HttpResponse() = default;

    explicit HttpResponse(std::shared_ptr<SeekableRawStream> raw_stream);

    ~HttpResponse() override = default;

    bool is_parsed();

    void is_parsed(bool parsed);

    std::string get_version();

    void set_version(const std::string &version);

    std::string get_status_code();

    void set_status_code(const std::string &status_code);

    std::string get_reason();

    void set_reason(const std::string &reason);

    std::map<std::string, std::string> get_fields();

    std::string get_field(const std::string &field);

    void set_field(const std::string &field, const std::string &value);

    void unset_field(const std::string &field);

    bool has_minimal_requirements();

    std::string make_header();

    void add_header_to_raw_stream();
};


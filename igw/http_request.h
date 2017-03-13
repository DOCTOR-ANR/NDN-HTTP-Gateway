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
#include "raw_stream.h"

class HttpRequest : public Message {
private:
    std::atomic<bool> _parsed;

    std::mutex _mutex;

    std::string _method;
    std::string _path;
    std::string _version;
    std::string _extension;
    std::string _query;
    std::map<std::string, std::string> _fields;

public:
    HttpRequest();

    HttpRequest(std::shared_ptr<RawStream> raw_stream);

    ~HttpRequest();

    bool is_parsed();

    void is_parsed(bool parsed);

    std::string get_method();

    void set_method(std::string method);

    std::string get_version();

    void set_version(std::string version);

    std::string get_path();

    void set_path(std::string path);

    std::string get_extension();

    void set_extension(std::string extension);

    std::string get_query();

    void set_query(std::string query);

    std::map<std::string, std::string> get_fields();

    std::string get_field(std::string field);

    void set_field(std::string field, std::string value);

    void unset_field(std::string field);

    bool has_minimal_requirements();

    std::string make_header();

    void add_header_to_raw_stream();
};

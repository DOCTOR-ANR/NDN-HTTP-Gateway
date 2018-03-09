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

#include "http_response.h"

#include <iostream>
#include <sstream>

HttpResponse::HttpResponse(std::shared_ptr<SeekableRawStream> raw_stream) : Message(raw_stream), _parsed(false) {

}

bool HttpResponse::is_parsed() {
    return _parsed;
}

void HttpResponse::is_parsed(bool parsed) {
    _parsed = parsed;
}

std::string HttpResponse::get_version() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _version;
}

void HttpResponse::set_version(const std::string &version) {
    _version = version;
}

std::string HttpResponse::get_status_code() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _status_code;
}

void HttpResponse::set_status_code(const std::string &status_code) {
    std::lock_guard<std::mutex> lock(_mutex);
    _status_code = status_code;
}

std::string HttpResponse::get_reason() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _reason;
}

void HttpResponse::set_reason(const std::string &reason) {
    std::lock_guard<std::mutex> lock(_mutex);
    _reason = reason;
}

std::map<std::string, std::string> HttpResponse::get_fields() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _fields;
};

std::string HttpResponse::get_field(const std::string &field) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _fields.find(field);
    return it != _fields.end() ? it->second : "";
}

void HttpResponse::set_field(const std::string &field, const std::string &value) {
    std::lock_guard<std::mutex> lock(_mutex);
    _fields[field] = value;
}

void HttpResponse::unset_field(const std::string &field) {
    std::lock_guard<std::mutex> lock(_mutex);
    _fields.erase(field);
}

bool HttpResponse::has_minimal_requirements() {
    std::lock_guard<std::mutex> lock(_mutex);
    return !(_version.empty() | _status_code.empty());
}

std::string HttpResponse::make_header() {
    std::lock_guard<std::mutex> lock(_mutex);
    std::stringstream ss;
    ss << _version << " " << _status_code << " " << _reason << "\r\n";
    for(auto field : _fields){
        ss << field.first << ": " << field.second << "\r\n";
    }
    ss << "\r\n";
    return ss.str();
}

void HttpResponse::add_header_to_raw_stream() {
    _raw_stream->append_raw_data_at_first(make_header());
}
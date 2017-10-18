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

#include "http_request.h"

#include <sstream>

HttpRequest::HttpRequest(const std::shared_ptr<SeekableRawStream> &raw_stream) : Message(raw_stream), _parsed(false) {

}

bool HttpRequest::is_parsed() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _parsed;
}

void HttpRequest::is_parsed(bool parsed) {
    std::lock_guard<std::mutex> lock(_mutex);
    _parsed = parsed;
}

std::string HttpRequest::get_method() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _method;
}

void HttpRequest::set_method(const std::string &method){
    std::lock_guard<std::mutex> lock(_mutex);
    _method = method;
}

std::string HttpRequest::get_version() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _version;
}

void HttpRequest::set_version(const std::string &version){
    std::lock_guard<std::mutex> lock(_mutex);
    _version = version;
}

std::string HttpRequest::get_path() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _path;
}

void HttpRequest::set_path(const std::string &path){
    std::lock_guard<std::mutex> lock(_mutex);
    _path = path;
}

std::string HttpRequest::get_extension() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _extension;
}

void HttpRequest::set_extension(const std::string &extension) {
    std::lock_guard<std::mutex> lock(_mutex);
    _extension = extension;
}

std::string HttpRequest::get_query() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _query;
}

void HttpRequest::set_query(const std::string &query){
    std::lock_guard<std::mutex> lock(_mutex);
    _query = query;
}

std::map<std::string, std::string> HttpRequest::get_fields() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _fields;
};

std::string HttpRequest::get_field(const std::string &field) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _fields.find(field);
    return it != _fields.end() ? it->second : std::string();
}

void HttpRequest::set_field(const std::string &field, const std::string &value) {
    std::lock_guard<std::mutex> lock(_mutex);
    _fields[field] = value;
}

void HttpRequest::unset_field(const std::string &field) {
    std::lock_guard<std::mutex> lock(_mutex);
    _fields.erase(field);
}

bool HttpRequest::has_minimal_requirements() {
    std::lock_guard<std::mutex> lock(_mutex);
    return !(_method.empty() | _path.empty() | _version.empty() | _fields.find("host") == _fields.end());
}

std::string HttpRequest::make_header() {
    std::lock_guard<std::mutex> lock(_mutex);
    std::stringstream ss;
    ss << _method << " " << _path << _query << " " << _version << "\r\n";
    for(auto field : _fields){
        ss << field.first << ": " << field.second << "\r\n";
    }
    ss << "\r\n";
    return ss.str();
}

void HttpRequest::add_header_to_raw_stream() {
    _raw_stream->append_raw_data_at_first(make_header());
}



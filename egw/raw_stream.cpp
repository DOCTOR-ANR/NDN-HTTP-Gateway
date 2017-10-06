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

#include "raw_stream.h"

#include <iostream>

RawStream::RawStream() : _completed(false), _aborted(false), _total_read_bytes(0) {

}

RawStream::~RawStream() {

}

bool RawStream::is_completed() {
    return _completed;
}


void RawStream::is_completed(bool completed) {
    _completed = completed;
}

bool RawStream::is_aborted() {
    return _aborted;
}

void RawStream::is_aborted(bool aborted) {
    _aborted = aborted;
}

uint64_t RawStream::get_total_read_bytes() {
    return _total_read_bytes;
}

void RawStream::append_raw_data_at_first(std::string data) {
    std::lock_guard<std::mutex> lock(_mutex);
    std::streamsize pos = _raw_data.tellp();
    _raw_data.str(data + _raw_data.str());
    _raw_data.seekp(pos + data.size());
}

void RawStream::append_raw_data(const std::istream &stream) {
    std::lock_guard<std::mutex> lock(_mutex);
    _raw_data << stream.rdbuf();
};

void RawStream::append_raw_data(std::streambuf *buffer) {
    std::lock_guard<std::mutex> lock(_mutex);
    _raw_data << buffer;
};

void RawStream::append_raw_data(const char *buffer, size_t size) {
    std::lock_guard<std::mutex> lock(_mutex);
    _raw_data.write(buffer, size);
};

void RawStream::append_raw_data(const std::string &data) {
    std::lock_guard<std::mutex> lock(_mutex);
    _raw_data << data;
};

bool RawStream::has_remaining_bytes(){
    std::lock_guard<std::mutex> lock(_mutex);
    return (_completed || _aborted) ? _raw_data.good() : true;
}

long RawStream::read_raw_data(char *buffer, size_t size) {
    std::lock_guard<std::mutex> lock(_mutex);
    if(!_completed && !_aborted){
        if (_raw_data.tellp() - _raw_data.tellg() >= size) {
            auto read_bytes = _raw_data.read(buffer, size).gcount();
            _total_read_bytes += read_bytes;
            return read_bytes;
        } else {
            return -1;
        }
    }else {
        auto read_bytes = _raw_data.read(buffer, size).gcount();
        _total_read_bytes += read_bytes;
        return read_bytes;
    }
}

void RawStream::remove_first_bytes(size_t size) {
    std::lock_guard<std::mutex> lock(_mutex);
    size = std::min(size, (size_t)(_raw_data.tellp() - _raw_data.tellg()));
    _raw_data.seekg(size, std::stringstream::cur);
    _total_read_bytes += size;
}

std::string RawStream::raw_data_as_string() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _raw_data.str();
}
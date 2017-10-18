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

#include "seekable_raw_stream.h"

#include <iostream>

bool SeekableRawStream::is_completed() {
    return _completed;
}

void SeekableRawStream::is_completed(bool completed) {
    _completed = completed;
}

bool SeekableRawStream::is_aborted() {
    return _aborted;
}

void SeekableRawStream::is_aborted(bool aborted) {
    _aborted = aborted;
}

void SeekableRawStream::append_raw_data_at_first(const std::string &data) {
    std::lock_guard<std::mutex> lock(_mutex);
    _raw_data.insert(_raw_data.begin(), data.begin(), data.end());
}

void SeekableRawStream::append_raw_data(const std::istream &stream) {
    std::lock_guard<std::mutex> lock(_mutex);
    _raw_data.insert(_raw_data.end(), std::istreambuf_iterator<char>(stream.rdbuf()), {});
};

void SeekableRawStream::append_raw_data(std::streambuf *buffer) {
    std::lock_guard<std::mutex> lock(_mutex);
    _raw_data.insert(_raw_data.end(), std::istreambuf_iterator<char>(buffer), {});
};

void SeekableRawStream::append_raw_data(const char *buffer, size_t size) {
    std::lock_guard<std::mutex> lock(_mutex);
    _raw_data.insert(_raw_data.end(), buffer, buffer + size);
};

void SeekableRawStream::append_raw_data(const std::string &data) {
    std::lock_guard<std::mutex> lock(_mutex);
    _raw_data.insert(_raw_data.end(), data.begin(), data.end());
};

long SeekableRawStream::readRawData(size_t pos, char *buffer, size_t size) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (pos > _raw_data.size()) {
        return 0;
    }

    size_t remaining_bytes = _raw_data.size() - pos;
    if (size > remaining_bytes && !(_completed || _aborted)) {
        return -1;
    } else {
        size_t min = std::min(size, remaining_bytes);
        const auto raw_data_pos = _raw_data.begin() + pos;
        std::copy(raw_data_pos, raw_data_pos + min, buffer);
        return min;
    }
}

void SeekableRawStream::removeFirstBytes(size_t size) {
    std::lock_guard<std::mutex> lock(_mutex);
    _raw_data.erase(_raw_data.begin(), _raw_data.begin() + size);
}

long SeekableRawStream::remainingBytes(size_t pos) {
    return !(_completed || _aborted) ? -1 : std::max((long)(_raw_data.size() - pos), 0L);
}

std::string SeekableRawStream::raw_data_as_string() {
    std::lock_guard<std::mutex> lock(_mutex);
    return std::string(_raw_data.begin(), _raw_data.end());
}
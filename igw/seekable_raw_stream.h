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

#include <atomic>
#include <deque>
#include <mutex>

class SeekableRawStream {
protected:
    std::atomic<bool> _completed {false};
    std::atomic<bool> _aborted {false};

    std::mutex _mutex;
    std::deque<char> _raw_data;

public:
    SeekableRawStream() = default;

    ~SeekableRawStream() = default;

    bool is_completed();

    void is_completed(bool state);

    bool is_aborted();

    void is_aborted(bool state);

    void append_raw_data_at_first(const std::string &data);

    void append_raw_data(const std::istream &stream);

    void append_raw_data(std::streambuf *buffer);

    void append_raw_data(const char *buffer, size_t size);

    void append_raw_data(const std::string &data);

    long readRawData(size_t pos, char *buffer, size_t size);

    void removeFirstBytes(size_t size);

    long remainingBytes(size_t pos);

    std::string raw_data_as_string();
};

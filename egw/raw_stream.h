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
#include <sstream>
#include <mutex>

class RawStream {
protected:
    std::atomic<bool> _completed;
    std::atomic<bool> _aborted;

    std::stringstream _raw_data;
    std::atomic<uint64_t> _total_read_bytes;

    std::mutex _mutex;

public:
    RawStream();

    ~RawStream();

    bool is_completed();

    void is_completed(bool state);

    bool is_aborted();

    void is_aborted(bool state);

    uint64_t get_total_read_bytes();

    void append_raw_data_at_first(std::string data);

    void append_raw_data(const std::istream &stream);

    void append_raw_data(std::streambuf *buffer);

    void append_raw_data(const char *buffer, size_t size);

    void append_raw_data(const std::string &data);

    bool has_remaining_bytes();

    long read_raw_data(char *buffer, size_t size);

    void remove_first_bytes(size_t size);

    std::string raw_data_as_string();
};

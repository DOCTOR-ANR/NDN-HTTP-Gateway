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

#include <ndn-cxx/data.hpp>

#include <memory>

#include "message.h"
#include "raw_stream.h"

class NdnContent : public Message {
private:
    ndn::Name _name;
    ndn::time::milliseconds _freshness;
    ndn::time::system_clock::time_point _timestamp;

    std::chrono::steady_clock::time_point _last_access;
    std::map<uint64_t, std::shared_ptr<ndn::Data>> _datas;

public:
    NdnContent();

    NdnContent(const std::shared_ptr<RawStream> &raw_stream);

    const ndn::Name& get_name() const;

    void set_name(const ndn::Name &name);

    const ndn::time::milliseconds& get_freshness() const;

    void set_freshness(const ndn::time::milliseconds& freshness);

    const ndn::time::system_clock::time_point& get_timestamp() const;

    void set_timestamp(const ndn::time::system_clock::time_point& freshness);

    const std::chrono::steady_clock::time_point& get_last_access() const;

    void refresh();

    void add_data(uint64_t segment, std::shared_ptr<ndn::Data> data);

    std::map<uint64_t, std::shared_ptr<ndn::Data>>::iterator find_data(uint64_t segment);

    std::map<uint64_t, std::shared_ptr<ndn::Data>>::iterator end();
};
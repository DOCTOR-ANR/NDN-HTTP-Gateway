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

#pragma once

#include <ndn-cxx/data.hpp>

#include <memory>

#include "message.h"
#include "seekable_raw_stream.h"

class NdnContent : public Message {
private:
    ndn::Name _name;
    ndn::time::milliseconds _freshness{0};
    ndn::time::system_clock::time_point _timestamp;

    std::chrono::steady_clock::time_point _last_access;
    std::map<uint64_t, std::shared_ptr<ndn::Data>> _datas;

public:
    NdnContent() = default;

    explicit NdnContent(const std::shared_ptr<SeekableRawStream> &raw_stream);

    ~NdnContent() override = default;

    const ndn::Name& getName() const;

    void setName(const ndn::Name &name);

    const ndn::time::milliseconds& getFreshness() const;

    void setFreshness(const ndn::time::milliseconds &freshness);

    const ndn::time::system_clock::time_point& getTimestamp() const;

    void setTimestamp(const ndn::time::system_clock::time_point &freshness);

    const std::chrono::steady_clock::time_point& getLastAccess() const;

    void refresh();

    bool addData(uint64_t segment, std::shared_ptr<ndn::Data> data);

    std::map<uint64_t, std::shared_ptr<ndn::Data>>::iterator findData(uint64_t segment);

    std::map<uint64_t, std::shared_ptr<ndn::Data>>::iterator end();

    size_t SegmentCount();
};
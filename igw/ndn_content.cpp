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

#include "ndn_content.h"

NdnContent::NdnContent(const std::shared_ptr<SeekableRawStream> &raw_stream) : Message(raw_stream), _last_access(std::chrono::steady_clock::now()){

}

const ndn::Name& NdnContent::getName() const {
    return _name;
}

void NdnContent::setName(const ndn::Name &name) {
    _name = name;
}

const ndn::time::milliseconds& NdnContent::getFreshness() const {
    return _freshness;
}

void NdnContent::setFreshness(const ndn::time::milliseconds& freshness) {
    _freshness = freshness;
}

const ndn::time::system_clock::time_point& NdnContent::getTimestamp() const {
    return _timestamp;
}

void NdnContent::setTimestamp(const ndn::time::system_clock::time_point& timestamp) {
    _timestamp = timestamp;
}

const std::chrono::steady_clock::time_point& NdnContent::getLastAccess() const {
    return _last_access;
}

void NdnContent::refresh() {
    _last_access = std::chrono::steady_clock::now();
}

bool NdnContent::addData(uint64_t segment, std::shared_ptr<ndn::Data> data) {
    refresh();
    return _datas.emplace(segment, data).second;
}

std::map<uint64_t, std::shared_ptr<ndn::Data>>::iterator NdnContent::findData(uint64_t segment) {
    refresh();
    return _datas.find(segment);
}

std::map<uint64_t, std::shared_ptr<ndn::Data>>::iterator NdnContent::end() {
    return _datas.end();
}

size_t NdnContent::currentSegmentCount() {
    return _datas.size();
}
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

NdnContent::NdnContent() {

}

NdnContent::NdnContent(const std::shared_ptr<RawStream> &raw_stream) : Message(raw_stream), _last_access(std::chrono::steady_clock::now()){

}

const ndn::Name& NdnContent::get_name() const {
    return _name;
}

void NdnContent::set_name(const ndn::Name &name) {
    _name = name;
}

const ndn::time::milliseconds& NdnContent::get_freshness() const {
    return _freshness;
}

void NdnContent::set_freshness(const ndn::time::milliseconds& freshness) {
    _freshness = freshness;
}

const ndn::time::system_clock::time_point& NdnContent::get_timestamp() const {
    return _timestamp;
}

void NdnContent::set_timestamp(const ndn::time::system_clock::time_point& timestamp) {
    _timestamp = timestamp;
}

const std::chrono::steady_clock::time_point& NdnContent::get_last_access() const {
    return _last_access;
}

void NdnContent::refresh() {
    _last_access = std::chrono::steady_clock::now();
}

void NdnContent::add_data(uint64_t segment, std::shared_ptr<ndn::Data> data) {
    refresh();
    _datas[segment] = data;
}

std::map<uint64_t, std::shared_ptr<ndn::Data>>::iterator NdnContent::find_data(uint64_t segment) {
    refresh();
    return _datas.find(segment);
}

std::map<uint64_t, std::shared_ptr<ndn::Data>>::iterator NdnContent::end() {
    return _datas.end();
}
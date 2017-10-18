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

#include <ndn-cxx/name.hpp>

#include <memory>

#include "message.h"
#include "seekable_raw_stream.h"

class NdnMessage : public Message {
private:
    ndn::Name _name;

    ndn::time::milliseconds _freshness;

public:
    NdnMessage();

    NdnMessage(const std::shared_ptr<SeekableRawStream> &raw_stream);

    const ndn::Name& get_name() const;

    const NdnMessage& set_name(const ndn::Name &name);

    const ndn::time::milliseconds& get_freshness() const;

    const NdnMessage& set_freshness(const ndn::time::milliseconds& freshness);
};
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

#include <ndn-cxx/interest.hpp>

#include <memory>

#include "ndn_source.h"
#include "ndn_content.h"

class NdnSource;

class NdnSink {
protected:
    NdnSource *_ndn_source;

public:
    virtual void fromNdnSource(const std::shared_ptr<NdnContent> &content) = 0;

    void attachNdnSource(NdnSource *ndn_source) {
        _ndn_source = ndn_source;
    }
};
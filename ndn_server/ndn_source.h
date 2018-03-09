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

#include <memory>

#include "ndn_sink.h"
#include "ndn_content.h"

class NdnSink;

class NdnSource {
protected:
    NdnSink *_ndn_sink;

public:
    virtual void fromNdnSink(const std::shared_ptr<NdnContent> &ndn_content) = 0;
    
    void attachNdnSink(NdnSink *ndn_sink) {
        _ndn_sink = ndn_sink;
    }
};
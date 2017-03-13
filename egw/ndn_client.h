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

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/data.hpp>

#include <memory>
#include <thread>

#include "global.h"
#include "module.h"
#include "ndn_sink.h"
#include "ndn_message.h"

class NdnClient : public Module, public NdnSink {
private:
    ndn::Face _face;

    std::thread _face_thread;

public:
    NdnClient(size_t concurrency = 1);

    ~NdnClient();

    virtual void run() override;

    void start_face();

    virtual void solve(std::shared_ptr<NdnMessage> message) override;

    void solve_handler(std::shared_ptr<NdnMessage> message);

    void on_data(const ndn::Interest &interest, const ndn::Data &data, std::shared_ptr<NdnMessage> message);

    void on_timeout(const ndn::Interest &interest, std::shared_ptr<NdnMessage> message, int left);
};
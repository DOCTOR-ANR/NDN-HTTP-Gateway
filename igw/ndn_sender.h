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

#include <ndn-cxx/face.hpp>

#include "sub_module.h"
#include "ndn_producer.h"
#include "offloaded_ndn_producer.h"

class NdnProducerSubModule : public SubModule<OffloadedNdnProducer>, public NdnProducer {
private:
    ndn::Name _prefix;
    ndn::Face _face;

public:
    NdnProducerSubModule(OffloadedNdnProducer &parent, const ndn::Name &prefix);

    ~NdnProducerSubModule() override = default;

    void run() override;

    void publish(const std::shared_ptr<ndn::Data> &data) override;

private:
    void publishHandler(const std::shared_ptr<ndn::Data> &data);

    void onInterest(const ndn::Interest &interest);

    void onRegisterFailed();
};
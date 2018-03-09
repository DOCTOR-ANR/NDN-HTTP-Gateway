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
#include "ndn_consumer.h"
#include "offloaded_ndn_consumer.h"

class NdnConsumerSubModule : public SubModule<OffloadedNdnConsumer>, public NdnConsumer {
private:
    ndn::Face _face;

public:
    explicit NdnConsumerSubModule(OffloadedNdnConsumer &parent);

    ~NdnConsumerSubModule() override = default;

    void run() override;

    void retrieve(const ndn::Name &name) override;

private:
    void retrieveHandler(const ndn::Name &name);

    void onData(const ndn::Interest &interest, const ndn::Data &data, const std::shared_ptr<NdnContent> &content, size_t seg);

    void onTimeout(const ndn::Interest &interest, const std::shared_ptr<NdnContent> &content, size_t seg, size_t remaining_tries);

    void onNack(const ndn::Interest &interest, const ndn::lp::Nack &nack, const std::shared_ptr<NdnContent> &content);
};
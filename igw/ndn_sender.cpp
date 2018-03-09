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

#include "ndn_sender.h"

NdnProducerSubModule::NdnProducerSubModule(OffloadedNdnProducer &parent, const ndn::Name &prefix) : SubModule(1, parent), _prefix(prefix), _face(_ios) {
    _parent.attachNdnProducer(this);
}

void NdnProducerSubModule::run() {
    _face.setInterestFilter(_prefix, bind(&NdnProducerSubModule::onInterest, this, _2), boost::bind(&NdnProducerSubModule::onRegisterFailed, this));
}

void NdnProducerSubModule::publish(const std::shared_ptr<ndn::Data> &data) {
    _ios.post(boost::bind(&NdnProducerSubModule::publishHandler, this, data));
}

void NdnProducerSubModule::publishHandler(const std::shared_ptr<ndn::Data> &data) {
    try {
        _face.put(*data);
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}

void NdnProducerSubModule::onInterest(const ndn::Interest &interest) {
    _parent.fromNdnProducer(interest);
}

void NdnProducerSubModule::onRegisterFailed() {
    std::exit(-1);
}

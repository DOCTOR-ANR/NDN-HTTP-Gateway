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

#include "ndn_receiver.h"

#include <ndn-cxx/transport/tcp-transport.hpp>

static const ndn::time::milliseconds INTERESTDEFAULTLIFETIME {1500};

NdnConsumerSubModule::NdnConsumerSubModule(OffloadedNdnConsumer &parent)
        : SubModule(1, parent)
        , _face(std::make_shared<ndn::TcpTransport>("127.0.0.1", "6363"), _ios) {
    _parent.attachNdnConsumer(this);
}

void NdnConsumerSubModule::run() {

}

void NdnConsumerSubModule::retrieve(const ndn::Name &name) {
    _ios.post(boost::bind(&NdnConsumerSubModule::retrieveHandler, this, name));
}

void NdnConsumerSubModule::retrieveHandler(const ndn::Name &name) {
    auto content = std::make_shared<NdnContent>();
    content->setName(name);
    _face.expressInterest(ndn::Interest(name, INTERESTDEFAULTLIFETIME).setMustBeFresh(true),
                          boost::bind(&NdnConsumerSubModule::onData, this, _1, _2, content, 0),
                          boost::bind(&NdnConsumerSubModule::onNack, this, _1, _2, content),
                          boost::bind(&NdnConsumerSubModule::onTimeout, this, _1, content, 0, 2));
}

void NdnConsumerSubModule::onData(const ndn::Interest &interest, const ndn::Data &data, const std::shared_ptr<NdnContent> &content, size_t seg) {
    if(data.getName().get(-1).isSegment() && data.getName().get(-1).toSegment() != seg) {
        // if here => library problem, only appear for 1st packet
        _face.expressInterest(ndn::Interest(data.getName().getPrefix(-1).appendSegment(seg), INTERESTDEFAULTLIFETIME).setMustBeFresh(true),
                              boost::bind(&NdnConsumerSubModule::onData, this, _1, _2, content, seg),
                              boost::bind(&NdnConsumerSubModule::onNack, this, _1, _2, content),
                              boost::bind(&NdnConsumerSubModule::onTimeout, this, _1, content, seg, 2));
    } else {
        content->getRawStream()->append_raw_data((const char *) data.getContent().value(), data.getContent().value_size());
        if (data.getFinalBlockId().empty()) {
            _face.expressInterest(ndn::Interest(ndn::Name(data.getName().getPrefix(-1)).appendSegment(seg + 1), INTERESTDEFAULTLIFETIME).setMustBeFresh(true),
                                  boost::bind(&NdnConsumerSubModule::onData, this, _1, _2, content, seg + 1),
                                  boost::bind(&NdnConsumerSubModule::onNack, this, _1, _2, content),
                                  boost::bind(&NdnConsumerSubModule::onTimeout, this, _1, content, seg + 1, 2));
        } else {
            content->getRawStream()->is_completed(true);
        }
        if (!interest.getName().get(-1).isSegment() || interest.getName().get(-1).toSegment() == 0) {
            _parent.fromNdnConsumer(content);
        }
    }
}

void NdnConsumerSubModule::onTimeout(const ndn::Interest &interest, const std::shared_ptr<NdnContent> &content, size_t seg, size_t remaining_tries) {
    if(remaining_tries > 0) {
        ndn::Interest i(interest);
        i.setInterestLifetime(i.getInterestLifetime() * 2);
        i.refreshNonce();
        _face.expressInterest(i, boost::bind(&NdnConsumerSubModule::onData, this, _1, _2, content, seg),
                              boost::bind(&NdnConsumerSubModule::onNack, this, _1, _2, content),
                              boost::bind(&NdnConsumerSubModule::onTimeout, this, _1, content, seg, remaining_tries - 1));
    } else {
        content->getRawStream()->is_aborted(true);
        if (!interest.getName().get(-1).isSegment() || interest.getName().get(-1).toSegment() == 0) {
            _parent.fromNdnConsumer(content);
        }
        std::cout << interest.getName() << " unreachable" << std::endl;
    }
}

void NdnConsumerSubModule::onNack(const ndn::Interest &interest, const ndn::lp::Nack &nack, const std::shared_ptr<NdnContent> &content) {
    content->getRawStream()->is_aborted(true);
    if (!interest.getName().get(-1).isSegment() || interest.getName().get(-1).toSegment() == 0) {
        _parent.fromNdnConsumer(content);
    }
    std::cout << interest.getName() << " " << nack.getReason() << std::endl;
}

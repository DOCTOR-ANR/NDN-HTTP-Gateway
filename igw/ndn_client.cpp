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

#include "ndn_client.h"

#include <boost/bind.hpp>

NdnClient::NdnClient(size_t concurrency)
        : Module(concurrency)
        , _face() { //it seems that face can't handle multi-threaded io_service

}

NdnClient::~NdnClient() {
    _face.getIoService().stop();
    _face_thread.join();
}

void NdnClient::run() {
    _face_thread = std::thread(&NdnClient::start_face, this);
}

void NdnClient::start_face() {
    _face.processEvents(ndn::time::milliseconds(0), true);
}

void NdnClient::solve(std::shared_ptr<NdnMessage> message) {
    _ios.post(boost::bind(&NdnClient::resolve_handler, this, message));
}

void NdnClient::resolve_handler(std::shared_ptr<NdnMessage> message) {
    ndn::Interest interest(message->get_name(), ndn::time::milliseconds(500));
    interest.setMustBeFresh(true);
    _face.expressInterest(interest,
                          boost::bind(&NdnClient::on_notify, this, _1, _2, message),
                          boost::bind(&NdnClient::on_notify_timeout, this, _1, message, 7));
}

void NdnClient::on_data(const ndn::Interest &interest, const ndn::Data &data, std::shared_ptr<NdnMessage> message) {
    message->get_raw_stream()->append_raw_data((const char*)data.getContent().value(), data.getContent().value_size());
    if(data.getFinalBlockId().empty()) {
        ndn::Interest i(ndn::Name(data.getName().getPrefix(-1)).appendSegment(data.getName().get(-1).toSegment() + 1),
                        ndn::time::milliseconds(500));
        i.setMustBeFresh(true);
        _face.expressInterest(i, boost::bind(&NdnClient::on_data, this, _1, _2, message),
                              boost::bind(&NdnClient::on_timeout, this, _1, message, 7));
    } else {
        message->get_raw_stream()->is_completed(true);
    }
}

void NdnClient::on_notify(const ndn::Interest &interest, const ndn::Data &data, std::shared_ptr<NdnMessage> message) {
    int n = 0;
    while (!interest.getName().get(++n).isVersion());
    ndn::Name name(interest.getName().getPrefix(n));
    name.append(interest.getName().get(-1));
    ndn::Interest i(name, ndn::time::milliseconds(500));
    i.setMustBeFresh(true);
    _face.expressInterest(i, boost::bind(&NdnClient::on_data, this, _1, _2, message),
                          boost::bind(&NdnClient::on_timeout, this, _1, message, 7));
}

void NdnClient::on_timeout(const ndn::Interest &interest, std::shared_ptr<NdnMessage> message, int left) {
    if(left > 0) {
        ndn::Interest i(interest);
        i.setInterestLifetime(i.getInterestLifetime() + ndn::time::milliseconds(250));
        i.refreshNonce();
        _face.expressInterest(i, bind(&NdnClient::on_data, this, _1, _2, message),
                              bind(&NdnClient::on_timeout, this, _1, message, left - 1));
    } else {
        message->get_raw_stream()->is_aborted(true);
        std::cout << interest.getName() << " unreachable" << std::endl;
    }
}

void NdnClient::on_notify_timeout(const ndn::Interest &interest, std::shared_ptr<NdnMessage> message, int left) {
    if(left > 0) {
        ndn::Interest i(interest);
        i.setInterestLifetime(i.getInterestLifetime() + ndn::time::milliseconds(250));
        i.refreshNonce();
        _face.expressInterest(i, bind(&NdnClient::on_notify, this, _1, _2, message),
                              bind(&NdnClient::on_notify_timeout, this, _1, message, left - 1));
    } else {
        message->get_raw_stream()->is_aborted(true);
        std::cout << interest.getName() << " unreachable" << std::endl;
    }
}
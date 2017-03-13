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

#include "ndn_server.h"

NdnServer::Content::Content() : _last_access(std::chrono::steady_clock::now()) {

}

NdnServer::Content::~Content() {

}

std::chrono::steady_clock::time_point NdnServer::Content::get_last_access() {
    return _last_access;
}

void NdnServer::Content::add_data(uint64_t segment, std::shared_ptr<ndn::Data> data) {
    _last_access = std::chrono::steady_clock::now();
    _datas[segment] = data;
}

std::shared_ptr<ndn::Data> NdnServer::Content::get_data(uint64_t segment) {
    _last_access = std::chrono::steady_clock::now();
    auto it = _datas.find(segment);
    return it != _datas.end() ? it->second : std::shared_ptr<ndn::Data>();
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------

boost::posix_time::seconds NdnServer::DEFAULT_WAIT_PURGE = boost::posix_time::seconds(60);

NdnServer::NdnServer(ndn::Name prefix, size_t concurrency)
        : Module(concurrency)
        , _prefix(prefix)
        , _face() //it seems that face can't handle multi-threaded io_service
        , _purge_timer(_ios) {

}

NdnServer::~NdnServer() {
    _face.getIoService().stop();
    _face_thread.join();
}

void NdnServer::run() {
    _face_thread = std::thread(&NdnServer::start_face, this);
    _face.setInterestFilter(_prefix, bind(&NdnServer::on_interest, this, _2),
                            boost::bind(&NdnServer::on_register_failed, this));
    _purge_timer.expires_from_now(DEFAULT_WAIT_PURGE);
    _purge_timer.async_wait(boost::bind(&NdnServer::purge_old_data, this));
}

void NdnServer::start_face() {
    _face.processEvents(ndn::time::milliseconds(0), true);
}

void NdnServer::solve(std::shared_ptr<NdnMessage> ndn_message) {
    _ios.post(boost::bind(&NdnServer::resolve_handler, this, ndn_message));
}

void NdnServer::resolve_handler(std::shared_ptr<NdnMessage> ndn_message) {
    auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
    _ios.post(boost::bind(&NdnServer::generate_data, this, ndn_message, timer, 0));
}

void NdnServer::on_interest(const ndn::Interest &interest) {
    auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
    _ios.post(boost::bind(&NdnServer::on_interest_handler, this, interest, timer, 9));
}

void NdnServer::on_interest_handler(ndn::Interest interest, std::shared_ptr<boost::asio::deadline_timer> timer, int left) {
    if(left>0) {
        uint64_t segment;
        std::string name;

        if(interest.getName().get(-1).isSegment()){
            segment = interest.getName().get(-1).toSegment();
            name = interest.getName().getPrefix(-1).toUri();
        } else {
            segment = 0;
            name = interest.getName().toUri();
        }

        std::shared_ptr<Content> content;
        if(_contents.find(name, content)){
            auto data = content->get_data(segment);
            if (data) {
                _face.put(*data);
            } else {
                timer->expires_from_now(boost::posix_time::milliseconds((10 - left) * 10));
                timer->async_wait(boost::bind(&NdnServer::on_interest_handler, this, interest, timer, left-1));
            }
        } else {
            timer->expires_from_now(boost::posix_time::milliseconds((10 - left) * 10));
            timer->async_wait(boost::bind(&NdnServer::on_interest_handler, this, interest, timer, left-1));
        }
    } else {
        std::cout << interest.getName() << " dropped" << std::endl;
    }
}

void NdnServer::generate_data(std::shared_ptr<NdnMessage> ndn_message, std::shared_ptr<boost::asio::deadline_timer> timer, uint64_t segment) {
    int generation_tokens = 16;
    char buffer[global::DEFAULT_BUFFER_SIZE];
    long read_bytes;
    while(generation_tokens > 0 && (read_bytes = ndn_message->get_raw_stream()->read_raw_data(buffer, global::DEFAULT_BUFFER_SIZE)) > 0){
        auto data = std::make_shared<ndn::Data>(ndn::Name(ndn_message->get_name()).appendSegment(segment));
        data->setContent((uint8_t*)buffer, read_bytes);
        if(!ndn_message->get_raw_stream()->has_remaining_bytes()){
            data->setFinalBlockId(data->getName().get(-1));
        }
        data->setFreshnessPeriod(ndn::time::milliseconds(0));
        _keychain.signWithSha256(*data);

        if(segment == 0){
            _contents.insert(ndn::Name(ndn_message->get_name()), std::make_shared<Content>());
        }
        _contents.find(ndn_message->get_name())->add_data(segment, data);

        ++segment;
        --generation_tokens;
    }
    if (read_bytes != 0) {
        timer->expires_from_now(global::DEFAULT_WAIT_REDO);
        timer->async_wait(boost::bind(&NdnServer::generate_data, this, ndn_message, timer, segment));
    }
}

void NdnServer::purge_old_data() {
    auto lt = _contents.lock_table();
    auto time_point = std::chrono::steady_clock::now();
    auto it = lt.begin();
    while(it != lt.end()){
        if(it->second->get_last_access() + std::chrono::seconds(15) < time_point) {
            it = lt.erase(it);
        } else {
            ++it;
        }
    }
    _purge_timer.expires_from_now(DEFAULT_WAIT_PURGE);
    _purge_timer.async_wait(boost::bind(&NdnServer::purge_old_data, this));
}


void NdnServer::on_register_failed() {
    exit(-1);
}
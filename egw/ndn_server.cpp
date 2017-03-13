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

const std::chrono::steady_clock::time_point& NdnServer::Content::get_last_access() const {
    return _last_access;
}

void NdnServer::Content::add_data(uint64_t segment, std::shared_ptr<ndn::Data> data) {
    _last_access = std::chrono::steady_clock::now();
    _datas[segment] = data;
}

std::map<uint64_t, std::shared_ptr<ndn::Data>>::iterator NdnServer::Content::find_data(uint64_t segment) {
    _last_access = std::chrono::steady_clock::now();
    return _datas.find(segment);
}

std::map<uint64_t, std::shared_ptr<ndn::Data>>::iterator NdnServer::Content::end() {
    return _datas.end();
}

//----------------------------------------------------------------------------------------------------------------------

boost::posix_time::seconds NdnServer::DEFAULT_WAIT_PURGE = boost::posix_time::seconds(60);

NdnServer::NdnServer(ndn::Name prefix, size_t concurrency)
        : Module(concurrency)
        , _prefix(prefix)
        , _face(/*_ios*/) //it seems that face can't handle multi-threaded io_service
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
    _ios.post(boost::bind(&NdnServer::solve_handler, this, ndn_message));
}

void NdnServer::solve_handler(const std::shared_ptr<NdnMessage> &ndn_message) {
    auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
    _ios.post(boost::bind(&NdnServer::generate_data, this, ndn_message, timer, 0));
}

void NdnServer::on_interest(const ndn::Interest &interest) {
    _ios.post(boost::bind(&NdnServer::on_interest_handler, this, interest));
}

void NdnServer::on_interest_handler(const ndn::Interest &interest) {
    //check if notify
    bool notify = false;
    for(const auto& component : interest.getName()){
        if(component.isVersion()){
            notify = true;
            break;
        }
    }

    if(notify){
        _ios.post(boost::bind(&NdnServer::reply_notify, this, interest));
    } else {
        auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
        _ios.post(boost::bind(&NdnServer::reply_content, this, interest, timer, 9));
    }
}

void NdnServer::reply_notify(const ndn::Interest &interest) {
    int n=0;
    while(!interest.getName().get(++n).isVersion());
    ndn::Name name(interest.getName().getPrefix(n));
    name.append(interest.getName().get(-1));

    if(!_contents.contains(name)) {
        auto message = std::make_shared<NdnMessage>();
        message->set_name(interest.getName());
        _ndn_sink->solve(message);
    }

    auto data = std::make_shared<ndn::Data>(interest.getName());
    data->setFreshnessPeriod(ndn::time::milliseconds(0));
    _keychain.signWithSha256(*data);
    _face.put(*data);
}

void NdnServer::reply_content(const ndn::Interest &interest, const std::shared_ptr<boost::asio::deadline_timer> &timer, int left) {
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
            auto it = content->find_data(segment);
            if (it != content->end()) {
                _face.put(*it->second);
            } else {
                timer->expires_from_now(boost::posix_time::milliseconds((10 - left) * 10));
                timer->async_wait(boost::bind(&NdnServer::reply_content, this, interest, timer, left-1));
            }
        } else {
            timer->expires_from_now(boost::posix_time::milliseconds((10 - left) * 10));
            timer->async_wait(boost::bind(&NdnServer::reply_content, this, interest, timer, left-1));
        }
    } else {
        std::cout << interest.getName() << " dropped" << std::endl;
    }
}

void NdnServer::generate_data(const std::shared_ptr<NdnMessage> &ndn_message, const std::shared_ptr<boost::asio::deadline_timer> &timer, uint64_t segment) {
    int generation_tokens = 16;
    char buffer[global::DEFAULT_BUFFER_SIZE];
    long read_bytes;
    while(generation_tokens > 0 && (read_bytes = ndn_message->get_raw_stream()->read_raw_data(buffer, global::DEFAULT_BUFFER_SIZE)) > 0){
        auto data = std::make_shared<ndn::Data>(ndn::Name(ndn_message->get_name()).appendSegment(segment));
        data->setContent((uint8_t*)buffer, read_bytes);
        if(!ndn_message->get_raw_stream()->has_remaining_bytes()){
            data->setFinalBlockId(data->getName().get(-1));
        }
        data->setFreshnessPeriod(ndn_message->get_freshness());
        _keychain.signWithSha256(*data);
        //std::cout << *data << std::endl;
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

void NdnServer::attach_ndn_sinks(NdnSink *ndn_sink) {
    _ndn_sink = ndn_sink;
}

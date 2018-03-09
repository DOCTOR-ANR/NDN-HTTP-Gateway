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

#include "ndn_resolver.h"

NdnResolver::NdnResolver(size_t concurrency)
        : Module(concurrency)
        , _purge_timer(_ios) {

}

void NdnResolver::run() {

    _purge_timer.expires_from_now(global::DEFAULT_WAIT_PURGE);
    _purge_timer.async_wait(boost::bind(&NdnResolver::purge_old_data, this));
}

void NdnResolver::fromNdnProducer(const ndn::Interest &interest) {
    _ios.post(boost::bind(&NdnResolver::fromNdnProducerHandler, this, interest));
}

void NdnResolver::fromNdnConsumer(const std::shared_ptr<NdnContent> &content) {
    _ios.post(boost::bind(&NdnResolver::fromNdnConsumerHandler, this, content));
}

void NdnResolver::fromNdnSink(const std::shared_ptr<NdnContent> &content) {
    _ios.post(boost::bind(&NdnResolver::fromNdnSinkHandler, this, content));
}

void NdnResolver::fromNdnProducerHandler(const ndn::Interest &interest) {
    ndn::Name client_prefix;
    try {
        client_prefix.wireDecode(interest.getName().get(-2).blockFromValue());
        ndn::Name::Component hash(interest.getName().get(-1));

        ndn::Name content_name(interest.getName().getPrefix(-2));
        content_name.append(hash);

        std::string state;
        { // block for RAII
            std::lock_guard<std::mutex> lock(_map_mutex);
            auto contents_it = _contents.find(content_name.toUri());
            if(contents_it == _contents.end()) {
                _ndn_consumer->retrieve(client_prefix.append(hash));
                state = "OK";
            } else {
                contents_it->second->refresh();
                state = "SKIP";
            }
        }

        auto data = std::make_shared<ndn::Data>(interest.getName());
        data->setFreshnessPeriod(ndn::time::milliseconds(0));
        data->setFinalBlockId(ndn::Name::Component::fromSegment(0));
        data->setContent((uint8_t*)state.c_str(), state.size());
        _keychain.sign(*data, ndn::security::SigningInfo(ndn::security::SigningInfo::SIGNER_TYPE_SHA256));
        _ndn_producer->publish(data);
    } catch (const std::exception &e) {
        auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
        checkContent(interest, timer, 7);
    }
}

void NdnResolver::fromNdnConsumerHandler(const std::shared_ptr<NdnContent> &content) {
    if (!content->getRawStream()->is_aborted()) {
        _ndn_sink->fromNdnSource(content);
    } else {
        std::cout << "can't get content with name " << content->getName() << std::endl;
    }
}

void NdnResolver::fromNdnSinkHandler(const std::shared_ptr<NdnContent> &content) {
    { // block for RAII
        std::lock_guard<std::mutex> lock(_map_mutex);
        _contents.emplace(content->getName().toUri(), content);
    }
    auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
    generate_data(content, timer);
}

void NdnResolver::checkContent(const ndn::Interest &interest, const std::shared_ptr<boost::asio::deadline_timer> &timer, size_t remaining_tries) {
    if (remaining_tries > 0) {
        uint64_t segment;
        std::string name;
        if (interest.getName().get(-1).isSegment()) {
            segment = interest.getName().get(-1).toSegment();
            name = interest.getName().getPrefix(-2).toUri();
        } else {
            segment = 0;
            name = interest.getName().toUri();
        }

        std::shared_ptr<NdnContent> content;
        { // block for RAII
            std::lock_guard<std::mutex> lock(_map_mutex);
            auto contents_it = _contents.find(name);
            if(contents_it != _contents.end()) {
                content = contents_it->second;
            }
        }
        if (content) {
            auto datas_it = content->findData(segment);
            if (datas_it != content->end()) {
                _ndn_producer->publish(datas_it->second);
            } else {
                timer->expires_from_now(boost::posix_time::milliseconds((8 - remaining_tries) * 25));
                timer->async_wait(boost::bind(&NdnResolver::checkContent, this, interest, timer, remaining_tries - 1));
            }
        } else {
            timer->expires_from_now(boost::posix_time::milliseconds((8 - remaining_tries) * 25));
            timer->async_wait(boost::bind(&NdnResolver::checkContent, this, interest, timer, remaining_tries - 1));
        }
    } else {
        //std::cout << interest.getName() << " dropped" << std::endl;
    }
}

void NdnResolver::generate_data(const std::shared_ptr<NdnContent> &content, const std::shared_ptr<boost::asio::deadline_timer> &timer, uint64_t segment) {
    int generation_tokens = 16;
    char buffer[global::DEFAULT_BUFFER_SIZE];
    long read_bytes;
    while(generation_tokens > 0 && (read_bytes = content->getRawStream()->readRawData(segment * global::DEFAULT_BUFFER_SIZE, buffer, global::DEFAULT_BUFFER_SIZE)) > 0){
        auto data = std::make_shared<ndn::Data>(ndn::Name(content->getName()).appendTimestamp(content->getTimestamp()).appendSegment(segment));
        data->setContent((uint8_t*)buffer, read_bytes);
        if(content->getRawStream()->remainingBytes(segment * global::DEFAULT_BUFFER_SIZE + read_bytes) == 0){
            data->setFinalBlockId(ndn::Name::Component::fromSegment(segment));
        }
        data->setFreshnessPeriod(content->getFreshness());
        _keychain.sign(*data, ndn::security::SigningInfo(ndn::security::SigningInfo::SIGNER_TYPE_SHA256));

        content->addData(segment, data);

        ++segment;
        --generation_tokens;
    }

    if (read_bytes != 0) {
        timer->expires_from_now(global::DEFAULT_WAIT_REDO);
        timer->async_wait(boost::bind(&NdnResolver::generate_data, this, content, timer, segment));
    } else if(content->getRawStream()->is_aborted()) {
        // remove uncompleted content
        std::lock_guard<std::mutex> lock(_map_mutex);
        _contents.erase(content->getName().toUri());
    }
}

void NdnResolver::purge_old_data() {
    auto time_point = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(_map_mutex);
#ifndef NDEBUG
    size_t remove_count = 0;
#endif
    auto it = _contents.begin();
    while(it != _contents.end()) {
        if(it->second->getLastAccess() + std::chrono::seconds(30) < time_point) {
            it = _contents.erase(it);
#ifndef NDEBUG
            ++remove_count;
#endif
        } else {
            ++it;
        }
    }
#ifndef NDEBUG
    std::cout << remove_count << " content(s) removed from buffer (" << _contents.size() << " remaining contents)" << std::endl;
#endif
    _purge_timer.expires_from_now(global::DEFAULT_WAIT_PURGE);
    _purge_timer.async_wait(boost::bind(&NdnResolver::purge_old_data, this));
}

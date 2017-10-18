#include "ndn_resolver.h"

NdnResolver::NdnResolver(const ndn::Name &prefix, size_t concurrency)
        : Module(concurrency)
        , _prefix(prefix.wireEncode())
        , _purge_timer(_ios) {

}

void NdnResolver::run() {
    _purge_timer.expires_from_now(global::DEFAULT_WAIT_PURGE);
    _purge_timer.async_wait(boost::bind(&NdnResolver::purgeOldContents, this));
}

void NdnResolver::fromNdnProducer(const ndn::Interest &interest) {
    _ios.post(boost::bind(&NdnResolver::fromNdnProducerHandler, this, interest));
}

void NdnResolver::fromNdnConsumer(const std::shared_ptr<NdnContent> &content) {
    _ios.post(boost::bind(&NdnResolver::fromNdnConsumerHandler, this, content));
}

void NdnResolver::fromNdnSource(const std::shared_ptr<NdnContent> &content) {
    _ios.post(boost::bind(&NdnResolver::fromNdnSourceHandler, this, content));
}

void NdnResolver::fromNdnProducerHandler(const ndn::Interest &interest) {
    auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
    checkForContent(interest, timer, 7);
}

void NdnResolver::fromNdnConsumerHandler(const std::shared_ptr<NdnContent> &content) {
    ndn::Name prefix;
    try {
        prefix.wireDecode(content->getName().get(-2).blockFromValue());
        ndn::Name name(content->getName().getPrefix(-2));
        name.append(content->getName().get(-1));

        if (content->getRawStream()->raw_data_as_string() == "0") {
            std::shared_ptr<NdnContent> request;
            { // block for RAII
                std::lock_guard<std::mutex> lock(_contents_mutex);
                auto it = _contents.find(name.get(-1).toUri());
                if (it != _contents.end()) {
                    request = it->second;
                }
            }
            if (request && request->currentSegmentCount() > 4) {
                auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
                timer->expires_from_now(boost::posix_time::seconds(5));
                timer->async_wait(boost::bind(&NdnResolver::waitContentCompletion, this, name, timer));
                std::lock_guard<std::mutex> lock(_contents_mutex);
                auto result = _pendings.emplace(name.get(-1).toUri(), timer);
            } else {
                _ndn_consumer->retrieve(name);
            }
        } else {
            _ndn_consumer->retrieve(name);
        }
    } catch (const std::exception &e) {
        _ndn_source->fromNdnSink(content);
    }
}

void NdnResolver::fromNdnSourceHandler(const std::shared_ptr<NdnContent> &content) {
    ndn::Name old_name = content->getName();
    ndn::Name new_name(_prefix);
    content->setName(new_name.append(old_name.get(-1)));
    { // block for RAII
        std::lock_guard<std::mutex> lock(_contents_mutex);
        _contents.emplace(content->getName().toUri(), content);
    }
    ndn::Name notify_name(old_name.getPrefix(-1));
    _ndn_consumer->retrieve(notify_name.append(_prefix).append(old_name.get(-1)));
    auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
    generateDataPackets(content, timer);
}

void NdnResolver::checkForContent(const ndn::Interest &interest,
                                  const std::shared_ptr<boost::asio::deadline_timer> &timer, size_t remaining_tries) {
    if (remaining_tries > 0) {
        uint64_t segment;
        std::string name;
        if (interest.getName().get(-1).isSegment()) {
            segment = interest.getName().get(-1).toSegment();
            name = interest.getName().getPrefix(-1).toUri();
        } else {
            segment = 0;
            name = interest.getName().toUri();
        }

        std::shared_ptr<NdnContent> content;
        { // block for RAII
            std::lock_guard<std::mutex> lock(_contents_mutex);
            auto it = _contents.find(name);
            if (it != _contents.end()) {
                content = it->second;
            }
        }
        if (content) {
            auto it = content->findData(segment);
            if (it != content->end()) {
                _ndn_producer->publish(it->second);
                std::shared_ptr<boost::asio::deadline_timer> pending_timer;
                { // block for RAII
                    std::lock_guard<std::mutex> lock(_pendings_mutex);
                    auto it = _pendings.find(content->getName().get(-1).toUri());
                    if (it != _pendings.end()) {
                        std::cout << "ok" << std::endl;
                        pending_timer = it->second;
                    }
                }
                if (pending_timer) {
                    std::cout << it->second->getFinalBlockId() << std::endl;
                    if (it->second->getFinalBlockId().empty()) {
                        std::cerr << "update" << std::endl;
                        pending_timer->expires_from_now(boost::posix_time::seconds(5));
                    } else {
                        std::cerr << "cancel" << std::endl;
                        pending_timer->cancel();
                    }
                }
            } else {
                timer->expires_from_now(boost::posix_time::milliseconds((8 - remaining_tries) * 25));
                timer->async_wait(boost::bind(&NdnResolver::checkForContent, this, interest, timer, remaining_tries - 1));
            }
        } else {
            timer->expires_from_now(boost::posix_time::milliseconds((8 - remaining_tries) * 25));
            timer->async_wait(boost::bind(&NdnResolver::checkForContent, this, interest, timer, remaining_tries - 1));
        }
    } else {
        //std::cout << interest.getName() << " dropped" << std::endl;
    }
}

void NdnResolver::waitContentCompletion(const ndn::Name &name, const std::shared_ptr<boost::asio::deadline_timer> &timer) {
    if (timer->expires_at() <= boost::asio::deadline_timer::traits_type::now()) {
        std::cerr << "do" << std::endl;
        { // block for RAII
            std::lock_guard<std::mutex> lock(_pendings_mutex);
            _pendings.erase(name.get(-1).toUri());
        }
        _ndn_consumer->retrieve(name);
    } else {
        std::cerr << "wait" << std::endl;
        timer->async_wait(boost::bind(&NdnResolver::waitContentCompletion, this, name, timer));
    }
}

void NdnResolver::generateDataPackets(const std::shared_ptr<NdnContent> &content,
                                      const std::shared_ptr<boost::asio::deadline_timer> &timer, uint64_t segment) {
    int generation_tokens = 16;
    char buffer[global::DEFAULT_BUFFER_SIZE];
    long read_bytes;
    while(generation_tokens > 0 && (read_bytes = content->getRawStream()->readRawData(segment * global::DEFAULT_BUFFER_SIZE, buffer, global::DEFAULT_BUFFER_SIZE)) > 0){
        auto data = std::make_shared<ndn::Data>(ndn::Name(content->getName()).appendSegment(segment));
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
        timer->async_wait(boost::bind(&NdnResolver::generateDataPackets, this, content, timer, segment));
    } else if(content->getRawStream()->is_aborted()) {
        std::lock_guard<std::mutex> lock(_contents_mutex);
        _contents.erase(content->getName().toUri());
    }
}

void NdnResolver::purgeOldContents() {
    std::lock_guard<std::mutex> lock(_contents_mutex);
    auto time_point = std::chrono::steady_clock::now();
    auto it = _contents.begin();
    while(it != _contents.end()){
        if(it->second->getLastAccess() + std::chrono::seconds(15) < time_point) {
            it = _contents.erase(it);
        } else {
            ++it;
        }
    }
    _purge_timer.expires_from_now(global::DEFAULT_WAIT_PURGE);
    _purge_timer.async_wait(boost::bind(&NdnResolver::purgeOldContents, this));
}

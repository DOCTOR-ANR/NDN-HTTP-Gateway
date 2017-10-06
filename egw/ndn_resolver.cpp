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

        std::shared_ptr<NdnContent> content;
        if (!_contents.find(content_name.toUri(), content)) {
            _ndn_consumer->retrieve(ndn::Interest(client_prefix.append(hash)));
        } else {
            content->refresh();
        }
        auto data = std::make_shared<ndn::Data>(interest.getName());
        data->setFreshnessPeriod(ndn::time::milliseconds(0));
        _keychain.sign(*data, ndn::security::SigningInfo(ndn::security::SigningInfo::SIGNER_TYPE_SHA256));
        _ndn_producer->publish(data);
    } catch (const std::exception &e) {
        auto timer = std::make_shared<boost::asio::deadline_timer>(_ios);
        checkContent(interest, timer, 7);
    }
}

void NdnResolver::fromNdnConsumerHandler(const std::shared_ptr<NdnContent> &content) {
    if (!content->get_raw_stream()->is_aborted()) {
        _ndn_sink->fromNdnSource(content);
    } else {
        std::cout << "can't get content with name " << content->get_name() << std::endl;
    }
}

void NdnResolver::fromNdnSinkHandler(const std::shared_ptr<NdnContent> &content) {
    _contents.insert(content->get_name().toUri(), std::shared_ptr<NdnContent>(content));
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
        if (_contents.find(name, content)) {
            auto it = content->find_data(segment);
            if (it != content->end()) {
                _ndn_producer->publish(it->second);
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
    while(generation_tokens > 0 && (read_bytes = content->get_raw_stream()->read_raw_data(buffer, global::DEFAULT_BUFFER_SIZE)) > 0){
        auto data = std::make_shared<ndn::Data>(ndn::Name(content->get_name()).appendTimestamp(content->get_timestamp()).appendSegment(segment));
        data->setContent((uint8_t*)buffer, read_bytes);
        if(!content->get_raw_stream()->has_remaining_bytes()){
            data->setFinalBlockId(ndn::Name::Component::fromSegment(segment));
        }
        data->setFreshnessPeriod(content->get_freshness());
        _keychain.sign(*data, ndn::security::SigningInfo(ndn::security::SigningInfo::SIGNER_TYPE_SHA256));

        content->add_data(segment, data);

        ++segment;
        --generation_tokens;
    }

    if (read_bytes != 0) {
        timer->expires_from_now(global::DEFAULT_WAIT_REDO);
        timer->async_wait(boost::bind(&NdnResolver::generate_data, this, content, timer, segment));
    } else if(content->get_raw_stream()->is_aborted()) {
        std::cerr << "only partial content for " << content->get_name().getPrefix(-1) << std::endl;
        _contents.erase(content->get_name().toUri());
    }
}

void NdnResolver::purge_old_data() {
    auto lt = _contents.lock_table();
    auto time_point = std::chrono::steady_clock::now();
    auto it = lt.begin();
    while(it != lt.end()){
        if(it->second->get_last_access() + std::chrono::seconds(30) < time_point) {
            it = lt.erase(it);
        } else {
            ++it;
        }
    }
    _purge_timer.expires_from_now(global::DEFAULT_WAIT_PURGE);
    _purge_timer.async_wait(boost::bind(&NdnResolver::purge_old_data, this));
}

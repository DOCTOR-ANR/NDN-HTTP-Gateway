#pragma once

#include <ndn-cxx/name.hpp>

#include "libcuckoo/cuckoohash_map.hh"

#include "global.h"
#include "module.h"
#include "offloaded_ndn_consumer.h"
#include "ndn_receiver.h"
#include "offloaded_ndn_producer.h"
#include "ndn_sender.h"
#include "ndn_source.h"
#include "ndn_content.h"

class NdnResolver : public Module, public OffloadedNdnConsumer, public OffloadedNdnProducer, public NdnSource  {
private:
    ndn::KeyChain _keychain;

    boost::asio::deadline_timer _purge_timer;
    cuckoohash_map<std::string, std::shared_ptr<NdnContent>> _contents;

public:
    NdnResolver(size_t concurrency);

    void run() override;

    void fromNdnProducer(const ndn::Interest &interest) override;

    void fromNdnConsumer(const std::shared_ptr<NdnContent> &content) override;

    void fromNdnSink(const std::shared_ptr<NdnContent> &content) override;

private:
    void fromNdnProducerHandler(const ndn::Interest &interest);

    void fromNdnConsumerHandler(const std::shared_ptr<NdnContent> &content);

    void fromNdnSinkHandler(const std::shared_ptr<NdnContent> &content);

    void checkContent(const ndn::Interest &interest, const std::shared_ptr<boost::asio::deadline_timer> &timer, size_t remaining_tries);

    void generate_data(const std::shared_ptr<NdnContent> &content, const std::shared_ptr<boost::asio::deadline_timer> &timer, uint64_t segment = 0);

    void purge_old_data();
};
#pragma once

#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/data.hpp>
#include <ndn-cxx/security/key-chain.hpp>

#include <unordered_map>
#include <mutex>

#include "global.h"
#include "module.h"
#include "offloaded_ndn_consumer.h"
#include "offloaded_ndn_producer.h"
#include "ndn_sink.h"
#include "ndn_content.h"

class NdnResolver : public Module, public NdnSink, public OffloadedNdnConsumer, public OffloadedNdnProducer {
private:
    ndn::Block _prefix;
    ndn::KeyChain _keychain;

    boost::asio::deadline_timer _purge_timer;
    std::mutex _contents_mutex;
    std::unordered_map<std::string, std::shared_ptr<NdnContent>> _contents;

    std::mutex _pendings_mutex;
    std::unordered_map<std::string, std::shared_ptr<boost::asio::deadline_timer>> _pendings;

public:
    explicit NdnResolver(const ndn::Name &prefix, size_t concurrency = 1);

    ~NdnResolver() override = default;

    void run() override;

    void fromNdnProducer(const ndn::Interest &interest) override;

    void fromNdnConsumer(const std::shared_ptr<NdnContent> &content) override;

    void fromNdnSource(const std::shared_ptr<NdnContent> &content) override;

private:
    void fromNdnProducerHandler(const ndn::Interest &interest);

    void fromNdnConsumerHandler(const std::shared_ptr<NdnContent> &content);

    void fromNdnSourceHandler(const std::shared_ptr<NdnContent> &content);

    void checkForContent(const ndn::Interest &interest, const std::shared_ptr<boost::asio::deadline_timer> &timer,
                         size_t remaining_tries);

    void waitContentCompletion(const ndn::Name &name, const std::shared_ptr<boost::asio::deadline_timer> &timer);

    void generateDataPackets(const std::shared_ptr<NdnContent> &content,
                             const std::shared_ptr<boost::asio::deadline_timer> &timer, uint64_t segment = 0);

    void purgeOldContents();
};
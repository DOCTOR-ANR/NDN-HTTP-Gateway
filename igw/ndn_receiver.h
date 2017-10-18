#pragma once

#include <ndn-cxx/face.hpp>

#include "sub_module.h"
#include "ndn_consumer.h"
#include "offloaded_ndn_consumer.h"

class NdnConsumerSubModule : public SubModule<OffloadedNdnConsumer>, public NdnConsumer {
private:
    ndn::Face _face;

public:
    explicit NdnConsumerSubModule(OffloadedNdnConsumer &parent);

    ~NdnConsumerSubModule() override = default;

    void run() override;

    void retrieve(const ndn::Name &name) override;

private:
    void retrieveHandler(const ndn::Name &name);

    void onData(const ndn::Interest &interest, const ndn::Data &data, const std::shared_ptr<NdnContent> &content,
                size_t seg);

    void onTimeout(const ndn::Interest &interest, const std::shared_ptr<NdnContent> &content, size_t seg,
                   size_t remaining_tries);

    void onNack(const ndn::Interest &interest, const ndn::lp::Nack &nack, const std::shared_ptr<NdnContent> &content);
};
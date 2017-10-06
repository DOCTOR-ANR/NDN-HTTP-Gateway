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

    ~NdnConsumerSubModule();

    void run() override;

    void retrieve(const ndn::Interest &interest) override;

private:
    void retrieveHandler(const ndn::Interest &interest);

    void onData(const ndn::Interest &interest, const ndn::Data &data, const std::shared_ptr<NdnContent> &content);

    void onTimeout(const ndn::Interest &interest, const std::shared_ptr<NdnContent> &content, int remaining_tries);

    void onNack(const ndn::Interest &interest, const ndn::lp::Nack &nack, const std::shared_ptr<NdnContent> &content);
};
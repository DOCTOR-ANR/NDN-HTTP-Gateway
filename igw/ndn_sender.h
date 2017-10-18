#pragma once

#include <ndn-cxx/face.hpp>

#include "sub_module.h"
#include "ndn_producer.h"
#include "offloaded_ndn_producer.h"

class NdnProducerSubModule : public SubModule<OffloadedNdnProducer>, public NdnProducer {
private:
    ndn::Name _prefix;
    ndn::Face _face;

public:
    NdnProducerSubModule(OffloadedNdnProducer &parent, const ndn::Name &prefix);

    ~NdnProducerSubModule() override = default;

    void run() override;

    void publish(const std::shared_ptr<ndn::Data> &data) override;

private:
    void publishHandler(const std::shared_ptr<ndn::Data> &data);

    void onInterest(const ndn::Interest &interest);

    void onRegisterFailed();
};
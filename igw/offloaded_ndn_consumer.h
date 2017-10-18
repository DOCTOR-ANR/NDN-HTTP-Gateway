#pragma once

#include <ndn-cxx/interest.hpp>

#include <memory>

#include "ndn_consumer.h"
#include "ndn_content.h"

class OffloadedNdnConsumer {
protected:
    NdnConsumer* _ndn_consumer;

public:
    virtual void fromNdnConsumer(const std::shared_ptr<NdnContent> &content) = 0;

    void attachNdnConsumer(NdnConsumer *ndn_consumer) {
        _ndn_consumer = ndn_consumer;
    }
};
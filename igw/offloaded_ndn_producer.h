#pragma once

#include <ndn-cxx/interest.hpp>

#include "ndn_producer.h"

class OffloadedNdnProducer {
protected:
    NdnProducer* _ndn_producer;

public:
    void attachNdnProducer(NdnProducer *ndn_producer) {
        _ndn_producer = ndn_producer;
    }

    virtual void fromNdnProducer(const ndn::Interest &interest) = 0;
};
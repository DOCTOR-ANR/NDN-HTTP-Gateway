#pragma once

#include <ndn-cxx/data.hpp>

#include <memory>

class NdnProducer {
public:
    virtual void publish(const std::shared_ptr<ndn::Data> &content) = 0;
};
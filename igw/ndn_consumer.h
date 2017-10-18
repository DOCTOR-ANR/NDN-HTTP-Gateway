#pragma once

#include <ndn-cxx/name.hpp>

class NdnConsumer {
public:
    virtual void retrieve(const ndn::Name &name) = 0;
};
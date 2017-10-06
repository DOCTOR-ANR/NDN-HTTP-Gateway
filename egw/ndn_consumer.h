#pragma once

#include <ndn-cxx/interest.hpp>

class NdnConsumer {
public:
    virtual void retrieve(const ndn::Interest &interest) = 0;
};
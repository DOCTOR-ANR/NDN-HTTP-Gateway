#pragma once

#include <ndn-cxx/interest.hpp>

#include <memory>

#include "ndn_source.h"
#include "ndn_content.h"

class NdnSource;

class NdnSink {
protected:
    NdnSource *_ndn_source;

public:
    virtual void fromNdnSource(const std::shared_ptr<NdnContent> &content) = 0;

    void attachNdnSource(NdnSource *ndn_source) {
        _ndn_source = ndn_source;
    }
};
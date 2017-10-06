#pragma once

#include <memory>

#include "ndn_sink.h"
#include "ndn_content.h"

class NdnSink;

class NdnSource {
protected:
    NdnSink *_ndn_sink;

public:
    virtual void fromNdnSink(const std::shared_ptr<NdnContent> &ndn_content) = 0;
    
    void attachNdnSink(NdnSink *ndn_sink) {
        _ndn_sink = ndn_sink;
    }
};
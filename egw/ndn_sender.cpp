#include "ndn_sender.h"

NdnProducerSubModule::NdnProducerSubModule(OffloadedNdnProducer &parent, const ndn::Name &prefix)
        : SubModule(1, parent)
        , _prefix(prefix)
        , _face(_ios) {
    _parent.attachNdnProducer(this);
}

NdnProducerSubModule::~NdnProducerSubModule() {

}

void NdnProducerSubModule::run() {
    _face.setInterestFilter(_prefix, bind(&NdnProducerSubModule::onInterest, this, _2),
                            boost::bind(&NdnProducerSubModule::onRegisterFailed, this));
}

void NdnProducerSubModule::publish(const std::shared_ptr<ndn::Data> &data) {
    _ios.post(boost::bind(&NdnProducerSubModule::publishHandler, this, data));
}

void NdnProducerSubModule::publishHandler(const std::shared_ptr<ndn::Data> &data) {
    _face.put(*data);
}

void NdnProducerSubModule::onInterest(const ndn::Interest &interest) {
    _parent.fromNdnProducer(interest);
}

void NdnProducerSubModule::onRegisterFailed() {
    std::exit(-1);
}
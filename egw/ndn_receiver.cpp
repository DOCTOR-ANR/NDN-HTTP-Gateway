#include "ndn_receiver.h"


NdnConsumerSubModule::NdnConsumerSubModule(OffloadedNdnConsumer &parent) : SubModule(1, parent), _face(_ios) {
    _parent.attachNdnConsumer(this);
}

NdnConsumerSubModule::~NdnConsumerSubModule() {

}

void NdnConsumerSubModule::run() {

}

void NdnConsumerSubModule::retrieve(const ndn::Interest &interest) {
    _ios.post(boost::bind(&NdnConsumerSubModule::retrieveHandler, this, interest));
}

void NdnConsumerSubModule::retrieveHandler(const ndn::Interest &interest) {
    auto content = std::make_shared<NdnContent>();
    content->set_name(interest.getName());
    _face.expressInterest(interest, boost::bind(&NdnConsumerSubModule::onData, this, _1, _2, content),
                          boost::bind(&NdnConsumerSubModule::onNack, this, _1, _2, content),
                          boost::bind(&NdnConsumerSubModule::onTimeout, this, _1, content, 2));
}

void NdnConsumerSubModule::onData(const ndn::Interest &interest, const ndn::Data &data, const std::shared_ptr<NdnContent> &content) {
    content->get_raw_stream()->append_raw_data((const char *) data.getContent().value(), data.getContent().value_size());
    if(data.getFinalBlockId().empty()) {
        ndn::Interest i(ndn::Name(data.getName().getPrefix(-1)).appendSegment(data.getName().get(-1).toSegment() + 1), ndn::time::milliseconds(1000));
        _face.expressInterest(i, boost::bind(&NdnConsumerSubModule::onData, this, _1, _2, content),
                              boost::bind(&NdnConsumerSubModule::onNack, this, _1, _2, content),
                              boost::bind(&NdnConsumerSubModule::onTimeout, this, _1, content, 2));
    } else {
        content->get_raw_stream()->is_completed(true);
    }
    if(!interest.getName().get(-1).isSegment()) {
        _parent.fromNdnConsumer(content);
    }
}

void NdnConsumerSubModule::onTimeout(const ndn::Interest &interest, const std::shared_ptr<NdnContent> &content, int remaining_tries) {
    if(remaining_tries > 0) {
        ndn::Interest i(interest);
        i.setInterestLifetime(i.getInterestLifetime() + ndn::time::milliseconds(667));
        i.refreshNonce();
        _face.expressInterest(i, boost::bind(&NdnConsumerSubModule::onData, this, _1, _2, content),
                              boost::bind(&NdnConsumerSubModule::onNack, this, _1, _2, content),
                              boost::bind(&NdnConsumerSubModule::onTimeout, this, _1, content, remaining_tries - 1));
    } else {
        content->get_raw_stream()->is_aborted(true);
        _parent.fromNdnConsumer(content);
        std::cout << interest.getName() << " unreachable" << std::endl;
    }
}

void NdnConsumerSubModule::onNack(const ndn::Interest &interest, const ndn::lp::Nack &nack, const std::shared_ptr<NdnContent> &content) {
    content->get_raw_stream()->is_aborted(true);
    _parent.fromNdnConsumer(content);
    std::cout << interest.getName() << " " << nack.getReason() << std::endl;
}

#include "ndn_receiver.h"


NdnConsumerSubModule::NdnConsumerSubModule(OffloadedNdnConsumer &parent) : SubModule(1, parent), _face(_ios) {
    _parent.attachNdnConsumer(this);
}

void NdnConsumerSubModule::run() {

}

void NdnConsumerSubModule::retrieve(const ndn::Name &name) {
    _ios.post(boost::bind(&NdnConsumerSubModule::retrieveHandler, this, name));
}

void NdnConsumerSubModule::retrieveHandler(const ndn::Name &name) {
    auto content = std::make_shared<NdnContent>();
    content->setName(name);
    _face.expressInterest(ndn::Interest(name, ndn::time::milliseconds(1000)).setMustBeFresh(true),
                          boost::bind(&NdnConsumerSubModule::onData, this, _1, _2, content, 0),
                          boost::bind(&NdnConsumerSubModule::onNack, this, _1, _2, content),
                          boost::bind(&NdnConsumerSubModule::onTimeout, this, _1, content, 0, 2));
}

void NdnConsumerSubModule::onData(const ndn::Interest &interest, const ndn::Data &data, const std::shared_ptr<NdnContent> &content, size_t seg) {
    if(data.getName().get(-1).isSegment() && data.getName().get(-1).toSegment() != seg) {
        // if here => library problem, only appear for 1st packet
        _face.expressInterest(ndn::Interest(data.getName().getPrefix(-1).appendSegment(seg), ndn::time::milliseconds(1000)).setMustBeFresh(true),
                              boost::bind(&NdnConsumerSubModule::onData, this, _1, _2, content, seg),
                              boost::bind(&NdnConsumerSubModule::onNack, this, _1, _2, content),
                              boost::bind(&NdnConsumerSubModule::onTimeout, this, _1, content, seg, 2));
    } else {
        content->getRawStream()->append_raw_data((const char *) data.getContent().value(), data.getContent().value_size());
        if (data.getFinalBlockId().empty()) {
            _face.expressInterest(ndn::Interest(ndn::Name(data.getName().getPrefix(-1)).appendSegment(seg + 1), ndn::time::milliseconds(1000)).setMustBeFresh(true),
                                  boost::bind(&NdnConsumerSubModule::onData, this, _1, _2, content, seg + 1),
                                  boost::bind(&NdnConsumerSubModule::onNack, this, _1, _2, content),
                                  boost::bind(&NdnConsumerSubModule::onTimeout, this, _1, content, seg + 1, 2));
        } else {
            content->getRawStream()->is_completed(true);
        }
        if (!interest.getName().get(-1).isSegment() || interest.getName().get(-1).toSegment() == 0) {
            _parent.fromNdnConsumer(content);
        }
    }
}

void NdnConsumerSubModule::onTimeout(const ndn::Interest &interest, const std::shared_ptr<NdnContent> &content, size_t seg, size_t remaining_tries) {
    if(remaining_tries > 0) {
        ndn::Interest i(interest);
        i.setInterestLifetime(i.getInterestLifetime() + ndn::time::milliseconds(667));
        i.refreshNonce();
        _face.expressInterest(i, boost::bind(&NdnConsumerSubModule::onData, this, _1, _2, content, seg),
                              boost::bind(&NdnConsumerSubModule::onNack, this, _1, _2, content),
                              boost::bind(&NdnConsumerSubModule::onTimeout, this, _1, content, seg, remaining_tries - 1));
    } else {
        content->getRawStream()->is_aborted(true);
        if (!interest.getName().get(-1).isSegment() || interest.getName().get(-1).toSegment() == 0) {
            _parent.fromNdnConsumer(content);
        }
        std::cout << interest.getName() << " unreachable" << std::endl;
    }
}

void NdnConsumerSubModule::onNack(const ndn::Interest &interest, const ndn::lp::Nack &nack, const std::shared_ptr<NdnContent> &content) {
    content->getRawStream()->is_aborted(true);
    if (!interest.getName().get(-1).isSegment() || interest.getName().get(-1).toSegment() == 0) {
        _parent.fromNdnConsumer(content);
    }
    std::cout << interest.getName() << " " << nack.getReason() << std::endl;
}

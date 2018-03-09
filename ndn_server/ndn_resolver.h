/*
Copyright (C) 2015-2018  Xavier MARCHAL
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <ndn-cxx/name.hpp>

#include <mutex>
#include <unordered_map>

#include "global.h"
#include "module.h"
#include "offloaded_ndn_consumer.h"
#include "ndn_receiver.h"
#include "offloaded_ndn_producer.h"
#include "ndn_sender.h"
#include "ndn_source.h"
#include "ndn_content.h"

class NdnResolver : public Module, public OffloadedNdnConsumer, public OffloadedNdnProducer, public NdnSource  {
private:
    ndn::KeyChain _keychain;

    boost::asio::deadline_timer _purge_timer;
    std::mutex _map_mutex;
    std::unordered_map<std::string, std::shared_ptr<NdnContent>> _contents;

public:
    explicit NdnResolver(size_t concurrency);

    ~NdnResolver() override = default;

    void run() override;

    void fromNdnProducer(const ndn::Interest &interest) override;

    void fromNdnConsumer(const std::shared_ptr<NdnContent> &content) override;

    void fromNdnSink(const std::shared_ptr<NdnContent> &content) override;

private:
    void fromNdnProducerHandler(const ndn::Interest &interest);

    void fromNdnConsumerHandler(const std::shared_ptr<NdnContent> &content);

    void fromNdnSinkHandler(const std::shared_ptr<NdnContent> &content);

    void checkContent(const ndn::Interest &interest, const std::shared_ptr<boost::asio::deadline_timer> &timer, size_t remaining_tries);

    void generate_data(const std::shared_ptr<NdnContent> &content, const std::shared_ptr<boost::asio::deadline_timer> &timer, uint64_t segment = 0);

    void purge_old_data();
};
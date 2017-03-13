/*
Copyright (C) 2015-2017  Xavier MARCHAL
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
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/data.hpp>

#include <memory>
#include <thread>
#include <map>
#include <chrono>

#include <libcuckoo/cuckoohash_map.hh>

#include "global.h"
#include "module.h"
#include "ndn_sink.h"
#include "ndn_message.h"

class NdnServer : public Module, public NdnSink {
private:
    static boost::posix_time::seconds DEFAULT_WAIT_PURGE;

    class Content {
        std::chrono::steady_clock::time_point _last_access;

        std::map<uint64_t, std::shared_ptr<ndn::Data>> _datas;

    public:
        Content();

        ~Content();

        const std::chrono::steady_clock::time_point& get_last_access() const;

        void add_data(uint64_t segment, std::shared_ptr<ndn::Data> data);

        std::map<uint64_t, std::shared_ptr<ndn::Data>>::iterator find_data(uint64_t segment);

        std::map<uint64_t, std::shared_ptr<ndn::Data>>::iterator end();
    };

    ndn::Name _prefix;
    ndn::Face _face;
    ndn::KeyChain _keychain;

    std::thread _face_thread;

    boost::asio::deadline_timer _purge_timer;
    cuckoohash_map<ndn::Name, std::shared_ptr<Content>> _contents;

    NdnSink *_ndn_sink;

public:
    NdnServer(ndn::Name prefix, size_t concurrency = 1);

    ~NdnServer();

    virtual void run() override;

    void start_face();

    virtual void solve(std::shared_ptr<NdnMessage> ndn_message) override;

    void solve_handler(const std::shared_ptr<NdnMessage> &ndn_message);

    void on_interest(const ndn::Interest &interest);

    void on_interest_handler(const ndn::Interest &interest);

    void reply_notify(const ndn::Interest &interest);

    void reply_content(const ndn::Interest &interest, const std::shared_ptr<boost::asio::deadline_timer> &timer, int left);

    void generate_data(const std::shared_ptr<NdnMessage> &ndn_message, const std::shared_ptr<boost::asio::deadline_timer> &timer, uint64_t segment);

    void purge_old_data();

    void on_register_failed();

    void attach_ndn_sinks(NdnSink *ndn_sink);
};

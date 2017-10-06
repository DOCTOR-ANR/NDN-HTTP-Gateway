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

#include <ndn-cxx/name.hpp>

#include <iostream>
#include <csignal>

#include "ndn_resolver.h"
#include "ndn_http_interpreter.h"
#include "http_client.h"

static bool stop = false;

static void signal_handler(int signum) {
    stop = true;
}

int main(int argc, char *argv[]) {
    ndn::Name prefix("/http");

    for(int i = 1; i < argc; ++i){
        switch (argv[i][1]){
            case 'n':
                prefix = argv[++i];
                break;
            case 'h':
            default:
                std::cout << argv[0] << " [-n NDN_NAME]" << std::endl;
                return -1;
        }
    }

    NdnResolver ndn_resolver(6);
    NdnConsumerSubModule ndn_receiver(ndn_resolver);
    NdnProducerSubModule ndn_sender(ndn_resolver, prefix);
    NdnHttpInterpreter interpreter(8);
    HttpClient http_client(12);

    ndn_resolver.attachNdnSink(&interpreter);
    interpreter.attachNdnSource(&ndn_resolver);
    interpreter.attachHttpSink(&http_client);
    http_client.attach_http_source(&interpreter);

    ndn_resolver.start();
    ndn_receiver.start();
    ndn_sender.start();
    interpreter.start();
    http_client.start();

    std::cout << "HTTP/NDN egress gateway uses NDN prefix " << prefix << std::endl;
    signal(SIGINT, signal_handler);

    do {
        sleep(15);
    }while(!stop);

    http_client.stop();
    interpreter.stop();
    ndn_sender.stop();
    ndn_receiver.stop();
    ndn_resolver.stop();


    return 0;
}
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

#include <ndn-cxx/name.hpp>

#include <iostream>
#include <csignal>

#include "http_server.h"
#include "http_ndn_interpreter.h"
#include "ndn_resolver.h"
#include "ndn_receiver.h"
#include "ndn_sender.h"

static bool stop = false;

static void signal_handler(int signum) {
    stop = true;
}

int main(int argc, char *argv[]) {
    unsigned short port = 8080;
    ndn::Name prefix("/http/iGW/");

    for(int i = 1; i < argc; ++i){
        switch (argv[i][1]){
            case 'p':
                port = (unsigned short) std::stoi(argv[++i]);
                break;
            case 'n':
                prefix = argv[++i];
                break;
            case 'h':
            default:
                std::cout << argv[0] << " [-p PORT_NUMBER] [-n NDN_NAME]" << std::endl;
                return -1;
        }
    }

    std::cout << "HTTP/NDN ingress gateway v1.1-2" << std::endl;

    HttpServer http_server(port, 4);
    HttpNdnInterpreter interpreter(2);
    NdnResolver ndn_resolver(prefix, 4);
    NdnConsumerSubModule ndn_receiver(ndn_resolver);
    NdnProducerSubModule ndn_sender(ndn_resolver, prefix);

    http_server.attachHttpSink(&interpreter);
    interpreter.attachHttpSource(&http_server);
    interpreter.attachNdnSink(&ndn_resolver);
    ndn_resolver.attachNdnSource(&interpreter);

    http_server.start();
    interpreter.start();
    ndn_resolver.start();
    ndn_receiver.start();
    ndn_sender.start();

    std::cout << "listens on 0.0.0.0:" << port << " and registered as " << prefix << std::endl;
    signal(SIGINT, signal_handler);

    do {
        sleep(15);
    }while(!stop);

    ndn_receiver.stop();
    ndn_sender.stop();
    ndn_resolver.stop();
    http_server.stop();
    interpreter.stop();

    return 0;
}
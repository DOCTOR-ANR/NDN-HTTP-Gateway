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

#include "http_server.h"
#include "http_ndn_interpreter.h"
#include "ndn_client.h"
#include "ndn_server.h"

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

    HttpServer http_server(port, 8);
    HttpNdnInterpreter interpreter(prefix, 4);
    NdnClient ndn_client;
    NdnServer ndn_server(prefix, 8);

    http_server.attach_http_sink(&interpreter);
    interpreter.attach_ndn_sinks(&ndn_client, &ndn_server);

    http_server.start();
    interpreter.start();
    ndn_client.start();
    ndn_server.start();

    std::cerr << "HTTP/NDN ingress gateway listens on 0.0.0.0:" << port << " and uses NDN prefix " << prefix << std::endl;
    signal(SIGINT, signal_handler);

    do {
        sleep(15);
    }while(!stop);

    http_server.stop();
    interpreter.stop();
    ndn_client.stop();
    ndn_server.stop();

    return 0;
}
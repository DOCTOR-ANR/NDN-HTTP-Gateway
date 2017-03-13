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

#include "ndn_server.h"
#include "ndn_client.h"
#include "http_client.h"
#include "ndn_http_interpreter.h"

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

    NdnServer ndn_server(prefix, 8);
    NdnClient ndn_client;
    NdnHttpInterpreter interpreter(4);
    HttpClient http_client(8);

    ndn_server.attach_ndn_sinks(&interpreter);
    interpreter.attach_ndn_sinks(&ndn_client, &ndn_server);
    interpreter.attach_http_sink(&http_client);

    ndn_server.start();
    ndn_client.start();
    interpreter.start();
    http_client.start();

    std::cerr << "HTTP/NDN egress gateway uses NDN prefix /http/" << std::endl;
    signal(SIGINT, signal_handler);

    do {
        sleep(15);
    }while(!stop);

    ndn_server.stop();
    ndn_client.stop();
    interpreter.stop();
    http_client.stop();

    return 0;
}
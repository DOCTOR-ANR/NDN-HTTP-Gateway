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

#include <boost/asio.hpp>

namespace global {
    const boost::posix_time::milliseconds DEFAULT_WAIT_REDO(5);
    const boost::posix_time::seconds DEFAULT_WAIT_PURGE(60);
    const boost::posix_time::seconds DEFAULT_TIMEOUT_CONNECT(2);
    const boost::posix_time::seconds DEFAULT_TIMEOUT_READ_HTTP_HEADER(5);
    const boost::posix_time::seconds DEFAULT_TIMEOUT_READ_HTTP_BODY(2);
    const uint32_t DEFAULT_BUFFER_SIZE = 4096;
};

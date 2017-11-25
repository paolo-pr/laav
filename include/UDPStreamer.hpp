/* 
 * Created (10/11/2017) by Paolo-Pr.
 * This file is part of Live Asynchronous Audio Video Library.
 *
 * Live Asynchronous Audio Video Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Live Asynchronous Audio Video Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Live Asynchronous Audio Video Library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef UDPSTREAMER_HPP_INCLUDED
#define UDPSTREAMER_HPP_INCLUDED

#include "Streamer.hpp"
#ifdef LINUX
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/types.h>
    #include <sys/socket.h>
#endif

namespace laav
{

template <typename Container>
class UDPStreamer : public Streamer<Container>
{

protected:

    UDPStreamer(std::string address, unsigned int port):
        Streamer<Container>(address, port),
#ifdef LINUX        
        mSock(socket(PF_INET, SOCK_DGRAM, 0)),
        mSlen(sizeof mSockAddr)
#endif
    {
#ifdef LINUX        
        if (mSock == -1)
            printAndThrowUnrecoverableError
            ("UDPStreamer(std::string address, unsigned int port): socket() failed");

        mSockAddr.sin_family = AF_INET;
        mSockAddr.sin_port = htons(port);
        mSockAddr.sin_addr.s_addr = inet_addr(address.c_str());
        memset(mSockAddr.sin_zero, '\0', sizeof mSockAddr.sin_zero);
#endif
        this->mStatus = MEDIA_READY;
    }

    ~UDPStreamer()
    {
        close(mSock);
    }

    struct sockaddr_in mSockAddr;
    int mSock;
    socklen_t mSlen;

};

}

#endif // UDPSTREAMER_HPP_INCLUDED

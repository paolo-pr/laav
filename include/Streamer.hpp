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

#ifndef STREAMER_HPP_INCLUDED
#define STREAMER_HPP_INCLUDED

#include "FFMPEGAudioVideoMuxer.hpp"

namespace laav
{

template <typename Container>
class Streamer
{

public:

    enum MediaStatus status() const
    {
        return mStatus;
    }

    int getErrno() const
    {
        return mErrno;
    }

protected:

    Streamer(std::string address, unsigned int port):
        mAddress(address),
        mPort(port),
        mStatus(MEDIA_NOT_READY),
        mErrno(0)
    {       
    }

    ~Streamer()
    {
    }

    std::string mAddress;
    unsigned int mPort;
    enum MediaStatus mStatus;
    int mErrno;

};

}

#endif // STREAMER_HPP_INCLUDED

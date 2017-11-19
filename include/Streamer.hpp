/*
 * Created (10/11/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
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

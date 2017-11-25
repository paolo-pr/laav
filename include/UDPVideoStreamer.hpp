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

#ifndef UDPVIDEOSTREAMER_HPP_INCLUDED
#define UDPVIDEOSTREAMER_HPP_INCLUDED

#include "UDPStreamer.hpp"

namespace laav
{

template <typename Container,
          typename VideoCodecOrFormat, unsigned int width, unsigned int height>
class UDPVideoStreamer : public UDPStreamer<Container>
{

public:

    UDPVideoStreamer(std::string address, unsigned int port) :
        UDPStreamer<Container>(address, port),
        mVideoMuxer(true)
    {
    }

    void takeStreamableFrame(const VideoFrame<VideoCodecOrFormat, width, height>& videoFrameToStream)
    {
        if (this->mStatus == MEDIA_READY)
            mVideoMuxer.takeMuxableFrame(videoFrameToStream);
    }

    void streamMuxedData()
    {   
        const std::vector<MuxedVideoData<Container, VideoCodecOrFormat,
                                         width, height> >&                                              
        chunksToStream = mVideoMuxer.muxedVideoChunks();
        unsigned int n;
        std::vector<unsigned char> dataToStream;
        
        for (n = 0; n < mVideoMuxer.muxedVideoChunksOffset(); n++)
        {
            dataToStream.insert(dataToStream.end(), chunksToStream[n].data(), 
                                chunksToStream[n].data()+chunksToStream[n].size());
        }
        if (sendto(this->mSock, dataToStream.data(), dataToStream.size(), 0, 
            (const sockaddr* )&this->mSockAddr, this->mSlen)==-1)
            printAndThrowUnrecoverableError("UDPVideoStreamer::streamMuxedData(); sendto() error");
    }

private:

    FFMPEGVideoMuxer<Container,
                     VideoCodecOrFormat, width, height> mVideoMuxer;

};

}

#endif // UDPVIDEOSTREAMER_HPP_INCLUDED

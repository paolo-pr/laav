/*
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
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

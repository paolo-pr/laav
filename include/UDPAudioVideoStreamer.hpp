/*
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef UDPAUDIOVIDEOSTREAMER_HPP_INCLUDED
#define UDPAUDIOVIDEOSTREAMER_HPP_INCLUDED

#include "UDPStreamer.hpp"

namespace laav
{

template <typename Container,
          typename VideoCodecOrFormat, unsigned int width, unsigned int height,
          typename AudioCodecOrFormat, unsigned int audioSampleRate, enum AudioChannels audioChannels>
class UDPAudioVideoStreamer : public UDPStreamer<Container>
{

public:

    UDPAudioVideoStreamer(std::string address, unsigned int port) :
        UDPStreamer<Container>(address, port),
        mAudioVideoMuxer(true)
    {
    }

    void takeStreamableFrame(const VideoFrame<VideoCodecOrFormat, width, height>& videoFrameToStream)
    {
        if (this->mStatus == MEDIA_READY)
            mAudioVideoMuxer.takeMuxableFrame(videoFrameToStream);
    }

    void takeStreamableFrame(const AudioFrame<AudioCodecOrFormat, audioSampleRate,
                                              audioChannels>& audioFrameToStream)
    {
        mAudioVideoMuxer.takeMuxableFrame(audioFrameToStream);
    }

    void streamMuxedData()
    {   
        const std::vector<MuxedAudioVideoData<Container, VideoCodecOrFormat,
                                                width, height,
                                                AudioCodecOrFormat, audioSampleRate,
                                                audioChannels> >&                                              
        chunksToStream = mAudioVideoMuxer.muxedAudioVideoChunks();
        unsigned int n;
        
        std::vector<unsigned char> dataToStream;
        
        for (n = 0; n < mAudioVideoMuxer.muxedAudioVideoChunksOffset(); n++)
        {
            dataToStream.insert(dataToStream.end(), chunksToStream[n].data(), 
                                chunksToStream[n].data()+chunksToStream[n].size());
        }
        if (sendto(this->mSock, dataToStream.data(), dataToStream.size(), 0, 
            (const sockaddr* )&this->mSockAddr, this->mSlen)==-1)
            printAndThrowUnrecoverableError("UDPAudioVideoStreamer::streamMuxedData(); sendto() error");
    }

private:

    FFMPEGAudioVideoMuxer<Container,
                          VideoCodecOrFormat, width, height,
                          AudioCodecOrFormat, audioSampleRate, audioChannels> mAudioVideoMuxer;

};

}

#endif // UDPAUDIOVIDEOSTREAMER_HPP_INCLUDED

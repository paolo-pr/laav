/* 
 * Created (25/04/2017) by Paolo-Pr.
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

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
          typename VideoCodecOrFormat, typename width, typename height,
          typename AudioCodecOrFormat, typename audioSampleRate, typename audioChannels>
class UDPAudioVideoStreamer : public UDPStreamer<Container>
{

public:

    UDPAudioVideoStreamer(std::string address, unsigned int port, const std::string& id = "") :
        UDPStreamer<Container>(address, port, id),
        mAudioVideoMuxer(id+"EmbeddedMux")
    {
        this->logStreamAddr(constantToString<Container>()+"-"+constantToString<AudioCodecOrFormat>()
        +"-"+constantToString<VideoCodecOrFormat>());   
    }

    /*!
     *  \exception MediumException
     */
    void takeStreamableFrame(const VideoFrame<VideoCodecOrFormat, width, height>& videoFrameToStream)
    {
        if (videoFrameToStream.isEmpty())
            throw MediumException(INPUT_EMPTY);         
        
        Medium::mInputVideoFrameFactoryId = videoFrameToStream.mFactoryId; 
        Medium::mDistanceFromInputVideoFrameFactory = videoFrameToStream.mDistanceFromFactory + 1;  

        if (Medium::mInputStatus ==  PAUSED)
            throw MediumException(MEDIUM_PAUSED);  
        
        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);        
        
        mAudioVideoMuxer.mux(videoFrameToStream);        
    }

    /*!
     *  \exception MediumException
     */
    void takeStreamableFrame(const AudioFrame<AudioCodecOrFormat, audioSampleRate,
                                              audioChannels>& audioFrameToStream)
    {
        if (audioFrameToStream.isEmpty())
            throw MediumException(INPUT_EMPTY);        
        
        Medium::mInputAudioFrameFactoryId = audioFrameToStream.mFactoryId; 
        Medium::mDistanceFromInputAudioFrameFactory = audioFrameToStream.mDistanceFromFactory + 1;
        
        if (Medium::mInputStatus ==  PAUSED)
            throw MediumException(MEDIUM_PAUSED); 
        
        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY); 
        
        mAudioVideoMuxer.mux(audioFrameToStream);        
    }

    /*!
     *  \exception MediumException
     */    
    void streamMuxedData()
    {   
        if (Medium::mInputStatus ==  PAUSED)
            throw MediumException(MEDIUM_PAUSED);
        
        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);        
        
        const std::vector<MuxedAudioVideoData<Container, VideoCodecOrFormat,
                                                width, height,
                                                AudioCodecOrFormat, audioSampleRate,
                                                audioChannels> >&                                              
        chunksToStream = mAudioVideoMuxer.muxedAudioVideoChunks();
        Medium::mOutputStatus = READY;        
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
        Medium::mOutputStatus = READY;         
    }

private:

    FFMPEGAudioVideoMuxer<Container,
                          VideoCodecOrFormat, width, height,
                          AudioCodecOrFormat, audioSampleRate, audioChannels> mAudioVideoMuxer;

};

}

#endif // UDPAUDIOVIDEOSTREAMER_HPP_INCLUDED

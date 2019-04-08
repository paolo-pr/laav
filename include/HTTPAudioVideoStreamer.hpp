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

#ifndef HTTPAUDIOVIDEOSTREAMER_HPP_INCLUDED
#define HTTPAUDIOVIDEOSTREAMER_HPP_INCLUDED

#include "HTTPStreamer.hpp"

namespace laav
{

template <typename Container,
          typename VideoCodecOrFormat, typename width, typename height,
          typename AudioCodecOrFormat, typename audioSampleRate, typename audioChannels>
class HTTPAudioVideoStreamer : public HTTPStreamer<Container>
{

public:

    HTTPAudioVideoStreamer(SharedEventsCatcher eventsCatcher, std::string address, 
                           unsigned int port, const std::string& id = "") :
        HTTPStreamer<Container>(eventsCatcher, address, port, id),
        mAudioVideoMuxer("")
    {
        this->logStreamAddr(constantToString<Container>()+"-"+constantToString<AudioCodecOrFormat>()
        +"-"+constantToString<VideoCodecOrFormat>());        
    }

    template <typename RawVideoFrameFormat>
    HTTPAudioVideoStreamer(SharedEventsCatcher eventsCatcher, std::string address, 
                           unsigned int port, 
                           const FFMPEGVideoEncoder<RawVideoFrameFormat, VideoCodecOrFormat, width, height>& vEnc,
                           const std::string& id = "") :
        HTTPStreamer<Container>(eventsCatcher, address, port, id),
        mAudioVideoMuxer(vEnc, "")
    {
        this->logStreamAddr(constantToString<Container>()+"-"+constantToString<AudioCodecOrFormat>()
        +"-"+constantToString<VideoCodecOrFormat>());         
    }    
    
    template <typename PCMSoundFormat>
    HTTPAudioVideoStreamer(SharedEventsCatcher eventsCatcher, std::string address, 
                           unsigned int port, 
                           const FFMPEGAudioEncoder<PCMSoundFormat, AudioCodecOrFormat, audioSampleRate, audioChannels>& aEnc,
                           const std::string& id = "") :
        HTTPStreamer<Container>(eventsCatcher, address, port, id),
        mAudioVideoMuxer(aEnc, "")
    {
        this->logStreamAddr(constantToString<Container>()+"-"+constantToString<AudioCodecOrFormat>()
        +"-"+constantToString<VideoCodecOrFormat>());         
    }
    
    template <typename PCMSoundFormat, typename RawVideoFrameFormat>
    HTTPAudioVideoStreamer(SharedEventsCatcher eventsCatcher, std::string address, 
                           unsigned int port, 
                           const FFMPEGAudioEncoder<PCMSoundFormat, AudioCodecOrFormat, audioSampleRate, audioChannels>& aEnc,
                           const FFMPEGVideoEncoder<RawVideoFrameFormat, VideoCodecOrFormat, width, height>& vEnc,
                           const std::string& id = "") :
        HTTPStreamer<Container>(eventsCatcher, address, port, id),
        mAudioVideoMuxer(aEnc, vEnc, "")
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
        
        using Iter3 = std::map<struct evhttp_connection*, struct evhttp_request* >::iterator;
        for (Iter3 it = this->mClientConnectionsAndRequests.begin();
                it != this->mClientConnectionsAndRequests.end(); ++it)
        {
            if (videoFrameToStream.mLibAVFlags & AV_PKT_FLAG_KEY)
            {
                this->mGotKeyFrameForRequest[it->second] = true;
                if (this->mSkipFramesForRequest[it->second] == -1 && mAudioVideoMuxer.latencyCounter() != -1)
                    this->mSkipFramesForRequest[it->second] = mAudioVideoMuxer.latencyCounter();
            }
        }        
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
        
        if (this->mClientConnectionsAndRequests.size() == 0)
            throw MediumException(INPUT_NOT_READY);        
        
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
        
        if (this->mClientConnectionsAndRequests.size() != 0)
        {
            //try
            {
                using Iter3 = std::map<struct evhttp_connection*, struct evhttp_request* >::iterator;
                for (Iter3 it = this->mClientConnectionsAndRequests.begin();
                     it != this->mClientConnectionsAndRequests.end(); ++it)
                {

                    const std::vector<MuxedAudioVideoData<Container, VideoCodecOrFormat,
                                                          width, height,
                                                          AudioCodecOrFormat, audioSampleRate,
                                                          audioChannels> >&
                    chunksToStream = mAudioVideoMuxer.muxedAudioVideoChunks();
                    
                    bool gotKeyFrame = this->mGotKeyFrameForRequest[it->second];
                    int& framesToSkip = this->mSkipFramesForRequest[it->second];
                    if (! (gotKeyFrame && framesToSkip != -1) )
                        continue;
                    else if (framesToSkip > 0)
                    {
                        framesToSkip--;
                        continue;
                    }                    
                    
                    Medium::mOutputStatus = READY;
                    unsigned int n;
                    struct evbuffer* buf = evbuffer_new();
                    if (this->mWrittenHeaderFlagAndRequests[it->second] == false)
                    {
                        if (!std::is_same<Container, MPEGTS>::value)
                            evbuffer_add(buf, mAudioVideoMuxer.header().c_str(),
                                         mAudioVideoMuxer.header().size());

                        this->mWrittenHeaderFlagAndRequests[it->second] = true;
                    }
                    for (n = 0; n < mAudioVideoMuxer.muxedAudioVideoChunksOffset(); n++)
                    {
                        if (buf == NULL)
                            printAndThrowUnrecoverableError("buf == NULL");
                        evbuffer_add(buf, chunksToStream[n].data(), chunksToStream[n].size());
                    }
                    evhttp_send_reply_chunk(it->second, buf);
                    evbuffer_free(buf);
                    Medium::mOutputStatus = READY;                     
                }
            }/*
            catch (const MediumException& mediumException)
            {
                // DO next step
            }*/
        }
        else
            throw MediumException(OUTPUT_NOT_READY);
    }

private:


    FFMPEGAudioVideoMuxer<Container,
                          VideoCodecOrFormat, width, height,
                          AudioCodecOrFormat, audioSampleRate, audioChannels> mAudioVideoMuxer;

};

}

#endif // HTTPAUDIOVIDEOSTREAMER_HPP_INCLUDED

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

#ifndef VIDEOENCODER_HPP_INCLUDED
#define VIDEOENCODER_HPP_INCLUDED

namespace laav
{
    
template <typename EncodedVideoFrameCodec,
          typename width,
          typename height>
class VideoFrameHolder;

template <typename Container,
          typename VideoCodecOrFormat,
          typename width,
          typename height>
class HTTPVideoStreamer;    

template <typename Container,
          typename VideoCodecOrFormat,
          typename width,
          typename height>
class FFMPEGVideoMuxer;

template <typename Container,
          typename VideoCodecOrFormat,
          typename width,
          typename height>
class UDPVideoStreamer;

template <typename Container,
          typename VideoCodecOrFormat,
          typename width,
          typename height,
          typename AudioCodec,
          typename audioSampleRate,
          typename audioChannels>
class HTTPAudioVideoStreamer;

template <typename Container,
          typename VideoCodecOrFormat,
          typename width,
          typename height,
          typename AudioCodec,
          typename audioSampleRate,
          typename audioChannels>
class FFMPEGAudioVideoMuxer;

template <typename Container,
          typename VideoCodecOrFormat,
          typename width,
          typename height,
          typename AudioCodec,
          typename audioSampleRate,
          typename audioChannels>
class UDPAudioVideoStreamer;

template <typename RawVideoFrameFormat,
          typename EncodedVideoFrameCodec,
          typename width,
          typename height>
class VideoEncoder : public Medium
{

public:

    VideoEncoder(const std::string& id = ""):
        Medium(id),
        mEncodingStartTime(0),
        mFillingEncodedVideoFrameBuffer(true),
        mEncodedVideoFrameBufferOffset(0),
        mNeedsToBuffer(false)
    {
        unsigned int i;
        for (i = 0; i < encodedVideoFrameBufferSize; i++)
        {
            mEncodedVideoFrameBuffer.push_back(VideoFrame<EncodedVideoFrameCodec, width, height>());
        }
        Medium::mInputStatus = READY;        
    }

    VideoFrameHolder<EncodedVideoFrameCodec, width, height>&
    operator >>
    (VideoFrameHolder<EncodedVideoFrameCodec, width, height>& videoFrameHolder)
    {
        Medium& m = videoFrameHolder;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);
        setStatusInPipe(m, READY); 
        
        try
        {
            videoFrameHolder.hold(lastEncodedFrame());          
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoFrameHolder;
    }

    template <typename Container, typename AudioCodec,
              typename audioSampleRate, typename audioChannels>
    FFMPEGAudioVideoMuxer<Container,
                          EncodedVideoFrameCodec, width, height,
                          AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioVideoMuxer<Container,
                           EncodedVideoFrameCodec, width, height,
                           AudioCodec, audioSampleRate, audioChannels>& audioVideoMuxer)
    {
        Medium& m = audioVideoMuxer;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);           
        setStatusInPipe(m, READY); 
        
        try
        {
            audioVideoMuxer.mux(lastEncodedFrame());            
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return audioVideoMuxer;
    }

    template <typename Container>
    FFMPEGVideoMuxer<Container, EncodedVideoFrameCodec, width, height>&
    operator >>
    (FFMPEGVideoMuxer<Container, EncodedVideoFrameCodec, width, height>& videoMuxer)
    {
        Medium& m = videoMuxer;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);
        setStatusInPipe(m, READY); 
    
        try
        {
            videoMuxer.mux(lastEncodedFrame());       
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoMuxer;
    }

    template <typename Container, typename AudioCodec,
              typename audioSampleRate, typename audioChannels>
    HTTPAudioVideoStreamer<Container,
                           EncodedVideoFrameCodec, width, height,
                           AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (HTTPAudioVideoStreamer<Container,
                            EncodedVideoFrameCodec, width, height,
                            AudioCodec, audioSampleRate, audioChannels>& httpAudioVideoStreamer)
    {
        Medium& m = httpAudioVideoStreamer;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);
        setStatusInPipe(m, READY); 
        
        try
        {
            httpAudioVideoStreamer.takeStreamableFrame(lastEncodedFrame());
            httpAudioVideoStreamer.streamMuxedData();            
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }

        return httpAudioVideoStreamer;
    }

    template <typename Container>
    HTTPVideoStreamer<Container, EncodedVideoFrameCodec, width, height>&
    operator >>
    (HTTPVideoStreamer<Container, EncodedVideoFrameCodec, width, height>& httpVideoStreamer)
    {
        Medium& m = httpVideoStreamer;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);
        setStatusInPipe(m, READY); 
        
        try
        {
            httpVideoStreamer.takeStreamableFrame(lastEncodedFrame());
            httpVideoStreamer.streamMuxedData();           
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }

        return httpVideoStreamer;
    }

    template <typename Container, typename AudioCodec,
              typename audioSampleRate, typename audioChannels>
    UDPAudioVideoStreamer<Container,
                          EncodedVideoFrameCodec, width, height,
                          AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (UDPAudioVideoStreamer<Container,
                           EncodedVideoFrameCodec, width, height,
                           AudioCodec, audioSampleRate, audioChannels>& udpAudioVideoStreamer)
    {
        Medium& m = udpAudioVideoStreamer;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);        
        setStatusInPipe(m, READY); 
        
        try
        {
            udpAudioVideoStreamer.takeStreamableFrame(lastEncodedFrame());
            udpAudioVideoStreamer.streamMuxedData();           
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);
        }

        return udpAudioVideoStreamer;
    }

    template <typename Container>
    UDPVideoStreamer<Container, EncodedVideoFrameCodec, width, height>&
    operator >>
    (UDPVideoStreamer<Container, EncodedVideoFrameCodec, width, height>& udpVideoStreamer)
    {
        Medium& m = udpVideoStreamer;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);         
        setStatusInPipe(m, READY); 
        
        try
        {
            udpVideoStreamer.takeStreamableFrame(lastEncodedFrame());
            udpVideoStreamer.streamMuxedData();            
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);
        }

        return udpVideoStreamer;
    }
    
    /*!
     *  \exception MediumException
     */
    VideoFrame<EncodedVideoFrameCodec, width, height>& encodedFrame(unsigned int bufferOffset)
    {
        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);
        
        if (Medium::mInputStatus ==  PAUSED)
            throw MediumException(MEDIUM_PAUSED);        
        
        if (mFillingEncodedVideoFrameBuffer &&
            (bufferOffset % mEncodedVideoFrameBuffer.size() >= mEncodedVideoFrameBufferOffset) )
            throw MediumException(MEDIUM_BUFFERING);
        
        if (mNeedsToBuffer)
            throw MediumException(MEDIUM_BUFFERING);
        
        else
        {
            return mEncodedVideoFrameBuffer[bufferOffset % mEncodedVideoFrameBuffer.size()];
        }
    }

    /*!
     *  \exception MediumException
     */
    VideoFrame<EncodedVideoFrameCodec, width, height>& lastEncodedFrame()
    {
        unsigned int pos = (mEncodedVideoFrameBuffer.size() +
        mEncodedVideoFrameBufferOffset - 1) % mEncodedVideoFrameBuffer.size();

        return encodedFrame(pos);
    }

    unsigned int encodedFrameBufferIndex() const
    {
        return mEncodedVideoFrameBufferOffset;
    }

    unsigned int encodedFrameBufferSize() const
    {
        return mEncodedVideoFrameBuffer.size();
    }

    virtual void encode(const VideoFrame<RawVideoFrameFormat, width, height>& rawVideoFrame) = 0;

protected:

    virtual void beforeEncodeCallback(){}
    
    virtual void afterEncodeCallback(){}    
    
    int64_t mEncodingStartTime;
    bool mFillingEncodedVideoFrameBuffer;
    //TODO: should it be mutable, with const getting functions?
    std::vector<VideoFrame<EncodedVideoFrameCodec, width, height> > mEncodedVideoFrameBuffer;
    unsigned int mEncodedVideoFrameBufferOffset;
    bool mNeedsToBuffer;
};

}

#endif // VIDEOENCODER_HPP_INCLUDED

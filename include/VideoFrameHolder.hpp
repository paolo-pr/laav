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

#ifndef VIDEOFRAMEHOLDER_HPP_INCLUDED
#define VIDEOFRAMEHOLDER_HPP_INCLUDED

namespace laav
{

template <typename CodecOrFormat, typename width, typename height>
class VideoFrameHolder : public Medium
{
    
public:

    VideoFrameHolder(const std::string& id = "") :
        Medium(id)
    {
        Medium::mInputStatus = READY;        
    }
    
    /*!
     *  \exception MediumException
     */    
    void hold(const VideoFrame<CodecOrFormat, width, height>& videoFrame)
    {        
        Medium::mOutputStatus = NOT_READY;         
        
        if (videoFrame.isEmpty())
            throw MediumException(INPUT_EMPTY);  
        
        mInputVideoFrameFactoryId = videoFrame.mFactoryId;
        Medium::mDistanceFromInputVideoFrameFactory = videoFrame.mDistanceFromFactory + 1;        

        if (Medium::mInputStatus ==  PAUSED)
            throw MediumException(MEDIUM_PAUSED);        
        
        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);  
        
        Medium::mOutputStatus = READY;
        mVideoFrame = videoFrame;         
        
    }

    /*!
     *  \exception MediumException
     */
    VideoFrame<CodecOrFormat, width, height>& get()
    {
        if (mVideoFrame.isEmpty())
            throw MediumException(OUTPUT_NOT_READY);        
        
        mVideoFrame.mDistanceFromFactory = Medium::mDistanceFromInputVideoFrameFactory;
                        
        if (Medium::mInputStatus ==  PAUSED)
            throw MediumException(MEDIUM_PAUSED);

        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);         
        
        return mVideoFrame;
    }

    template <typename ConvertedVideoFrameFormat, typename outputWidth, typename outputHeigth>
    FFMPEGVideoConverter<CodecOrFormat, width, height,
                         ConvertedVideoFrameFormat, outputWidth, outputHeigth>&
    operator >>
    (FFMPEGVideoConverter<CodecOrFormat, width, height,
                          ConvertedVideoFrameFormat, outputWidth, outputHeigth>& videoConverter)
    {
        Medium& m = videoConverter;
        setDistanceFromInputVideoFrameFactory(m, mDistanceFromInputVideoFrameFactory + 1);        
        setInputVideoFrameFactoryId(m, mInputVideoFrameFactoryId);
        setStatusInPipe(m, READY);          
        
        try
        {
            videoConverter.convert(get());          
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoConverter;
    }

    template <typename Container, typename AudioCodec,
              typename audioSampleRate, typename audioChannels>
    FFMPEGAudioVideoMuxer<Container,
                          CodecOrFormat, width, height,
                          AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioVideoMuxer<Container,
                           CodecOrFormat, width, height,
                           AudioCodec, audioSampleRate, audioChannels>& audioVideoMuxer)
    {

        Medium& m = audioVideoMuxer;
        setDistanceFromInputVideoFrameFactory(m, mDistanceFromInputVideoFrameFactory + 1);        
        setInputVideoFrameFactoryId(m, mInputVideoFrameFactoryId);           
        setStatusInPipe(m, READY);  
        
        try
        {
            audioVideoMuxer.mux(get());          
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return audioVideoMuxer;
    }

    template <typename Container>
    FFMPEGVideoMuxer<Container, CodecOrFormat, width, height>&
    operator >>
    (FFMPEGVideoMuxer<Container, CodecOrFormat, width, height>& videoMuxer)
    {
        Medium& m = videoMuxer;
        setDistanceFromInputVideoFrameFactory(m, mDistanceFromInputVideoFrameFactory + 1);        
        setInputVideoFrameFactoryId(m, mInputVideoFrameFactoryId);           
        setStatusInPipe(m, READY);          
        
        try
        {
            videoMuxer.mux(get());          
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoMuxer;
    }

    template <typename EncodedVideoFrameCodec>
    VideoEncoder<CodecOrFormat, EncodedVideoFrameCodec, width, height>&
    operator >>
    (VideoEncoder<CodecOrFormat, EncodedVideoFrameCodec, width, height>& videoEncoder)
    {
        Medium& m = videoEncoder;
        setDistanceFromInputVideoFrameFactory(m, mDistanceFromInputVideoFrameFactory + 1);        
        setInputVideoFrameFactoryId(m, mInputVideoFrameFactoryId);           
        setStatusInPipe(m, READY);        
        
        try
        {
            videoEncoder.encode(get());            
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoEncoder;
    }

    template <typename Container, typename AudioCodec,
              typename audioSampleRate, typename audioChannels >
    HTTPAudioVideoStreamer<Container,
                           CodecOrFormat, width, height,
                           AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (HTTPAudioVideoStreamer<Container,
                            CodecOrFormat, width, height,
                            AudioCodec, audioSampleRate, audioChannels>&
     httpAudioVideoStreamer)
    {
        Medium& m = httpAudioVideoStreamer;
        setDistanceFromInputVideoFrameFactory(m, mDistanceFromInputVideoFrameFactory + 1);        
        setInputVideoFrameFactoryId(m, mInputVideoFrameFactoryId);          
        setStatusInPipe(m, READY);        
        
        try
        {
            httpAudioVideoStreamer.takeStreamableFrame(get());
            httpAudioVideoStreamer.streamMuxedData();            
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return httpAudioVideoStreamer;
    }

    template <typename Container>
    HTTPVideoStreamer<Container, CodecOrFormat, width, height>&
    operator >>
    (HTTPVideoStreamer<Container, CodecOrFormat, width, height>& httpVideoStreamer)
    {
        Medium& m = httpVideoStreamer;
        setDistanceFromInputVideoFrameFactory(m, mDistanceFromInputVideoFrameFactory + 1);        
        setInputVideoFrameFactoryId(m, mInputVideoFrameFactoryId);  
        setStatusInPipe(m, READY);         
        
        try
        {
            httpVideoStreamer.takeStreamableFrame(get());
            httpVideoStreamer.streamMuxedData();           
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }

        return httpVideoStreamer;
    }

    template <typename Container, typename AudioCodec,
              typename audioSampleRate, typename audioChannels >
    UDPAudioVideoStreamer<Container,
                           CodecOrFormat, width, height,
                           AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (UDPAudioVideoStreamer<Container,
                            CodecOrFormat, width, height,
                            AudioCodec, audioSampleRate, audioChannels>&
     udpAudioVideoStreamer)
    {
        Medium& m = udpAudioVideoStreamer;
        setDistanceFromInputVideoFrameFactory(m, mDistanceFromInputVideoFrameFactory + 1);        
        setInputVideoFrameFactoryId(m, mInputVideoFrameFactoryId);          
        setStatusInPipe(m, READY);        
        
        try
        {
            udpAudioVideoStreamer.takeStreamableFrame(get());
            udpAudioVideoStreamer.streamMuxedData();            
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return udpAudioVideoStreamer;
    }    
    
    template <typename Container>
    UDPVideoStreamer<Container, CodecOrFormat, width, height>&
    operator >>
    (UDPVideoStreamer<Container, CodecOrFormat, width, height>& udpVideoStreamer)
    {
        Medium& m = udpVideoStreamer;
        setDistanceFromInputVideoFrameFactory(m, mDistanceFromInputVideoFrameFactory + 1);        
        setInputVideoFrameFactoryId(m, mInputVideoFrameFactoryId);          
        setStatusInPipe(m, READY); 
        
        try
        {
            udpVideoStreamer.takeStreamableFrame(get());
            udpVideoStreamer.streamMuxedData();            
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }

        return udpVideoStreamer;
    }    
    
    FFMPEGMJPEGDecoder<width, height>&
    operator >>
    (FFMPEGMJPEGDecoder<width, height>& videoDecoder)
    {
        Medium& m = videoDecoder;
        setDistanceFromInputVideoFrameFactory(m, mDistanceFromInputVideoFrameFactory + 1);        
        setInputVideoFrameFactoryId(m, mInputVideoFrameFactoryId);   
        setStatusInPipe(m, READY);         
        
        try
        {
            videoDecoder.decode(get());           
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoDecoder;
    }

private:

    /*
    bool isFrameEmpty(const EncodedVideoFrame& videoFrame) const
    {
        return videoFrame.size() == 0;
    }

    bool isFrameEmpty(const PackedRawVideoFrame& videoFrame) const
    {
        return videoFrame.size() == 0;
    }

    bool isFrameEmpty(const Planar3RawVideoFrame& videoFrame) const
    {
        return videoFrame.size<0>() == 0;
    }*/

    VideoFrame<CodecOrFormat, width, height> mVideoFrame;
};

}

#endif // VIDEOFRAMEHOLDER_HPP_INCLUDED

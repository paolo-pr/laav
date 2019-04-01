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
 */

#ifndef AUDIOFRAMEHOLDER_HPP_INCLUDED
#define AUDIOFRAMEHOLDER_HPP_INCLUDED

#include "FFMPEGAudioConverter.hpp"
#include "FFMPEGAudioVideoMuxer.hpp"

namespace laav
{

template <typename CodecOrFormat, typename audioSampleRate, typename audioChannels>
class AudioFrameHolder : public Medium
{

public:

    AudioFrameHolder(const std::string& id = "") :
        Medium(id)
    {
        Medium::mInputStatus = READY;        
    }

    /*!
     *  \exception MediumException
     */    
    void hold(const AudioFrame<CodecOrFormat, audioSampleRate, audioChannels>& audioFrame)
    {
        if (audioFrame.isEmpty())
            throw MediumException(INPUT_EMPTY);  
       
        mInputAudioFrameFactoryId = audioFrame.mFactoryId;
        Medium::mDistanceFromInputAudioFrameFactory = audioFrame.mDistanceFromFactory + 1;        
        
        if (Medium::mInputStatus ==  PAUSED)
            throw MediumException(MEDIUM_PAUSED);

        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);        

        Medium::mOutputStatus = READY;        
        mAudioFrame = audioFrame;
    }

    /*!
     *  \exception MediumException
     */
    AudioFrame<CodecOrFormat, audioSampleRate, audioChannels>& get()
    {     
        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);            
        
        mAudioFrame.mDistanceFromFactory = Medium::mDistanceFromInputAudioFrameFactory;
        
        if (Medium::mOutputStatus ==  NOT_READY)
            throw MediumException(OUTPUT_NOT_READY);        
        
        if (Medium::mInputStatus ==  PAUSED)
            throw MediumException(MEDIUM_PAUSED);

        return mAudioFrame;
    }

    template <typename ConvertedPCMSoundFormat, typename convertedAudioSampleRate,
              typename convertedAudioChannels>
    FFMPEGAudioConverter<CodecOrFormat, audioSampleRate, audioChannels,
                         ConvertedPCMSoundFormat, convertedAudioSampleRate, convertedAudioChannels>&
    operator >>
    (FFMPEGAudioConverter<CodecOrFormat, audioSampleRate, audioChannels,
                          ConvertedPCMSoundFormat, convertedAudioSampleRate, convertedAudioChannels>&
     audioConverter)
    {
        Medium& m = audioConverter;
        setDistanceFromInputAudioFrameFactory(m, mDistanceFromInputAudioFrameFactory + 1);        
        setInputAudioFrameFactoryId(m, mInputAudioFrameFactoryId);         
        setStatusInPipe(m, READY);
        
        try
        {
            audioConverter.convert(get());          
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return audioConverter;
    }

    template <typename Container, typename VideoCodec, typename width, typename height>
    FFMPEGAudioVideoMuxer<Container,
                          VideoCodec, width, height,
                          CodecOrFormat, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioVideoMuxer<Container,
                           VideoCodec, width, height,
                           CodecOrFormat, audioSampleRate, audioChannels>& audioVideoMuxer)
    {
        Medium& m = audioVideoMuxer;
        setDistanceFromInputAudioFrameFactory(m, mDistanceFromInputAudioFrameFactory + 1);        
        setInputAudioFrameFactoryId(m, mInputAudioFrameFactoryId);         
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

    template <typename Container, typename VideoCodec, typename width, typename height>
    HTTPAudioVideoStreamer<Container,
                           VideoCodec, width, height,
                           CodecOrFormat, audioSampleRate, audioChannels>&
    operator >>
    (HTTPAudioVideoStreamer<Container,
                            VideoCodec, width, height,
                            CodecOrFormat, audioSampleRate, audioChannels>& httpAudioVideoStreamer)
    {
        Medium& m = httpAudioVideoStreamer;
        setDistanceFromInputAudioFrameFactory(m, mDistanceFromInputAudioFrameFactory + 1);        
        setInputAudioFrameFactoryId(m, mInputAudioFrameFactoryId);        
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
    
    template <typename Container, typename VideoCodec, typename width, typename height>
    UDPAudioVideoStreamer<Container,
                           VideoCodec, width, height,
                           CodecOrFormat, audioSampleRate, audioChannels>&
    operator >>
    (UDPAudioVideoStreamer<Container,
                            VideoCodec, width, height,
                            CodecOrFormat, audioSampleRate, audioChannels>& udpAudioVideoStreamer)
    {
        Medium& m = udpAudioVideoStreamer;
        setDistanceFromInputAudioFrameFactory(m, mDistanceFromInputAudioFrameFactory + 1);        
        setInputAudioFrameFactoryId(m, mInputAudioFrameFactoryId);         
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
    
    template <typename AudioCodec>
    FFMPEGAudioEncoder<CodecOrFormat, AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioEncoder<CodecOrFormat, AudioCodec, audioSampleRate, audioChannels>& audioEncoder)
    {
        Medium& m = audioEncoder;
        setDistanceFromInputAudioFrameFactory(m, mDistanceFromInputAudioFrameFactory + 1);        
        setInputAudioFrameFactoryId(m, mInputAudioFrameFactoryId);             
        setStatusInPipe(m, READY);        
        
        try
        {
            audioEncoder.encode(get());            
        }
        catch (const MediumException& e)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return audioEncoder;
    }

    template <typename Container>
    HTTPAudioStreamer<Container, CodecOrFormat, audioSampleRate, audioChannels>&
    operator >>
    (HTTPAudioStreamer<Container, CodecOrFormat, audioSampleRate, audioChannels>& httpAudioStreamer)
    {
        Medium& m = httpAudioStreamer;
        setDistanceFromInputAudioFrameFactory(m, mDistanceFromInputAudioFrameFactory + 1);        
        setInputAudioFrameFactoryId(m, mInputAudioFrameFactoryId); 
        setStatusInPipe(m, READY);        
        
        try
        {
            httpAudioStreamer.takeStreamableFrame(get());
            httpAudioStreamer.streamMuxedData();            
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }

        return httpAudioStreamer;
    }

    template <typename Container>
    UDPAudioStreamer<Container, CodecOrFormat, audioSampleRate, audioChannels>&
    operator >>
    (UDPAudioStreamer<Container, CodecOrFormat, audioSampleRate, audioChannels>& udpAudioStreamer)
    {
        Medium& m = udpAudioStreamer;
        setDistanceFromInputAudioFrameFactory(m, mDistanceFromInputAudioFrameFactory + 1);        
        setInputAudioFrameFactoryId(m, mInputAudioFrameFactoryId);         
        setStatusInPipe(m, READY);
        
        try
        {
            udpAudioStreamer.takeStreamableFrame(get());
            udpAudioStreamer.streamMuxedData();            
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }

        return udpAudioStreamer;
    }    
    
    template <typename Container>
    FFMPEGAudioMuxer<Container, CodecOrFormat, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioMuxer<Container, CodecOrFormat, audioSampleRate, audioChannels>& audioMuxer)
    {
        Medium& m = audioMuxer;
        setDistanceFromInputAudioFrameFactory(m, mDistanceFromInputAudioFrameFactory + 1);        
        setInputAudioFrameFactoryId(m, mInputAudioFrameFactoryId);          
        setStatusInPipe(m, READY);        
        
        try
        {
            audioMuxer.mux(get());           
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }

        return audioMuxer;
    }

private:

    /*
    bool isFrameEmpty(const EncodedAudioFrame<CodecOrFormat>& audioFrame) const
    {
        return audioFrame.size() == 0;
    }

    bool isFrameEmpty(const PackedRawAudioFrame& audioFrame) const
    {
        return audioFrame.size() == 0;
    }

    bool isFrameEmpty(const Planar2RawAudioFrame& audioFrame) const
    {
        return audioFrame.size<0>() == 0;
    }*/

    AudioFrame<CodecOrFormat, audioSampleRate, audioChannels> mAudioFrame;

};

}

#endif // AUDIOFRAMEHOLDER_HPP_INCLUDED

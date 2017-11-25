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

namespace laav
{

template <typename CodecOrFormat, unsigned int audioSampleRate, enum AudioChannels audioChannels>
class AudioFrameHolder
{

public:

    AudioFrameHolder() :
        mMediaStatusInPipe(MEDIA_NOT_READY)
    {
    }

    void hold(const AudioFrame<CodecOrFormat, audioSampleRate, audioChannels>& audioFrame)
    {
        mAudioFrame = audioFrame;
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    AudioFrame<CodecOrFormat, audioSampleRate, audioChannels>& get()
    {
        if (mMediaStatusInPipe != MEDIA_READY || isFrameEmpty(mAudioFrame))
            throw MediaException(MEDIA_NO_DATA);
        return mAudioFrame;
    }

    template <typename ConvertedPCMSoundFormat, unsigned int convertedAudioSampleRate,
              enum AudioChannels convertedAudioChannels>
    FFMPEGAudioConverter<CodecOrFormat, audioSampleRate, audioChannels,
                         ConvertedPCMSoundFormat, convertedAudioSampleRate, convertedAudioChannels>&
    operator >>
    (FFMPEGAudioConverter<CodecOrFormat, audioSampleRate, audioChannels,
                          ConvertedPCMSoundFormat, convertedAudioSampleRate, convertedAudioChannels>&
     audioConverter)
    {
        try
        {
            audioConverter.convert(get());
            audioConverter.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            audioConverter.mMediaStatusInPipe = mediaException.cause();
        }
        return audioConverter;
    }

    template <typename Container, typename VideoCodec, unsigned int width, unsigned int height>
    FFMPEGAudioVideoMuxer<Container,
                          VideoCodec, width, height,
                          CodecOrFormat, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioVideoMuxer<Container,
                           VideoCodec, width, height,
                           CodecOrFormat, audioSampleRate, audioChannels>& audioVideoMuxer)
    {
        try
        {
            audioVideoMuxer.takeMuxableFrame(get());
        }
        catch (const MediaException& mediaException)
        {
            // End of pipe: don't do anything.
        }

        return audioVideoMuxer;
    }

    template <typename Container, typename VideoCodec, unsigned int width, unsigned int height>
    HTTPAudioVideoStreamer<Container,
                           VideoCodec, width, height,
                           CodecOrFormat, audioSampleRate, audioChannels>&
    operator >>
    (HTTPAudioVideoStreamer<Container,
                            VideoCodec, width, height,
                            CodecOrFormat, audioSampleRate, audioChannels>& httpAudioVideoStreamer)
    {
        try
        {
            httpAudioVideoStreamer.takeStreamableFrame(get());
            httpAudioVideoStreamer.streamMuxedData();
        }
        catch (const MediaException& mediaException)
        {
            // Do nothing, because the streamer is at the end of the pipe
        }

        return httpAudioVideoStreamer;
    }
    
    template <typename Container, typename VideoCodec, unsigned int width, unsigned int height>
    UDPAudioVideoStreamer<Container,
                           VideoCodec, width, height,
                           CodecOrFormat, audioSampleRate, audioChannels>&
    operator >>
    (UDPAudioVideoStreamer<Container,
                            VideoCodec, width, height,
                            CodecOrFormat, audioSampleRate, audioChannels>& udpAudioVideoStreamer)
    {
        try
        {
            udpAudioVideoStreamer.takeStreamableFrame(get());        
            udpAudioVideoStreamer.streamMuxedData();
        }
        catch (const MediaException& mediaException)
        {
            // Do nothing, because the streamer is at the end of the pipe
        }

        return udpAudioVideoStreamer;
    }    
    
    template <typename AudioCodec>
    FFMPEGAudioEncoder<CodecOrFormat, AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioEncoder<CodecOrFormat, AudioCodec, audioSampleRate, audioChannels>& audioEncoder)
    {
        try
        {
            audioEncoder.encode(get());
            audioEncoder.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& e)
        {
            audioEncoder.mMediaStatusInPipe = e.cause();
        }
        return audioEncoder;
    }

    template <typename Container>
    HTTPAudioStreamer<Container, CodecOrFormat, audioSampleRate, audioChannels>&
    operator >>
    (HTTPAudioStreamer<Container, CodecOrFormat, audioSampleRate, audioChannels>& httpAudioStreamer)
    {
        try
        {
            httpAudioStreamer.takeStreamableFrame(get());
            httpAudioStreamer.streamMuxedData();
        }
        catch (const MediaException& mediaException)
        {
            // Do nothing, because the streamer is at the end of the pipe
        }

        return httpAudioStreamer;
    }

    template <typename Container>
    UDPAudioStreamer<Container, CodecOrFormat, audioSampleRate, audioChannels>&
    operator >>
    (UDPAudioStreamer<Container, CodecOrFormat, audioSampleRate, audioChannels>& udpAudioStreamer)
    {
        try
        {
            udpAudioStreamer.takeStreamableFrame(get());
            udpAudioStreamer.streamMuxedData();
        }
        catch (const MediaException& mediaException)
        {
            // Do nothing, because the streamer is at the end of the pipe
        }

        return udpAudioStreamer;
    }    
    
    template <typename Container>
    FFMPEGAudioMuxer<Container, CodecOrFormat, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioMuxer<Container, CodecOrFormat, audioSampleRate, audioChannels>& audioMuxer)
    {
        try
        {
            audioMuxer.takeMuxableFrame(get());
        }
        catch (const MediaException& mediaException)
        {
            // Do nothing, because the streamer is at the end of the pipe
        }

        return audioMuxer;
    }

    enum MediaStatus mMediaStatusInPipe;

private:

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
    }

    AudioFrame<CodecOrFormat, audioSampleRate, audioChannels> mAudioFrame;

};

}

#endif // AUDIOFRAMEHOLDER_HPP_INCLUDED

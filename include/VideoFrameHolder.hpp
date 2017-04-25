/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef VIDEOFRAMEHOLDER_HPP_INCLUDED
#define VIDEOFRAMEHOLDER_HPP_INCLUDED

namespace laav
{

template <typename CodecOrFormat, unsigned int width, unsigned int height>
class VideoFrameHolder
{
public:

    VideoFrameHolder() :
        mMediaStatusInPipe(MEDIA_NOT_READY)
    {
    }

    void hold(const VideoFrame<CodecOrFormat, width, height>& videoFrame)
    {
        mVideoFrame = videoFrame;
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    VideoFrame<CodecOrFormat, width, height>& get()
    {
        if (mMediaStatusInPipe != MEDIA_READY || isFrameEmpty(mVideoFrame))
            throw MediaException(MEDIA_NO_DATA);
        return mVideoFrame;
    }

    template <typename ConvertedVideoFrameFormat, unsigned int outputWidth, unsigned int outputHeigth>
    FFMPEGVideoConverter<CodecOrFormat, width, height,
                         ConvertedVideoFrameFormat, outputWidth, outputHeigth>&
    operator >>
    (FFMPEGVideoConverter<CodecOrFormat, width, height,
                          ConvertedVideoFrameFormat, outputWidth, outputHeigth>& videoConverter)
    {
        try
        {
            videoConverter.convert(get());
            videoConverter.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            videoConverter.mMediaStatusInPipe = mediaException.cause();
        }
        return videoConverter;
    }

    template <typename Container, typename AudioCodec,
              unsigned int audioSampleRate, enum AudioChannels audioChannels>
    FFMPEGAudioVideoMuxer<Container,
                          CodecOrFormat, width, height,
                          AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioVideoMuxer<Container,
                           CodecOrFormat, width, height,
                           AudioCodec, audioSampleRate, audioChannels>& audioVideoMuxer)
    {
        try
        {
            audioVideoMuxer.takeMuxableFrame(get());
        }
        catch (const MediaException& mediaException)
        {
            // Don't do anything: end of pipe.
        }
        return audioVideoMuxer;
    }

    template <typename Container>
    FFMPEGVideoMuxer<Container, CodecOrFormat, width, height>&
    operator >>
    (FFMPEGVideoMuxer<Container, CodecOrFormat, width, height>& videoMuxer)
    {
        try
        {
            videoMuxer.takeMuxableFrame(get());
        }
        catch (const MediaException& mediaException)
        {
            // Don't do anything: end of pipe.
        }
        return videoMuxer;
    }

    template <typename EncodedVideoFrameCodec>
    VideoEncoder<CodecOrFormat, EncodedVideoFrameCodec, width, height>&
    operator >>
    (VideoEncoder<CodecOrFormat, EncodedVideoFrameCodec, width, height>& videoEncoder)
    {
        try
        {
            videoEncoder.encode(get());
            videoEncoder.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            videoEncoder.mMediaStatusInPipe = mediaException.cause();
        }
        return videoEncoder;
    }

    template <typename Container, typename AudioCodec,
              unsigned int audioSampleRate, enum AudioChannels audioChannels >
    HTTPAudioVideoStreamer<Container,
                           CodecOrFormat, width, height,
                           AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (HTTPAudioVideoStreamer<Container,
                            CodecOrFormat, width, height,
                            AudioCodec, audioSampleRate, audioChannels>&
     httpAudioVideoStreamer)
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

    template <typename Container>
    HTTPVideoStreamer<Container, CodecOrFormat, width, height>&
    operator >>
    (HTTPVideoStreamer<Container, CodecOrFormat, width, height>& httpVideoStreamer)
    {

        try
        {
            httpVideoStreamer.takeStreamableFrame(get());
            httpVideoStreamer.streamMuxedData();
        }
        catch (const MediaException& mediaException)
        {
            // Do nothing, because the streamer is at the end of the pipe
        }

        return httpVideoStreamer;
    }

    FFMPEGMJPEGDecoder<width, height>&
    operator >>
    (FFMPEGMJPEGDecoder<width, height>& videoDecoder)
    {
        try
        {
            videoDecoder.decode(get());
            videoDecoder.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            videoDecoder.mMediaStatusInPipe = mediaException.cause();
        }
        return videoDecoder;
    }

    enum MediaStatus mMediaStatusInPipe;

private:

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
    }

    VideoFrame<CodecOrFormat, width, height> mVideoFrame;
};

}

#endif // VIDEOFRAMEHOLDER_HPP_INCLUDED

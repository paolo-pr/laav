/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef VIDEOENCODER_HPP_INCLUDED
#define VIDEOENCODER_HPP_INCLUDED

#include "HTTPAudioVideoStreamer.hpp"
#include "HTTPVideoStreamer.hpp"
#include "UDPAudioVideoStreamer.hpp"
#include "UDPVideoStreamer.hpp"

namespace laav
{

template <typename EncodedVideoFrameCodec,
          unsigned int width,
          unsigned int height>
class VideoFrameHolder;

template <typename RawVideoFrameFormat,
          typename EncodedVideoFrameCodec,
          unsigned int width,
          unsigned int height>
class VideoEncoder
{

public:

    VideoEncoder():
        mMediaStatusInPipe(MEDIA_NOT_READY),
        mEncodingStartTime(0),
        mFillingEncodedVideoFrameBuffer(true),
        mEncodedVideoFrameBufferOffset(0)
    {
        unsigned int i;
        for (i = 0; i < encodedVideoFrameBufferSize; i++)
        {
            mEncodedVideoFrameBuffer.push_back(VideoFrame<EncodedVideoFrameCodec, width, height>());
        }
    }

    VideoFrameHolder<EncodedVideoFrameCodec, width, height>&
    operator >>
    (VideoFrameHolder<EncodedVideoFrameCodec, width, height>& videoFrameHolder)
    {
        if (mMediaStatusInPipe == MEDIA_READY)
            videoFrameHolder.mMediaStatusInPipe = MEDIA_READY;
        else
        {
            videoFrameHolder.mMediaStatusInPipe = mMediaStatusInPipe;
            mMediaStatusInPipe = MEDIA_READY;
            return videoFrameHolder;
        }
        try
        {
            videoFrameHolder.hold(lastEncodedFrame());
            videoFrameHolder.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            videoFrameHolder.mMediaStatusInPipe = mediaException.cause();
        }
        return videoFrameHolder;
    }

    template <typename Container, typename AudioCodec,
              unsigned int audioSampleRate, enum AudioChannels audioChannels>
    FFMPEGAudioVideoMuxer<Container,
                          EncodedVideoFrameCodec, width, height,
                          AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioVideoMuxer<Container,
                           EncodedVideoFrameCodec, width, height,
                           AudioCodec, audioSampleRate, audioChannels>& audioVideoMuxer)
    {
        if (mMediaStatusInPipe == MEDIA_READY)
            audioVideoMuxer.mMediaStatusInPipe = MEDIA_READY;
        else
        {
            audioVideoMuxer.mMediaStatusInPipe = mMediaStatusInPipe;
            mMediaStatusInPipe = MEDIA_READY;
            return audioVideoMuxer;
        }

        try
        {
            audioVideoMuxer.takeMuxableFrame(lastEncodedFrame());
        }
        catch (const MediaException& mediaException)
        {
            audioVideoMuxer.mMediaStatusInPipe = mediaException.cause();
        }
        return audioVideoMuxer;
    }

    template <typename Container>
    FFMPEGVideoMuxer<Container, EncodedVideoFrameCodec, width, height>&
    operator >>
    (FFMPEGVideoMuxer<Container, EncodedVideoFrameCodec, width, height>& videoMuxer)
    {
        try
        {
            videoMuxer.takeMuxableFrame(lastEncodedFrame());
        }
        catch (const MediaException& mediaException)
        {
            // DO nothing (end of pipe)
        }
        return videoMuxer;
    }

    template <typename Container, typename AudioCodec,
              unsigned int audioSampleRate, enum AudioChannels audioChannels>
    HTTPAudioVideoStreamer<Container,
                           EncodedVideoFrameCodec, width, height,
                           AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (HTTPAudioVideoStreamer<Container,
                            EncodedVideoFrameCodec, width, height,
                            AudioCodec, audioSampleRate, audioChannels>& httpAudioVideoStreamer)
    {
        if (mMediaStatusInPipe != MEDIA_READY)
        {
            mMediaStatusInPipe = MEDIA_READY;
            return httpAudioVideoStreamer;
        }
        else
        {
            try
            {
                httpAudioVideoStreamer.takeStreamableFrame(lastEncodedFrame());
                httpAudioVideoStreamer.streamMuxedData();
            }
            catch (const MediaException& mediaException)
            {
                // Do nothing, because the streamer is at the end of the pipe
            }
        }

        return httpAudioVideoStreamer;
    }

    template <typename Container>
    HTTPVideoStreamer<Container, EncodedVideoFrameCodec, width, height>&
    operator >>
    (HTTPVideoStreamer<Container, EncodedVideoFrameCodec, width, height>& httpVideoStreamer)
    {
        if (mMediaStatusInPipe != MEDIA_READY)
        {
            mMediaStatusInPipe = MEDIA_READY;
            return httpVideoStreamer;
        }
        else
        {
            try
            {
                httpVideoStreamer.takeStreamableFrame(lastEncodedFrame());
                httpVideoStreamer.streamMuxedData();
            }
            catch (const MediaException& mediaException)
            {
                // Do nothing, because the streamer is at the end of the pipe
            }
        }

        return httpVideoStreamer;
    }

    template <typename Container, typename AudioCodec,
              unsigned int audioSampleRate, enum AudioChannels audioChannels>
    UDPAudioVideoStreamer<Container,
                          EncodedVideoFrameCodec, width, height,
                          AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (UDPAudioVideoStreamer<Container,
                           EncodedVideoFrameCodec, width, height,
                           AudioCodec, audioSampleRate, audioChannels>& udpAudioVideoStreamer)
    {
        if (mMediaStatusInPipe != MEDIA_READY)
        {
            mMediaStatusInPipe = MEDIA_READY;
            return udpAudioVideoStreamer;
        }
        else
        {
            try
            {
                udpAudioVideoStreamer.takeStreamableFrame(lastEncodedFrame());
                udpAudioVideoStreamer.streamMuxedData();
            }
            catch (const MediaException& mediaException)
            {
                // Do nothing, because the streamer is at the end of the pipe
            }
        }

        return udpAudioVideoStreamer;
    }

    template <typename Container>
    UDPVideoStreamer<Container, EncodedVideoFrameCodec, width, height>&
    operator >>
    (UDPVideoStreamer<Container, EncodedVideoFrameCodec, width, height>& udpVideoStreamer)
    {
        if (mMediaStatusInPipe != MEDIA_READY)
        {
            mMediaStatusInPipe = MEDIA_READY;
            return udpVideoStreamer;
        }
        else
        {
            try
            {
                udpVideoStreamer.takeStreamableFrame(lastEncodedFrame());
                udpVideoStreamer.streamMuxedData();
            }
            catch (const MediaException& mediaException)
            {
                // Do nothing, because the streamer is at the end of the pipe
            }
        }

        return udpVideoStreamer;
    }
    
    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    VideoFrame<EncodedVideoFrameCodec, width, height>& encodedFrame(unsigned int bufferOffset)
    {
        if ( mFillingEncodedVideoFrameBuffer &&
             (bufferOffset % mEncodedVideoFrameBuffer.size() >= mEncodedVideoFrameBufferOffset) )
            throw MediaException(MEDIA_NO_DATA);
        else
        {
            return mEncodedVideoFrameBuffer[bufferOffset % mEncodedVideoFrameBuffer.size()];
        }
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
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

    // TODO: private with friends...
    enum MediaStatus mMediaStatusInPipe;

    virtual void encode(const VideoFrame<RawVideoFrameFormat, width, height>& rawVideoFrame) = 0;

protected:

    int64_t mEncodingStartTime;
    bool mFillingEncodedVideoFrameBuffer;
    std::vector<VideoFrame<EncodedVideoFrameCodec, width, height> > mEncodedVideoFrameBuffer;
    unsigned int mEncodedVideoFrameBufferOffset;

};

}

#endif // VIDEOENCODER_HPP_INCLUDED

/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef FFMPEGAUDIOENCODER_HPP_INCLUDED
#define FFMPEGAUDIOENCODER_HPP_INCLUDED

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include "FFMPEGCommon.hpp"
#include "HTTPAudioVideoStreamer.hpp"
#include "HTTPAudioStreamer.hpp"

namespace laav
{

template <typename AudioCodec, unsigned int audioSampleRate, enum AudioChannels audioChannels>
class AudioFrameHolder;

template <typename PCMSoundFormat, typename AudioCodec,
          unsigned int audioSampleRate, enum AudioChannels audioChannels>
class FFMPEGAudioEncoder
{

public:

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    void encode(const AudioFrame<PCMSoundFormat, audioSampleRate, audioChannels>& rawAudioFrame)
    {
        if (isFrameEmpty(rawAudioFrame))
            throw MediaException(MEDIA_NO_DATA);

        unsigned int bufferSize = mRawInputLibAVFrame->linesize[0];
        unsigned int periodSize = getPeriodSize(rawAudioFrame);

        if ((mRawInputLibAVFrameBufferOffset + periodSize) >= bufferSize)
        {

            copyRawAudioFrameDataToLibAVFrameData(mRawInputLibAVFrameBufferOffset,
                                                  rawAudioFrame,
                                                  (bufferSize - mRawInputLibAVFrameBufferOffset));

            if (mEncodingStartTime == 0)
                mEncodingStartTime = av_rescale_q(av_gettime_relative(),
                                                  AV_TIME_BASE_Q,
                                                  mAudioEncoderCodecContext->time_base);

            int64_t monotonicNow = av_rescale_q(av_gettime_relative(), AV_TIME_BASE_Q,
                                                mAudioEncoderCodecContext->time_base);

            mEncodingCurrTime = monotonicNow;// - mEncodingStartTime;

            this->mRawInputLibAVFrame->pts = mEncodingCurrTime;

            int ret = avcodec_send_frame(this->mAudioEncoderCodecContext, this->mRawInputLibAVFrame);
            if (ret == AVERROR(EAGAIN))
                return;
            else if (ret != 0)
                printAndThrowUnrecoverableError("avcodec_send_frame(...)");

            struct timespec dateTimeNow;
            // TODO ifdef linux
            clock_gettime(CLOCK_REALTIME, &dateTimeNow);

            mEncodingDateTs[mEncodedAVPacketBufferOffset] =
            dateTimeNow.tv_sec * 1000000000 + dateTimeNow.tv_nsec;

            mEncodingMonotonicTs[mEncodedAVPacketBufferOffset] = monotonicNow;
            AVPacket& currPkt = mEncodedAudioPktBuffer[mEncodedAVPacketBufferOffset];

            int ret2 = avcodec_receive_packet(this->mAudioEncoderCodecContext, &currPkt);

            // Buffering
            if (ret2 == AVERROR(EAGAIN))
            {
                mEncodedAVPacketBufferOffset =
                (mEncodedAVPacketBufferOffset + 1) % mEncodedAudioFrameBuffer.size();

                return;
            }
            else if (ret2 != 0)
                printAndThrowUnrecoverableError("avcodec_receive_packet(...)");
            else
            {

                AudioFrame<AudioCodec, audioSampleRate, audioChannels>& currFrame =
                mEncodedAudioFrameBuffer[mEncodedAudioFrameBufferOffset];

                AVPacket& encodedPkt = mEncodedAudioPktBuffer[mEncodedAudioFrameBufferOffset];
                if (FFMPEGUtils::translateCodec<AudioCodec>() == AV_CODEC_ID_AAC)
                {
                    if (!mAdtsHeaderWritten)
                    {
                        if (avformat_write_header(mAdtsMuxerContext, NULL) < 0)
                            printAndThrowUnrecoverableError("avformat_write_header(...)");

                        mAdtsHeaderWritten = true;
                    }
                    mEncodedFrameForAdtsMuxer = &currFrame;
                    int64_t pts = av_rescale_q(encodedPkt.pts, mAudioEncoderCodecContext->time_base,
                                               mAdtsMuxerContext->streams[0]->time_base);

                    encodedPkt.pts = encodedPkt.dts = pts;
                    if (av_write_frame(mAdtsMuxerContext, &encodedPkt) < 0)
                        printAndThrowUnrecoverableError("av_write_frame(...)");
                }
                else
                {
                    fillEncodedAudioFrame(currFrame, encodedPkt);
                }

                currFrame.setMonotonicTimestamp(mEncodingMonotonicTs[mEncodedAudioFrameBufferOffset]);
                currFrame.setDateTimestamp(mEncodingDateTs[mEncodedAudioFrameBufferOffset]);
                currFrame.mLibAVFlags = encodedPkt.flags;
                //av_packet_unref(&encodedPkt);

                if (mEncodedAudioFrameBufferOffset + 1 == mEncodedAudioFrameBuffer.size())
                {
                    mFillingEncodedAudioFrameBuffer = false;
                }

                mEncodedAudioFrameBufferOffset =
                (mEncodedAudioFrameBufferOffset + 1) % mEncodedAudioFrameBuffer.size();

                mEncodedAVPacketBufferOffset =
                (mEncodedAVPacketBufferOffset + 1) % mEncodedAudioFrameBuffer.size();


                if ((mRawInputLibAVFrameBufferOffset + periodSize) % bufferSize != 0)
                {
                    copyRawAudioFrameDataToLibAVFrameData
                    (0, rawAudioFrame, (mRawInputLibAVFrameBufferOffset + periodSize) % bufferSize);
                }

                mFillingEncodedAudioFrame = false;
            }
        }
        else
        {
            mFillingEncodedAudioFrame = true;
            copyRawAudioFrameDataToLibAVFrameData(mRawInputLibAVFrameBufferOffset,
                                                  rawAudioFrame, periodSize);
        }

        mRawInputLibAVFrameBufferOffset = (mRawInputLibAVFrameBufferOffset + periodSize) % bufferSize;
    }

    AudioFrameHolder<AudioCodec, audioSampleRate, audioChannels>&
    operator >> (AudioFrameHolder<AudioCodec, audioSampleRate, audioChannels>& audioFrameHolder)
    {
        if (mMediaStatusInPipe == MEDIA_READY)
            audioFrameHolder.mMediaStatusInPipe = MEDIA_READY;
        else
        {
            audioFrameHolder.mMediaStatusInPipe = mMediaStatusInPipe;
            mMediaStatusInPipe = MEDIA_READY;
            return audioFrameHolder;
        }
        try
        {
            audioFrameHolder.hold(lastEncodedFrame());
            audioFrameHolder.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            audioFrameHolder.mMediaStatusInPipe = mediaException.cause();
        }
        return audioFrameHolder;
    }

    template <typename Container, typename VideoCodec, unsigned int width, unsigned int height >
    FFMPEGAudioVideoMuxer<Container,
                          VideoCodec, width, height,
                          AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioVideoMuxer<Container,
                           VideoCodec, width, height,
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

    template <typename Container, typename VideoCodec, unsigned int width, unsigned int height>
    HTTPAudioVideoStreamer<Container, VideoCodec, width, height,
                           AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (HTTPAudioVideoStreamer<Container, VideoCodec, width, height,
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
    HTTPAudioStreamer<Container, AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (HTTPAudioStreamer<Container, AudioCodec, audioSampleRate, audioChannels>& httpAudioStreamer)
    {
        if (mMediaStatusInPipe != MEDIA_READY)
        {
            mMediaStatusInPipe = MEDIA_READY;
            return httpAudioStreamer;
        }
        else
        {
            try
            {
                httpAudioStreamer.takeStreamableFrame(lastEncodedFrame());
                httpAudioStreamer.sendMuxedData();
            }
            catch (const MediaException& mediaException)
            {
                // Do nothing, because the streamer is at the end of the pipe
            }
        }

        return httpAudioStreamer;
    }

    template <typename Container>
    FFMPEGAudioMuxer<Container, AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioMuxer<Container, AudioCodec, audioSampleRate, audioChannels>& audioMuxer)
    {
        if (mMediaStatusInPipe != MEDIA_READY)
        {
            mMediaStatusInPipe = MEDIA_READY;
            return audioMuxer;
        }
        else
        {
            try
            {
                audioMuxer.takeMuxableFrame(lastEncodedFrame());
            }
            catch (const MediaException& mediaException)
            {
                // Do nothing, because the streamer is at the end of the pipe
            }
        }

        return audioMuxer;
    }

    /*!
     *  \exception MediaException(MEDIA_BUFFERING)
     */
    AudioFrame<AudioCodec, audioSampleRate, audioChannels>& encodedFrame(unsigned int bufferOffset)
    {
        if (mFillingEncodedAudioFrame)
        {
            throw MediaException(MEDIA_BUFFERING);
        }
        if (mFillingEncodedAudioFrameBuffer &&
            (bufferOffset % mEncodedAudioFrameBuffer.size() >= mEncodedAudioFrameBufferOffset))
        {
            throw MediaException(MEDIA_BUFFERING);
        }
        else
        {
            return mEncodedAudioFrameBuffer[bufferOffset % mEncodedAudioFrameBuffer.size()];
        }
    }

    /*!
     *  \exception MediaException(MEDIA_BUFFERING)
     */
    AudioFrame<AudioCodec, audioSampleRate, audioChannels>& lastEncodedFrame()
    {
        return encodedFrame((mEncodedAudioFrameBuffer.size() + mEncodedAudioFrameBufferOffset - 1) %
                            mEncodedAudioFrameBuffer.size());
    }

    unsigned int encodedFrameBufferIndex() const
    {
        return mEncodedAudioFrameBufferOffset;
    }

    unsigned int encodedFrameBufferSize() const
    {
        return mEncodedAudioFrameBuffer.size();
    }

    // TODO: private with friend converter, decoder, encoder
    enum MediaStatus mMediaStatusInPipe;

protected:

    FFMPEGAudioEncoder():
        mMediaStatusInPipe(MEDIA_NOT_READY),
        mEncodingCurrTime(0),
        mCounter(0),
        mEncodingStartTime(0),
        mFillingEncodedAudioFrameBuffer(true),
        mFillingEncodedAudioFrame(true),
        mRawInputLibAVFrameBufferOffset(0),
        mEncodedAudioFrameBufferOffset(0),
        mEncodedAVPacketBufferOffset(0)
    {
        av_register_all();
        avcodec_register_all();
        mAudioCodec = avcodec_find_encoder(FFMPEGUtils::translateCodec<AudioCodec>() );
        if (mAudioCodec == NULL)
            printAndThrowUnrecoverableError("avcodec_find_encoder(...)");

        mAudioEncoderCodecContext = avcodec_alloc_context3(mAudioCodec);
        if (mAudioEncoderCodecContext == NULL)
            printAndThrowUnrecoverableError("mAudioEncoderCodecContext =avcodec_alloc_context3(...)");

        mAudioEncoderCodecContext->sample_fmt = FFMPEGUtils::translateSampleFormat<PCMSoundFormat>();
        mAudioEncoderCodecContext->sample_rate = audioSampleRate;
        mAudioEncoderCodecContext->channels = audioChannels + 1;
        mAudioEncoderCodecContext->channel_layout = av_get_default_channel_layout(audioChannels + 1);
        mAudioEncoderCodecContext->time_base = (AVRational)
        {
            1, audioSampleRate
        };
        mAudioEncoderCodecContext->codec_type = AVMEDIA_TYPE_AUDIO ;
        if (FFMPEGUtils::translateCodec<AudioCodec>() == AV_CODEC_ID_AAC)
        {
            mAudioEncoderCodecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
        }
    }

    void completeEncoderInitialization()
    {
        if (avcodec_open2(mAudioEncoderCodecContext, mAudioCodec, NULL) < 0)
            printAndThrowUnrecoverableError("avcodec_open2(...)");

        mRawInputLibAVFrame = av_frame_alloc();
        if (!mRawInputLibAVFrame)
            printAndThrowUnrecoverableError("mRawInputLibAVFrame = av_frame_alloc()");

        mRawInputLibAVFrame->nb_samples     = mAudioEncoderCodecContext->frame_size;
        mRawInputLibAVFrame->format         = mAudioEncoderCodecContext->sample_fmt;
        mRawInputLibAVFrame->channels       = mAudioEncoderCodecContext->channels;
        mRawInputLibAVFrame->channel_layout = mAudioEncoderCodecContext->channel_layout;
        mRawInputLibAVFrame->sample_rate    = audioSampleRate;
        // Allocate frame buffer
        if (av_frame_get_buffer(mRawInputLibAVFrame, 0) < 0)
            printAndThrowUnrecoverableError("av_frame_get_buffer(mRawInputLibAVFrame, 0)");

        unsigned int i;
        for (i = 0; i < encodedAudioFrameBufferSize; i++)
        {
            AVPacket encodedAudioPkt;
            encodedAudioPkt.size = 0;
            mEncodedAudioPktBuffer.push_back(encodedAudioPkt);
            av_init_packet(&mEncodedAudioPktBuffer[i]);
            AudioFrame<AudioCodec, audioSampleRate, audioChannels> encodedAudioFrame;
            encodedAudioFrame.setMonotonicTimeBase(mAudioEncoderCodecContext->time_base);
            mEncodedAudioFrameBuffer.push_back(encodedAudioFrame);
            mEncodingMonotonicTs.push_back(AV_NOPTS_VALUE);
            mEncodingDateTs.push_back(-1);
        }

        // AAC ENCODER needs the ADTS header. FFMPEG wants a muxer for making the header.
        if (FFMPEGUtils::translateCodec<AudioCodec>() == AV_CODEC_ID_AAC)
        {

            mAdtsMuxerFormat = av_guess_format("adts", NULL, NULL);
            if (!mAdtsMuxerFormat)
                printAndThrowUnrecoverableError("mAdtsMuxerFormat = av_guess_format(\"adts\" ...)");

            mAdtsMuxerFormat->audio_codec = AV_CODEC_ID_AAC;

            avformat_alloc_output_context2(&mAdtsMuxerContext, mAdtsMuxerFormat, "dummy", NULL);
            if (!mAdtsMuxerContext)
                printAndThrowUnrecoverableError("avformat_alloc_output_context2(...)");

            size_t adtsMuxerAVIOContextBufferSize = 4096;
            mAdtsMuxerAVIOContextBuffer = (uint8_t* )av_malloc(adtsMuxerAVIOContextBufferSize);
            if (!mAdtsMuxerAVIOContextBuffer)
                printAndThrowUnrecoverableError("mAdtsMuxerAVIOContextBuffer = (uint8_t* )...");

            mAdtsMuxerAVIOContext = avio_alloc_context(mAdtsMuxerAVIOContextBuffer,
                                                       adtsMuxerAVIOContextBufferSize, 1, this,
                                                       NULL, &writeAdtsFrame, NULL);
            if (!mAdtsMuxerAVIOContext)
                printAndThrowUnrecoverableError("mAdtsMuxerAVIOContext = avio_alloc_context(...)");

            mAdtsMuxerContext->pb = mAdtsMuxerAVIOContext;
            mAdtsAudioStream = avformat_new_stream(mAdtsMuxerContext, NULL);
            if (!mAdtsAudioStream)
                printAndThrowUnrecoverableError("mAdtsAudioStream = avformat_new_stream(...)");

            mAdtsAudioStream->id = mAdtsMuxerContext->nb_streams - 1;
            avcodec_parameters_from_context(mAdtsAudioStream->codecpar, mAudioEncoderCodecContext);
            mAdtsHeaderWritten = false;
            unsigned int i;
            for (i = 0; i < encodedAudioFrameBufferSize; i++)
            {
                mAudioDataPtrs.push_back(new unsigned char[4096]);
            }
        }

    }

    ~FFMPEGAudioEncoder()
    {
        unsigned int q;
        for (q = 0; q < mEncodedAudioPktBuffer.size(); q++)
            av_packet_unref(&mEncodedAudioPktBuffer[q]);
        av_frame_free(&mRawInputLibAVFrame);
        avcodec_free_context(&mAudioEncoderCodecContext);

        if (FFMPEGUtils::translateCodec<AudioCodec>() == AV_CODEC_ID_AAC)
        {
            av_write_trailer(mAdtsMuxerContext);
            av_free(mAdtsMuxerAVIOContextBuffer);
            avformat_free_context(mAdtsMuxerContext);
            av_free(mAdtsMuxerAVIOContext);
            unsigned int i;
            for (i = 0; i < encodedAudioFrameBufferSize; i++)
                delete[](mAudioDataPtrs[i]);
        }
    }

    AVCodecContext* mAudioEncoderCodecContext;

private:

    bool isFrameEmpty(const PackedRawAudioFrame& audioFrame) const
    {
        return audioFrame.size() == 0;
    }

    bool isFrameEmpty(const Planar2RawAudioFrame& audioFrame) const
    {
        return audioFrame.size<0>() == 0;
    }

    unsigned int getPeriodSize(const Planar2RawAudioFrame& rawAudioFrame)
    {
        return rawAudioFrame.size<0>();
    }

    void copyRawAudioFrameDataToLibAVFrameData(unsigned int libAVFrameBufferOffset,
                                               const Planar2RawAudioFrame& rawAudioFrame,
                                               unsigned int len)
    {
        unsigned int channels = audioChannels + 1;

        memcpy(mRawInputLibAVFrame->data[0] + libAVFrameBufferOffset,
               rawAudioFrame.plane<0>(), len);

        if (channels == 2)
            memcpy(mRawInputLibAVFrame->data[1] + libAVFrameBufferOffset,
                   rawAudioFrame.plane<1>(), len);
    }

    unsigned int getPeriodSize(const PackedRawAudioFrame& rawAudioFrame)
    {
        return rawAudioFrame.size();
    }

    void copyRawAudioFrameDataToLibAVFrameData(unsigned int libAVFrameBufferOffset,
                                               const PackedRawAudioFrame& rawAudioFrame,
                                               unsigned int len)
    {
        memcpy(mRawInputLibAVFrame->data[0] + libAVFrameBufferOffset, rawAudioFrame.data(), len);
    }

    void fillEncodedAudioFrame(EncodedAudioFrame<AudioCodec>& encodedAudioFrame,
                               const AVPacket& encodedAvPacket)
    {
        auto freeNothing = [](unsigned char* buffer) {  };
        ShareableAudioFrameData mAudioData = ShareableAudioFrameData(encodedAvPacket.data, freeNothing);
        encodedAudioFrame.assignDataSharedPtr(mAudioData);
        encodedAudioFrame.setSize(encodedAvPacket.size);
    }

    AVCodec* mAudioCodec;
    int64_t mEncodingCurrTime;
    int mCounter;
    int64_t mEncodingStartTime;
    bool mFillingEncodedAudioFrameBuffer;
    bool mFillingEncodedAudioFrame;
    unsigned int mRawInputLibAVFrameBufferOffset;
    std::vector<AudioFrame<AudioCodec, audioSampleRate, audioChannels> > mEncodedAudioFrameBuffer;
    std::vector<AVPacket> mEncodedAudioPktBuffer;
    std::vector<int64_t> mEncodingMonotonicTs;
    std::vector<int64_t> mEncodingDateTs;
    unsigned int mEncodedAudioFrameBufferOffset;
    unsigned int mEncodedAVPacketBufferOffset;
    AVFrame* mRawInputLibAVFrame;

    // NEEDED FOR AAC ENCODER (ADTS CONTAINER)
    AVFormatContext* mAdtsMuxerContext;
    uint8_t* mAdtsMuxerAVIOContextBuffer;
    AVOutputFormat* mAdtsMuxerFormat;
    AVIOContext* mAdtsMuxerAVIOContext;
    AVStream* mAdtsAudioStream;
    bool mAdtsHeaderWritten;
    // TODO: use a vector for other encoders too?
    std::vector<unsigned char* > mAudioDataPtrs;

    static int writeAdtsFrame (void* opaque, uint8_t* muxedDataSink, int chunkSize)
    {

        if (chunkSize == 0)
            return 0;
        // TODO: static cast?

        FFMPEGAudioEncoder<PCMSoundFormat, AudioCodec, audioSampleRate, audioChannels>* audioEncoder =
        (FFMPEGAudioEncoder<PCMSoundFormat, AudioCodec, audioSampleRate, audioChannels>* )opaque;

        auto freeNothing = [](unsigned char* buffer) {  };

        memcpy(audioEncoder->mAudioDataPtrs[audioEncoder->mEncodedAudioFrameBufferOffset],
               muxedDataSink, chunkSize);

        ShareableAudioFrameData mAudioData = ShareableAudioFrameData
        (audioEncoder->mAudioDataPtrs[audioEncoder->mEncodedAudioFrameBufferOffset], freeNothing);

        audioEncoder->mEncodedFrameForAdtsMuxer->assignDataSharedPtr(mAudioData);
        audioEncoder->mEncodedFrameForAdtsMuxer->setSize(chunkSize);

        return chunkSize;

    }

    AudioFrame<AudioCodec, audioSampleRate, audioChannels>* mEncodedFrameForAdtsMuxer;

};

}

#endif // FFMPEGAUDIOENCODER_HPP_INCLUDED

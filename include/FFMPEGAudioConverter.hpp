/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef FFMPEGAUDIOCONVERTER_HPP_INCLUDED
#define FFMPEGAUDIOCONVERTER_HPP_INCLUDED

extern "C"
{
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

#include "AllAudioCodecsAndFormats.hpp"
#include "Frame.hpp"

namespace laav
{

template <typename InputPCMSoundFormat,
          unsigned int inputAudioSampleRate,
          enum AudioChannels inputAudioChannels,
          typename ConvertedPCMSoundFormat,
          unsigned int convertedAudioSampleRate,
          enum AudioChannels convertedAudioChannels>
class FFMPEGAudioConverter
{

public:

    FFMPEGAudioConverter() :
        mMediaStatusInPipe(MEDIA_NOT_READY)
    {
        static_assert(inputAudioSampleRate == convertedAudioSampleRate,
        "In and out samplerates must be equal (TODO: implement the case they are different");
        mSwrContext = swr_alloc();
        if (!mSwrContext)
            printAndThrowUnrecoverableError("(mSwrContext = swr_alloc();)");

        av_opt_set_int(mSwrContext,
                       "in_channel_count", (inputAudioChannels + 1), 0);
        av_opt_set_int(mSwrContext,
                       "in_sample_rate", inputAudioSampleRate, 0);
        av_opt_set_sample_fmt(mSwrContext,
                              "in_sample_fmt",
                              FFMPEGUtils::translateSampleFormat<InputPCMSoundFormat>(), 0);
        av_opt_set_int(mSwrContext,
                       "out_channel_count", (convertedAudioChannels + 1), 0);
        av_opt_set_int(mSwrContext,
                       "out_sample_rate", convertedAudioSampleRate, 0);
        av_opt_set_sample_fmt(mSwrContext,
                              "out_sample_fmt",
                              FFMPEGUtils::translateSampleFormat<ConvertedPCMSoundFormat>(), 0);
        int ret;
        // initialize the converting context
        if ((ret = swr_init(mSwrContext)) < 0)
            printAndThrowUnrecoverableError("swr_init(mSwrContext)");

        mConvertedLibAVFrame = av_frame_alloc();
        if (!mConvertedLibAVFrame)
            printAndThrowUnrecoverableError("mConvertedLibAVFrame = av_frame_alloc()");

        mConvertedLibAVFrame->format = FFMPEGUtils::translateSampleFormat<ConvertedPCMSoundFormat>();
        mConvertedLibAVFrame->channel_layout = convertedAudioChannels + 1 == 1 ?
            AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
        mConvertedLibAVFrame->sample_rate = convertedAudioSampleRate;
        // TODO: use a global variable?
        mConvertedLibAVFrame->nb_samples = 10000;
        if (av_frame_get_buffer(mConvertedLibAVFrame, 0) < 0)
            printAndThrowUnrecoverableError("av_frame_get_buffer(mConvertedLibAVFrame, 0)");

        fillConvertedAudioFrame(mConvertedAudioFrame);
    }

    ~FFMPEGAudioConverter()
    {
        av_frame_free(&mConvertedLibAVFrame);
        swr_free(&mSwrContext);
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    AudioFrame<ConvertedPCMSoundFormat, convertedAudioSampleRate, convertedAudioChannels>&
    convert(const AudioFrame<InputPCMSoundFormat, inputAudioSampleRate, inputAudioChannels>&
            inputAudioFrame)
    {
        if (isFrameEmpty(inputAudioFrame))
            throw MediaException(MEDIA_NO_DATA);

        int inputSamplesNum = inputAudioFrame.size() / (2 * (inputAudioChannels + 1));
        int outSamplesNum = swr_get_out_samples(mSwrContext, inputSamplesNum );
        const unsigned char* inputData = inputAudioFrame.data();
        int ret = swr_convert(mSwrContext, mConvertedLibAVFrame->data,
                              outSamplesNum, (const uint8_t **)&inputData, inputSamplesNum);
        if (ret < 0)
            printAndThrowUnrecoverableError("swr_convert(...)");

        convert1(mConvertedAudioFrame, outSamplesNum);
        mConvertedAudioFrame.setTimestampsToNow();
        return mConvertedAudioFrame;
    }

    AudioFrameHolder<ConvertedPCMSoundFormat, convertedAudioSampleRate, convertedAudioChannels>&
    operator >>
    (AudioFrameHolder<ConvertedPCMSoundFormat, convertedAudioSampleRate, convertedAudioChannels>&
     audioFrameHolder)
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
            audioFrameHolder.hold(mConvertedAudioFrame);
            audioFrameHolder.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            audioFrameHolder.mMediaStatusInPipe = mediaException.cause();
        }
        return audioFrameHolder;
    }

    template <typename ReConvertedPCMSoundFormat,
              unsigned int reConvertedAudioSampleRate,
              enum AudioChannels reConvertedAudioChannels>
    FFMPEGAudioConverter<ConvertedPCMSoundFormat,
                         convertedAudioSampleRate,
                         convertedAudioChannels,
                         ReConvertedPCMSoundFormat,
                         reConvertedAudioSampleRate,
                         reConvertedAudioChannels>&
    operator >>
    (FFMPEGAudioConverter<ConvertedPCMSoundFormat,
                          convertedAudioSampleRate,
                          convertedAudioChannels,
                          ReConvertedPCMSoundFormat,
                          reConvertedAudioSampleRate,
                          reConvertedAudioChannels>& audioReConverter)
    {
        if (mMediaStatusInPipe == MEDIA_READY)
            audioReConverter.mMediaStatusInPipe = MEDIA_READY;
        else
        {
            audioReConverter.mMediaStatusInPipe = mMediaStatusInPipe;
            mMediaStatusInPipe = MEDIA_READY;
            return audioReConverter;
        }
        try
        {
            audioReConverter.convert(mConvertedAudioFrame);
        }
        catch (const MediaException& mediaException)
        {
            audioReConverter.mMediaStatusInPipe = mediaException.cause();
        }
        return audioReConverter;
    }

    template <typename AudioCodec>
    FFMPEGAudioEncoder<ConvertedPCMSoundFormat,
                       AudioCodec,
                       convertedAudioSampleRate,
                       convertedAudioChannels>&
    operator >>
    (FFMPEGAudioEncoder<ConvertedPCMSoundFormat,
                        AudioCodec,
                        convertedAudioSampleRate,
                        convertedAudioChannels>& audioEncoder)
    {
        if (mMediaStatusInPipe == MEDIA_READY)
            audioEncoder.mMediaStatusInPipe = MEDIA_READY;
        else
        {
            audioEncoder.mMediaStatusInPipe = mMediaStatusInPipe;
            mMediaStatusInPipe = MEDIA_READY;
            return audioEncoder;
        }

        try
        {
            audioEncoder.encode(mConvertedAudioFrame);
        }
        catch (const MediaException& mediaException)
        {
            audioEncoder.mMediaStatusInPipe = mediaException.cause();
        }

        return audioEncoder;
    }

    // TODO: private with friend framer, decoder
    enum MediaStatus mMediaStatusInPipe;

private:

    bool isFrameEmpty(const PackedRawAudioFrame& audioFrame) const
    {
        return audioFrame.size() == 0;
    }

    bool isFrameEmpty(const Planar2RawAudioFrame& audioFrame) const
    {
        return audioFrame.size<0>() == 0;
    }

    void fillConvertedAudioFrame(Planar2RawAudioFrame& rawAudioFrame)
    {
        auto freeNothing = [](unsigned char* buffer)
        {
            /*av_freep(&buffer); */
        };

        ShareableAudioFrameData convertedAudioFrameShData0 =
        ShareableAudioFrameData(mConvertedLibAVFrame->data[0], freeNothing);

        rawAudioFrame.assignSharedPtrForPlane<0>(convertedAudioFrameShData0);

        ShareableAudioFrameData convertedAudioFrameShData1 =

        ShareableAudioFrameData(mConvertedLibAVFrame->data[1], freeNothing);
        rawAudioFrame.assignSharedPtrForPlane<1>(convertedAudioFrameShData1);
    }

    void fillConvertedAudioFrame(PackedRawAudioFrame& rawAudioFrame)
    {
        auto freeNothing = [](unsigned char* buffer)
        {
            /*av_freep(&buffer); */
        };

        ShareableAudioFrameData convertedAudioFrameShData =
        ShareableAudioFrameData(mConvertedLibAVFrame->data[0], freeNothing);

        rawAudioFrame.assignDataSharedPtr(convertedAudioFrameShData);
    }

    // TODO: find a better name
    void convert1(PackedRawAudioFrame& rawAudioFrame, int outSamplesNum)
    {
        int bytesPerSample =
        av_get_bytes_per_sample(FFMPEGUtils::translateSampleFormat<ConvertedPCMSoundFormat>());

        mConvertedAudioFrame.setSize(outSamplesNum * bytesPerSample * (convertedAudioChannels + 1));
    }

    // TODO: find a better name
    void convert1(Planar2RawAudioFrame& rawAudioFrame, int outSamplesNum)
    {
        int bytesPerSample =
        av_get_bytes_per_sample(FFMPEGUtils::translateSampleFormat<ConvertedPCMSoundFormat>());

        rawAudioFrame.setSize<0>(outSamplesNum * bytesPerSample);
        rawAudioFrame.setSize<1>(outSamplesNum * bytesPerSample);
    }

    AudioFrame<ConvertedPCMSoundFormat,
               convertedAudioSampleRate,
               convertedAudioChannels> mConvertedAudioFrame;

    ShareableAudioFrameData mConvertedAudioData;
    AVFrame* mInputLibAVFrame;
    AVFrame* mConvertedLibAVFrame;
    struct SwrContext* mSwrContext;

};

}

#endif // FFMPEGAUDIOCONVERTER_HPP_INCLUDED

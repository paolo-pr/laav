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
          typename inputAudioSampleRate,
          typename inputAudioChannels,
          typename ConvertedPCMSoundFormat,
          typename convertedAudioSampleRate,
          typename convertedAudioChannels>
class FFMPEGAudioConverter : public Medium
{

public:

    FFMPEGAudioConverter(const std::string& id = "") :
        Medium(id)
    {

        static_assert(inputAudioSampleRate::value == convertedAudioSampleRate::value,
        "In and out samplerates must be equal (TODO: implement the case they are different");
        mSwrContext = swr_alloc();
        if (!mSwrContext)
            printAndThrowUnrecoverableError("(mSwrContext = swr_alloc();)");

        av_opt_set_int(mSwrContext, "in_channel_count", (inputAudioChannels::value + 1), 0);
        av_opt_set_int(mSwrContext, "in_sample_rate", inputAudioSampleRate::value, 0);
        av_opt_set_sample_fmt(mSwrContext, "in_sample_fmt",
                              FFMPEGUtils::translateSampleFormat<InputPCMSoundFormat>(), 0);
        av_opt_set_int(mSwrContext, "out_channel_count", (convertedAudioChannels::value + 1), 0);
        av_opt_set_int(mSwrContext, "out_sample_rate", convertedAudioSampleRate::value, 0);
        av_opt_set_sample_fmt(mSwrContext, "out_sample_fmt",
                              FFMPEGUtils::translateSampleFormat<ConvertedPCMSoundFormat>(), 0);
        int ret;
        // initialize the converting context
        if ((ret = swr_init(mSwrContext)) < 0)
            printAndThrowUnrecoverableError("swr_init(mSwrContext)");

        mConvertedLibAVFrame = av_frame_alloc();
        if (!mConvertedLibAVFrame)
            printAndThrowUnrecoverableError("mConvertedLibAVFrame = av_frame_alloc()");

        mConvertedLibAVFrame->format = FFMPEGUtils::translateSampleFormat<ConvertedPCMSoundFormat>();
        mConvertedLibAVFrame->channel_layout = convertedAudioChannels::value + 1 == 1 ?
            AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
        mConvertedLibAVFrame->sample_rate = convertedAudioSampleRate::value;
        // TODO: use a global variable?
        mConvertedLibAVFrame->nb_samples = 10000;
        if (av_frame_get_buffer(mConvertedLibAVFrame, 0) < 0)
            printAndThrowUnrecoverableError("av_frame_get_buffer(mConvertedLibAVFrame, 0)");
        
        fillConvertedAudioFrame(mConvertedAudioFrame);
        Medium::mInputStatus = READY;
    }

    ~FFMPEGAudioConverter()
    {
        av_frame_free(&mConvertedLibAVFrame);
        swr_free(&mSwrContext);
    }

    /*!
     *  \exception MediumException    * 
     */
    AudioFrame<ConvertedPCMSoundFormat, convertedAudioSampleRate, convertedAudioChannels>&
    convert(const AudioFrame<InputPCMSoundFormat, inputAudioSampleRate,             inputAudioChannels>&
            inputAudioFrame)
    {        
        if (inputAudioFrame.isEmpty())
            throw MediumException(INPUT_EMPTY);

        Medium::mDistanceFromInputAudioFrameFactory = inputAudioFrame.mDistanceFromFactory + 1;
        Medium::mInputAudioFrameFactoryId = inputAudioFrame.mFactoryId;        

        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);
        
        if (Medium::mInputStatus ==  PAUSED)
            throw MediumException(MEDIUM_PAUSED); 
        
        int inputSamplesNum = inputAudioFrame.size() / (2 * (inputAudioChannels::value + 1));
        int outSamplesNum = swr_get_out_samples(mSwrContext, inputSamplesNum );
        const unsigned char* inputData = inputAudioFrame.data();
        int ret = swr_convert(mSwrContext, mConvertedLibAVFrame->data,
                              outSamplesNum, (const uint8_t **)&inputData, inputSamplesNum);
        if (ret < 0)
            printAndThrowUnrecoverableError("swr_convert(...)");

        convert1(mConvertedAudioFrame, outSamplesNum);
        mConvertedAudioFrame.setTimestampsToNow();
        
        mConvertedAudioFrame.mFactoryId = Medium::mId;
        mConvertedAudioFrame.mDistanceFromFactory = 0;
        
        Medium::mOutputStatus = READY;        
        return mConvertedAudioFrame;
    }

    AudioFrameHolder<ConvertedPCMSoundFormat, convertedAudioSampleRate, convertedAudioChannels>&
    operator >>
    (AudioFrameHolder<ConvertedPCMSoundFormat, convertedAudioSampleRate, convertedAudioChannels>&
     audioFrameHolder)
    {
        Medium& m = audioFrameHolder;
        setDistanceFromInputAudioFrameFactory(m, 1);        
        setInputAudioFrameFactoryId(m, mId);         
        setStatusInPipe(m, mStatusInPipe);
        
        try
        {
            audioFrameHolder.hold(mConvertedAudioFrame);           
        }
        catch (const MediumException& me)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return audioFrameHolder;
    }

    template <typename ReConvertedPCMSoundFormat,
              typename reConvertedAudioSampleRate,
              typename reConvertedAudioChannels>
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
        Medium& m = audioReConverter;
        setDistanceFromInputAudioFrameFactory(m, 1);        
        setInputAudioFrameFactoryId(m, mId);         
        setStatusInPipe(m, mStatusInPipe);
        
        try
        {
            audioReConverter.convert(mConvertedAudioFrame);         
        }
        catch (const MediumException& me)
        {
            setStatusInPipe(m, NOT_READY);            
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
        Medium& m = audioEncoder;
        setDistanceFromInputAudioFrameFactory(m, 1);        
        setInputAudioFrameFactoryId(m, mId);          
        setStatusInPipe(m, mStatusInPipe);
        
        try
        {
            audioEncoder.encode(mConvertedAudioFrame);
        }
        catch (const MediumException& me)
        {
            setStatusInPipe(m, NOT_READY);            
        }

        return audioEncoder;
    }

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

        mConvertedAudioFrame.setSize(outSamplesNum * bytesPerSample * (convertedAudioChannels::value + 1));       
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

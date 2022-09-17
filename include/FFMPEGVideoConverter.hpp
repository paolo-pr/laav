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

#ifndef FFMPEGVIDEOCONVERTER_HPP_INCLUDED
#define FFMPEGVIDEOCONVERTER_HPP_INCLUDED

extern "C"
{
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#include "AllVideoCodecsAndFormats.hpp"
#include "Frame.hpp"
#include "FFMPEGCommon.hpp"
#include "FFMPEGVideoEncoder.hpp"

namespace laav
{

template <typename InputVideoFrameFormat,
          typename inputWidth,
          typename inputHeigth,
          typename ConvertedVideoFrameFormat,
          typename outputWidth,
          typename outputHeigth>
class FFMPEGVideoConverter : public Medium
{
          
public:

    FFMPEGVideoConverter(const std::string& id = ""):
        Medium(id)
    {
        static_assert( ((inputWidth::value >= outputWidth::value) && (inputHeigth::value >= outputHeigth::value)),
                       "(inputWidth::value <= outputWidth::value) && (inputHeigth::value <= outputHeigth::value) == false");
        // TODO add other static asserts for not allowed conversions
        // TODO static assert if FFMPEGUtils::translatePixelFormat fails for input and/or output
        AVPixelFormat convFmt = FFMPEGUtils::translatePixelFormat<ConvertedVideoFrameFormat>();
        AVPixelFormat inFmt   = FFMPEGUtils::translatePixelFormat<InputVideoFrameFormat>();

        //av_register_all();
        mInputLibAVFrame = av_frame_alloc();

        if (!mInputLibAVFrame)
            printAndThrowUnrecoverableError("(mInputLibAVFrame = av_frame_alloc())");

        mInputLibAVFrame->format = FFMPEGUtils::translatePixelFormat<InputVideoFrameFormat>();
        mInputLibAVFrame->width  = inputWidth::value;
        mInputLibAVFrame->height = inputHeigth::value;
        if (av_image_alloc(mInputLibAVFrame->data, mInputLibAVFrame->linesize,
                           inputWidth::value, inputHeigth::value,
                           FFMPEGUtils::translatePixelFormat<InputVideoFrameFormat>(), 1) < 0)
            printAndThrowUnrecoverableError("av_image_alloc(...)");
        av_freep(&mInputLibAVFrame->data[0]);

        const AVCodec* rawCodecInput = avcodec_find_decoder(AV_CODEC_ID_RAWVIDEO);
        mInputCodecContext = avcodec_alloc_context3(rawCodecInput);
        if (!mInputCodecContext)
            printAndThrowUnrecoverableError("mInputCodecContext = avcodec_alloc_context3(...)");

        mInputCodecContext->width = inputWidth::value;
        mInputCodecContext->height = inputHeigth::value;
        mInputCodecContext->pix_fmt = inFmt;
        if (avcodec_open2(mInputCodecContext, rawCodecInput, NULL) < 0)
            printAndThrowUnrecoverableError("avcodec_open2(...)");

        const AVCodec* rawCodecOutput = avcodec_find_decoder(AV_CODEC_ID_RAWVIDEO);
        mOutputCodecContext = avcodec_alloc_context3(rawCodecOutput);
        if (!mOutputCodecContext)
            printAndThrowUnrecoverableError("(mOutputCodecContext = avcodec_alloc_context3(...))");

        mOutputCodecContext->width = outputWidth::value;
        mOutputCodecContext->height = outputHeigth::value;
        mOutputCodecContext->pix_fmt = convFmt;
        if (avcodec_open2(mOutputCodecContext, rawCodecOutput, NULL) < 0)
            printAndThrowUnrecoverableError("avcodec_open2(...)");

        mConvertedLibAVFrame = av_frame_alloc();
        if (!mConvertedLibAVFrame)
            printAndThrowUnrecoverableError("(mConvertedLibAVFrame = av_frame_alloc())");

        mConvertedLibAVFrame->format = convFmt;
        mConvertedLibAVFrame->width  = outputWidth::value;
        mConvertedLibAVFrame->height = outputHeigth::value;
        if (av_image_alloc(mConvertedLibAVFrame->data, mConvertedLibAVFrame->linesize,
                           outputWidth::value, outputHeigth::value, convFmt, 1) < 0)
            printAndThrowUnrecoverableError("av_image_alloc(...)");

        auto freeLibAVFrameBuffer = [](unsigned char* buffer)
        {
            av_freep(&buffer);
        };
        // Use freeNothing for planes 1,2b ecause
        // they will automatically be freed by freeLibAVFrameBuffer;
        auto freeNothing = [](unsigned char* buffer) {  };

        mConvertedLibAVFrameData0 =
        ShareableVideoFrameData(mConvertedLibAVFrame->data[0], freeLibAVFrameBuffer);

        mConvertedLibAVFrameData1 =
        ShareableVideoFrameData(mConvertedLibAVFrame->data[1], freeNothing);

        mConvertedLibAVFrameData2 =
        ShareableVideoFrameData(mConvertedLibAVFrame->data[2], freeNothing);

        mSwscaleContext = sws_getContext(inputWidth::value, inputHeigth::value, inFmt, 
                                         outputWidth::value, outputHeigth::value,
                                         convFmt, SWS_BILINEAR, NULL, NULL, NULL);
        if (!mSwscaleContext)
            printAndThrowUnrecoverableError("(mSwscaleContext = sws_getContext(...");
        mapConvertedFrameToLibAVFrame(mConvertedVideoFrame);
        
        Medium::mInputStatus = READY;
    }

    /*!
     *  \exception MediumException
     */
    VideoFrame<ConvertedVideoFrameFormat, outputWidth, outputHeigth>&
    convert(const VideoFrame<InputVideoFrameFormat, inputWidth, inputHeigth>& inputVideoFrame)
    {
        Medium::mOutputStatus = NOT_READY;

        if (inputVideoFrame.isEmpty())
            throw MediumException(INPUT_EMPTY);

        Medium::mInputVideoFrameFactoryId = inputVideoFrame.mFactoryId;
        Medium::mDistanceFromInputVideoFrameFactory = inputVideoFrame.mDistanceFromFactory + 1;        

        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);
        
        if (Medium::mInputStatus ==  PAUSED)
            throw MediumException(MEDIUM_PAUSED);

        specializedConvert(inputVideoFrame);
        mConvertedVideoFrame.setTimestampsToNow();
        setSizeOfConvertedFrame(mConvertedVideoFrame);
        mConvertedVideoFrame.mFactoryId = Medium::mId;
        mConvertedVideoFrame.mDistanceFromFactory = 0;
        Medium::mOutputStatus = READY;
        
        return mConvertedVideoFrame;
    }

    ~FFMPEGVideoConverter()
    {
        avcodec_free_context(&mInputCodecContext);
        avcodec_free_context(&mOutputCodecContext);
        av_frame_free(&mInputLibAVFrame);
        av_frame_free(&mConvertedLibAVFrame);
        sws_freeContext(mSwscaleContext);
    }

    VideoFrameHolder<ConvertedVideoFrameFormat, outputWidth, outputHeigth>&
    operator >>
    (VideoFrameHolder<ConvertedVideoFrameFormat, outputWidth, outputHeigth>& videoFrameHolder)
    {
        Medium& m = videoFrameHolder;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId); 
        setStatusInPipe(m, mStatusInPipe);
        
        try
        {
            videoFrameHolder.hold(mConvertedVideoFrame);          
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoFrameHolder;
    }

    template <typename ReConvertedVideoFrameFormat,
              typename outputWidth2,
              typename outputHeigth2>
    FFMPEGVideoConverter<ConvertedVideoFrameFormat, outputWidth, outputHeigth,
                         ReConvertedVideoFrameFormat, outputWidth2, outputHeigth2>&
    operator >>
    (FFMPEGVideoConverter<ConvertedVideoFrameFormat, outputWidth, outputHeigth,
                          ReConvertedVideoFrameFormat, outputWidth2, outputHeigth2>& videoReConverter)
    {
        Medium& m = videoReConverter;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId); 
        setStatusInPipe(m, mStatusInPipe); 
        
        try
        {
            videoReConverter.convert(mConvertedVideoFrame);          
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoReConverter;
    }

    template <typename EncodedVideoFrameCodec>
    VideoEncoder<ConvertedVideoFrameFormat,
                 EncodedVideoFrameCodec, outputWidth, outputHeigth>&
    operator >>
    (VideoEncoder<ConvertedVideoFrameFormat,
                  EncodedVideoFrameCodec, outputWidth, outputHeigth>& videoEncoder)
    {
        Medium& m = videoEncoder;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);
        setStatusInPipe(m, mStatusInPipe); 
        
        try
        {
            videoEncoder.encode(mConvertedVideoFrame);           
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        
        return videoEncoder;
    }

private:

    bool isFrameEmpty(const PackedRawVideoFrame& videoFrame) const
    {
        return videoFrame.size() == 0;
    }

    bool isFrameEmpty(const Planar3RawVideoFrame& videoFrame) const
    {
        return videoFrame.size<0>() == 0;
    }

    void setSizeOfConvertedFrame(Planar3RawVideoFrame& convertedVideoFrame)
    {
        AVPixelFormat convFmt = FFMPEGUtils::translatePixelFormat<ConvertedVideoFrameFormat>();
        int size0 = av_image_get_linesize(convFmt, outputWidth::value, 0);
        int size1 = av_image_get_linesize(convFmt, outputWidth::value, 1);
        int size2 = av_image_get_linesize(convFmt, outputWidth::value, 2);
        convertedVideoFrame.setSize<0>(size0 * outputHeigth::value);
        convertedVideoFrame.setSize<1>(size1 * outputHeigth::value);
        convertedVideoFrame.setSize<2>(size2 * outputHeigth::value);
    }

    void setSizeOfConvertedFrame(PackedRawVideoFrame& convertedVideoFrame)
    {
        convertedVideoFrame.setSize(mConvertedLibAVFrame->linesize[0]*outputHeigth::value);
    }

    void specializedConvert(const PackedRawVideoFrame& inputVideoFrame)
    {
        mInputLibAVFrame->data[0] = (uint8_t*)&inputVideoFrame.data()[0];
        sws_scale(mSwscaleContext, (const uint8_t*const *)mInputLibAVFrame->data,
                                    mInputLibAVFrame->linesize, 0,
                                    inputHeigth::value, mConvertedLibAVFrame->data,
                                    mConvertedLibAVFrame->linesize);
    }

    /*  TODO ?
        const ConvertedVideoFrame& specializedConvert(const Planar2RawVideoFrameBase& inputVideoFrame)
        {

        }
    */

    void specializedConvert(const Planar3RawVideoFrame& inputVideoFrame)
    {
        mInputLibAVFrame->data[0] = (uint8_t*)inputVideoFrame.plane<0>();
        mInputLibAVFrame->data[1] = (uint8_t*)inputVideoFrame.plane<1>();
        mInputLibAVFrame->data[2] = (uint8_t*)inputVideoFrame.plane<2>();
        sws_scale(mSwscaleContext, (const uint8_t*const *)mInputLibAVFrame->data,
                                    mInputLibAVFrame->linesize, 0,
                                    inputHeigth::value, mConvertedLibAVFrame->data,
                                    mConvertedLibAVFrame->linesize);
    }

    void mapConvertedFrameToLibAVFrame(Planar3RawVideoFrame& outputVideoFrame)
    {
        outputVideoFrame.assignSharedPtrForPlane<0>(mConvertedLibAVFrameData0);
        outputVideoFrame.assignSharedPtrForPlane<1>(mConvertedLibAVFrameData1);
        outputVideoFrame.assignSharedPtrForPlane<2>(mConvertedLibAVFrameData2);
    }

    void mapConvertedFrameToLibAVFrame(PackedRawVideoFrame& outputVideoFrame)
    {
        outputVideoFrame.assignDataSharedPtr(mConvertedLibAVFrameData0);
    }

    VideoFrame<ConvertedVideoFrameFormat, outputWidth, outputHeigth> mConvertedVideoFrame;
    AVFrame* mInputLibAVFrame;
    //AVPicture* mInputLibAVPicture;
    ShareableVideoFrameData mConvertedLibAVFrameData0; // packed/plane0
    ShareableVideoFrameData mConvertedLibAVFrameData1; // plane1
    ShareableVideoFrameData mConvertedLibAVFrameData2; // plane2
    ShareableVideoFrameData mConvertedLibAVFrameData3; // plane3
    AVCodecContext* mInputCodecContext;
    AVCodecContext* mOutputCodecContext;
    AVFrame* mConvertedLibAVFrame;
    struct SwsContext* mSwscaleContext;

};

}

#endif // FFMPEGVIDEOCONVERTER_HPP_INCLUDED

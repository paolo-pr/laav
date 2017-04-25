/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
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
          unsigned int inputWidth,
          unsigned int inputHeigth,
          typename ConvertedVideoFrameFormat,
          unsigned int outputWidth,
          unsigned int outputHeigth>
class FFMPEGVideoConverter
{
public:

    FFMPEGVideoConverter():
        mMediaStatusInPipe(MEDIA_NOT_READY)
    {
        static_assert( ((inputWidth >= outputWidth) && (inputHeigth >= outputHeigth)),
                       "(inputWidth <= outputWidth) && (inputHeigth <= outputHeigth) == false");
        // TODO add other static asserts for not allowed conversions
        // TODO static assert if FFMPEGUtils::translatePixelFormat fails for input and/or output
        AVPixelFormat convFmt = FFMPEGUtils::translatePixelFormat<ConvertedVideoFrameFormat>();
        AVPixelFormat inFmt   = FFMPEGUtils::translatePixelFormat<InputVideoFrameFormat>();

        av_register_all();
        mInputLibAVFrame = av_frame_alloc();

        if (!mInputLibAVFrame)
            printAndThrowUnrecoverableError("(mInputLibAVFrame = av_frame_alloc())");

        mInputLibAVFrame->format = FFMPEGUtils::translatePixelFormat<InputVideoFrameFormat>();
        mInputLibAVFrame->width  = inputWidth;
        mInputLibAVFrame->height = inputHeigth;
        if (av_image_alloc(mInputLibAVFrame->data, mInputLibAVFrame->linesize,
                           inputWidth, inputHeigth,
                           FFMPEGUtils::translatePixelFormat<InputVideoFrameFormat>(), 1) < 0)
            printAndThrowUnrecoverableError("av_image_alloc(...)");
        av_freep(&mInputLibAVFrame->data[0]);

        AVCodec* rawCodecInput = avcodec_find_decoder(AV_CODEC_ID_RAWVIDEO);
        mInputCodecContext = avcodec_alloc_context3(rawCodecInput);
        if (!mInputCodecContext)
            printAndThrowUnrecoverableError("mInputCodecContext = avcodec_alloc_context3(...)");

        mInputCodecContext->width = inputWidth;
        mInputCodecContext->height = inputHeigth;
        mInputCodecContext->pix_fmt = inFmt;
        if (avcodec_open2(mInputCodecContext, rawCodecInput, NULL) < 0)
            printAndThrowUnrecoverableError("avcodec_open2(...)");

        AVCodec* rawCodecOutput = avcodec_find_decoder(AV_CODEC_ID_RAWVIDEO);
        mOutputCodecContext = avcodec_alloc_context3(rawCodecOutput);
        if (!mOutputCodecContext)
            printAndThrowUnrecoverableError("(mOutputCodecContext = avcodec_alloc_context3(...))");

        mOutputCodecContext->width = outputWidth;
        mOutputCodecContext->height = outputHeigth;
        mOutputCodecContext->pix_fmt = convFmt;
        if (avcodec_open2(mOutputCodecContext, rawCodecOutput, NULL) < 0)
            printAndThrowUnrecoverableError("avcodec_open2(...)");

        mConvertedLibAVFrame = av_frame_alloc();
        if (!mConvertedLibAVFrame)
            printAndThrowUnrecoverableError("(mConvertedLibAVFrame = av_frame_alloc())");

        mConvertedLibAVFrame->format = convFmt;
        mConvertedLibAVFrame->width  = outputWidth;
        mConvertedLibAVFrame->height = outputHeigth;
        if (av_image_alloc(mConvertedLibAVFrame->data, mConvertedLibAVFrame->linesize,
                           outputWidth, outputHeigth, convFmt, 1) < 0)
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

        mSwscaleContext = sws_getContext(inputWidth, inputHeigth, inFmt, outputWidth, outputHeigth,
                                         convFmt, SWS_BILINEAR, NULL, NULL, NULL);
        if (!mSwscaleContext)
            printAndThrowUnrecoverableError("(mSwscaleContext = sws_getContext(...");
        mapConvertedFrameToLibAVFrame(mConvertedVideoFrame);
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    VideoFrame<ConvertedVideoFrameFormat, outputWidth, outputHeigth>&
    convert(const VideoFrame<InputVideoFrameFormat, inputWidth, inputHeigth>& inputVideoFrame)
    {
        if (isFrameEmpty(inputVideoFrame))
            throw MediaException(MEDIA_NO_DATA);

        specializedConvert(inputVideoFrame);
        mConvertedVideoFrame.setTimestampsToNow();
        setSizeOfConvertedFrame(mConvertedVideoFrame);
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
            videoFrameHolder.hold(mConvertedVideoFrame);
            videoFrameHolder.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            videoFrameHolder.mMediaStatusInPipe = mediaException.cause();
        }
        return videoFrameHolder;
    }

    template <typename ReConvertedVideoFrameFormat,
              unsigned int outputWidth2,
              unsigned int outputHeigth2>
    FFMPEGVideoConverter<ConvertedVideoFrameFormat, outputWidth, outputHeigth,
                         ReConvertedVideoFrameFormat, outputWidth2, outputHeigth2>&
    operator >>
    (FFMPEGVideoConverter<ConvertedVideoFrameFormat, outputWidth, outputHeigth,
                          ReConvertedVideoFrameFormat, outputWidth2, outputHeigth2>& videoReConverter)
    {
        if (mMediaStatusInPipe == MEDIA_READY)
            videoReConverter.mMediaStatusInPipe = MEDIA_READY;
        else
        {
            videoReConverter.mMediaStatusInPipe = mMediaStatusInPipe;
            mMediaStatusInPipe = MEDIA_READY;
            return videoReConverter;
        }
        try
        {
            videoReConverter.convert(mConvertedVideoFrame);
        }
        catch (const MediaException& mediaException)
        {
            videoReConverter.mMediaStatusInPipe = mediaException.cause();
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
        if (mMediaStatusInPipe == MEDIA_READY)
            videoEncoder.mMediaStatusInPipe = MEDIA_READY;
        else
        {
            videoEncoder.mMediaStatusInPipe = mMediaStatusInPipe;
            mMediaStatusInPipe = MEDIA_READY;
            return videoEncoder;
        }

        try
        {
            videoEncoder.encode(mConvertedVideoFrame);
        }
        catch (const MediaException& mediaException)
        {
            videoEncoder.mMediaStatusInPipe = mediaException.cause();
        }
        return videoEncoder;
    }

    // TODO: private with friend framer, decoder
    enum MediaStatus mMediaStatusInPipe;

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
        int size0 = av_image_get_linesize(convFmt, outputWidth, 0);
        int size1 = av_image_get_linesize(convFmt, outputWidth, 1);
        int size2 = av_image_get_linesize(convFmt, outputWidth, 2);
        convertedVideoFrame.setSize<0>(size0 * outputHeigth);
        convertedVideoFrame.setSize<1>(size1 * outputHeigth);
        convertedVideoFrame.setSize<2>(size2 * outputHeigth);
    }

    void setSizeOfConvertedFrame(PackedRawVideoFrame& convertedVideoFrame)
    {
        convertedVideoFrame.setSize(mConvertedLibAVFrame->linesize[0]*outputHeigth);
    }

    void specializedConvert(const PackedRawVideoFrame& inputVideoFrame)
    {
        mInputLibAVFrame->data[0] = (uint8_t*)&inputVideoFrame.data()[0];
        sws_scale(mSwscaleContext, (const uint8_t*const *)mInputLibAVFrame->data,
                                    mInputLibAVFrame->linesize, 0,
                                    inputHeigth, mConvertedLibAVFrame->data,
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
                                    inputHeigth, mConvertedLibAVFrame->data,
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
    AVPicture* mInputLibAVPicture;
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

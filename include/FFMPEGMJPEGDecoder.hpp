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

#ifndef FFMPEGMJPEGDECODER_HPP_INCLUDED
#define FFMPEGMJPEGDECODER_HPP_INCLUDED

#include "FFMPEGCommon.hpp"
#include "FFMPEGVideoConverter.hpp"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
}

namespace laav
{

template <unsigned int width, unsigned int height>
class FFMPEGMJPEGDecoder
{

public:

    FFMPEGMJPEGDecoder() :
        mMediaStatusInPipe(MEDIA_NOT_READY)
    {
        av_register_all();
        avcodec_register_all();

        mVideoCodec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
        if (!mVideoCodec)
            printAndThrowUnrecoverableError("mVideoCodec = avcodec_find_decoder(AV_CODEC_ID_MJPEG)");

        mVideoDecoderCodecContext = avcodec_alloc_context3(mVideoCodec);
        if (!mVideoDecoderCodecContext)
            printAndThrowUnrecoverableError("");
        mVideoDecoderCodecContext->codec_id = AV_CODEC_ID_MJPEG;
        mVideoDecoderCodecContext->width = width;
        mVideoDecoderCodecContext->height = height;
        mVideoDecoderCodecContext->time_base = AV_TIME_BASE_Q;
        mVideoDecoderCodecContext->color_range = AVCOL_RANGE_JPEG;

        if (avcodec_open2(mVideoDecoderCodecContext, mVideoCodec, NULL) < 0)
            printAndThrowUnrecoverableError("avcodec_open2(...)");

        mDecodedLibAVFrame = av_frame_alloc();
        if (!mDecodedLibAVFrame)
            printAndThrowUnrecoverableError("mDecodedLibAVFrame = av_frame_alloc()");

        setDecodedFrameSizes(mDecodedVideoFrame);
    }

    ~FFMPEGMJPEGDecoder()
    {
        av_frame_free(&mDecodedLibAVFrame);
        avcodec_free_context(&mVideoDecoderCodecContext);
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    VideoFrame<YUV422_PLANAR, width, height>& decode(const EncodedVideoFrame& encodedVideoFrame)
    {
        AVPacket tempPkt;
        av_init_packet(&tempPkt);

        tempPkt.data = (uint8_t* )encodedVideoFrame.data();
        tempPkt.size = encodedVideoFrame.size();

        int ret = avcodec_send_packet(mVideoDecoderCodecContext, &tempPkt);
        // TODO separate send and receive errors? Doesn't seem necessary here...
        ret = avcodec_receive_frame(mVideoDecoderCodecContext, mDecodedLibAVFrame);
        if (ret != 0)
        {
            throw MediaException(MEDIA_NO_DATA);
        }

        auto freeNothing = [](unsigned char* buffer) {  };

        mDecodedLibAVFrameData0 = ShareableVideoFrameData(mDecodedLibAVFrame->data[0], freeNothing);
        mDecodedLibAVFrameData1 = ShareableVideoFrameData(mDecodedLibAVFrame->data[1], freeNothing);
        mDecodedLibAVFrameData2 = ShareableVideoFrameData(mDecodedLibAVFrame->data[2], freeNothing);
        fillDecodedFrame(mDecodedVideoFrame);
        av_packet_unref(&tempPkt);

        return mDecodedVideoFrame;
    }

    template <typename EncodedVideoFrameCodec_>
    VideoEncoder<YUV422_PLANAR, EncodedVideoFrameCodec_, width, height>&
    operator >>
    (VideoEncoder<YUV422_PLANAR, EncodedVideoFrameCodec_, width, height>& videoEncoder)
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
            videoEncoder.encode(mDecodedVideoFrame);
        }
        catch (const MediaException& mediaException)
        {
            videoEncoder.mMediaStatusInPipe = mediaException.cause();
        }
        return videoEncoder;
    }

    template <typename ConvertedVideoFrameFormat, unsigned int outputWidth, unsigned int outputheight>
    FFMPEGVideoConverter<YUV422_PLANAR, width, height,
                         ConvertedVideoFrameFormat, outputWidth, outputheight >&
    operator >>
    (FFMPEGVideoConverter<YUV422_PLANAR, width, height,
                          ConvertedVideoFrameFormat, outputWidth, outputheight >& videoConverter)
    {
        if (mMediaStatusInPipe == MEDIA_READY)
            videoConverter.mMediaStatusInPipe = MEDIA_READY;
        else
        {
            videoConverter.mMediaStatusInPipe = mMediaStatusInPipe;
            mMediaStatusInPipe = MEDIA_READY;
            return videoConverter;
        }
        try
        {
            videoConverter.convert(mDecodedVideoFrame);
            // TODO: is the following line really necessary?
            videoConverter.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            videoConverter.mMediaStatusInPipe = mediaException.cause();
        }
        return videoConverter;
    }

    VideoFrameHolder<YUV422_PLANAR, width, height>&
    operator >>
    (VideoFrameHolder<YUV422_PLANAR, width, height>& videoFrameHolder)
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
            videoFrameHolder.hold(mDecodedVideoFrame);
            videoFrameHolder.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            videoFrameHolder.mMediaStatusInPipe = mediaException.cause();
        }
        return videoFrameHolder;
    }

    enum MediaStatus mMediaStatusInPipe;

private:

    void fillDecodedFrame(Planar3RawVideoFrame& decodedFrame)
    {
        decodedFrame.assignSharedPtrForPlane<0>(mDecodedLibAVFrameData0);
        decodedFrame.assignSharedPtrForPlane<1>(mDecodedLibAVFrameData1);
        decodedFrame.assignSharedPtrForPlane<2>(mDecodedLibAVFrameData2);
    }

    void setDecodedFrameSizes(Planar3RawVideoFrame& decodedFrame)
    {
        int size0 = av_image_get_linesize(AV_PIX_FMT_YUVJ422P, width, 0);
        int size1 = av_image_get_linesize(AV_PIX_FMT_YUVJ422P, width, 1);
        int size2 = av_image_get_linesize(AV_PIX_FMT_YUVJ422P, width, 2);
        decodedFrame.setSize<0>(size0 * height);
        decodedFrame.setSize<1>(size1 * height);
        decodedFrame.setSize<2>(size2 * height);
    }

    AVCodecContext* mVideoDecoderCodecContext;
    AVCodec* mVideoCodec;
    AVFrame* mDecodedLibAVFrame;
    VideoFrame< YUV422_PLANAR, width, height > mDecodedVideoFrame;
    ShareableVideoFrameData mDecodedLibAVFrameData0; // packed/plane0
    ShareableVideoFrameData mDecodedLibAVFrameData1; // plane1
    ShareableVideoFrameData mDecodedLibAVFrameData2; // plane2

};

}

#endif // FFMPEGMJPEGDECODER_HPP_INCLUDED

/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef FFMPEGVIDEOENCODER_HPP_INCLUDED
#define FFMPEGVIDEOENCODER_HPP_INCLUDED

#include "FFMPEGCommon.hpp"
#include "VideoEncoder.hpp"
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

template <typename RawVideoFrameFormat,
          typename EncodedVideoFrameCodec,
          unsigned int width,
          unsigned int height>
class FFMPEGVideoEncoder : public VideoEncoder<RawVideoFrameFormat, H264, width, height>
{

public:

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    void encode(const VideoFrame<RawVideoFrameFormat, width, height>& inputRawVideoFrame)
    {
        fillLibAVFrame(inputRawVideoFrame);
        this->doEncode();
    }

    // TODO: implement for packed and planar2
    // void fillLibAVFrame(const PackedRawVideoFrameBase& inputRawVideoFrame);

protected:

    FFMPEGVideoEncoder() :
        mLatency(0),
        mInternalPtsBufferOffset(0)
    {
        avcodec_register_all();

        mVideoCodec = avcodec_find_encoder(FFMPEGUtils::translateCodec<EncodedVideoFrameCodec>());
        if (!mVideoCodec)
            printAndThrowUnrecoverableError("mVideoCodec = avcodec_find_encoder(...)");

        mVideoEncoderCodecContext = avcodec_alloc_context3(mVideoCodec);
        if (!mVideoEncoderCodecContext)
            printAndThrowUnrecoverableError("mVideoEncoderCodecContext = avcodec_alloc_context3(...)");

        mVideoEncoderCodecContext->codec_id =
        FFMPEGUtils::translateCodec<EncodedVideoFrameCodec>();

        mVideoEncoderCodecContext->width = width;
        mVideoEncoderCodecContext->height = height;
        mVideoEncoderCodecContext->time_base = AV_TIME_BASE_Q;
        mVideoEncoderCodecContext->max_b_frames = 0;

        mVideoEncoderCodecContext->pix_fmt =
        FFMPEGUtils::translatePixelFormat<RawVideoFrameFormat>();

        mInputLibAVFrame = av_frame_alloc();
        if (!mInputLibAVFrame)
            printAndThrowUnrecoverableError("mInputLibAVFrame = av_frame_alloc()");

        mInputLibAVFrame->format = FFMPEGUtils::translatePixelFormat<RawVideoFrameFormat>();
        mInputLibAVFrame->width  = mVideoEncoderCodecContext->width;
        mInputLibAVFrame->height = mVideoEncoderCodecContext->height;

        int ret = av_image_alloc(mInputLibAVFrame->data,
                                 mInputLibAVFrame->linesize,
                                 mVideoEncoderCodecContext->width,
                                 mVideoEncoderCodecContext->height,
                                 // TODO: 32 == ??? Should it be a parameter?
                                 FFMPEGUtils::translatePixelFormat<RawVideoFrameFormat>(), 32);
        // Need to free the allocated internal picture buffed,
        // because it will be replaced by the inputVideoFrame's buffer
        if (ret < 0)
            printAndThrowUnrecoverableError("int ret = av_image_alloc(mInputLibAVFrame->data,...;");

        av_freep(&mInputLibAVFrame->data[0]);

        unsigned int i;
        for (i = 0; i < encodedVideoFrameBufferSize; i++)
        {
            AVPacket encodedVideoPkt;
            encodedVideoPkt.size = 0;
            mEncodedVideoPktBuffer.push_back(encodedVideoPkt);
            av_init_packet(&mEncodedVideoPktBuffer[i]);
        }

        for (i = 0; i < encodedVideoFrameBufferSize; i++)
        {
            mInternalMonotonicPtsBuffer.push_back(AV_NOPTS_VALUE);
            mInternalDatePtsBuffer.push_back(-1);
        }

    }

    void completeEncoderInitialization()
    {
        if (avcodec_open2(mVideoEncoderCodecContext, mVideoCodec, NULL) < 0)
            printAndThrowUnrecoverableError("avcodec_open2(...)");
    }

    ~FFMPEGVideoEncoder()
    {
        unsigned int q;
        for (q = 0; q < mEncodedVideoPktBuffer.size(); q++)
            av_packet_unref(&mEncodedVideoPktBuffer[q]);
        av_frame_free(&mInputLibAVFrame);
        avcodec_free_context(&mVideoEncoderCodecContext);
    }

    AVCodecContext* mVideoEncoderCodecContext;

private:

    AVCodec* mVideoCodec;
    AVFrame* mInputLibAVFrame;
    std::vector<AVPacket> mEncodedVideoPktBuffer;

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    void fillLibAVFrame(const Planar3RawVideoFrame& inputRawVideoFrame)
    {
        if (inputRawVideoFrame.size<0>() == 0)
        {
            throw MediaException(MEDIA_NO_DATA);
        }
        this->mInputLibAVFrame->data[0] = (uint8_t* )inputRawVideoFrame.plane<0>();
        this->mInputLibAVFrame->data[1] = (uint8_t* )inputRawVideoFrame.plane<1>();
        this->mInputLibAVFrame->data[2] = (uint8_t* )inputRawVideoFrame.plane<2>();
    }

    void doEncode()
    {
        if (this->mEncodingStartTime == 0)
            this->mEncodingStartTime = av_gettime_relative();

        // "this" is wanted by the compiler
        AVPacket& currEncodedVideoPkt =
        this->mEncodedVideoPktBuffer[this->mEncodedVideoFrameBufferOffset];

        VideoFrame<H264, width, height>& currEncodedVideoFrame =
        this->mEncodedVideoFrameBuffer[this->mEncodedVideoFrameBufferOffset];

        av_packet_unref(&currEncodedVideoPkt);

        int64_t nowPts = av_gettime_relative();
        this->mInputLibAVFrame->pts = nowPts;

        int ret = avcodec_send_frame(this->mVideoEncoderCodecContext, this->mInputLibAVFrame);
        if (ret == AVERROR(EAGAIN))
        {
            return;
        }
        else if (ret != 0)
            printAndThrowUnrecoverableError("avcodec_send_frame(...)");

        ret = avcodec_receive_packet(this->mVideoEncoderCodecContext, &currEncodedVideoPkt);
        if (ret == AVERROR(EAGAIN))
            ;
        else if (ret != 0)
            printAndThrowUnrecoverableError("avcodec_receive_packet(...)");

        this->mInternalMonotonicPtsBuffer[this->mInternalPtsBufferOffset] = nowPts;
        struct timespec dateTimeNow;
        // TODO ifdef linux
        clock_gettime(CLOCK_REALTIME, &dateTimeNow);
        this->mInternalDatePtsBuffer[this->mInternalPtsBufferOffset] =
        dateTimeNow.tv_sec * 1000000000 + dateTimeNow.tv_nsec;

        if (ret == 0)
        {

            if (mLatency == 0)
            {
                mLatency = av_gettime_relative() - this->mEncodingStartTime;
            }

            auto freeNothing = [](unsigned char* buffer) {  };
            ShareableVideoFrameData mVideoData =
            ShareableVideoFrameData(currEncodedVideoPkt.data, freeNothing);

            currEncodedVideoFrame.assignDataSharedPtr(mVideoData);
            currEncodedVideoFrame.setSize(currEncodedVideoPkt.size);
            currEncodedVideoFrame.mLibAVFlags = currEncodedVideoPkt.flags;
            currEncodedVideoFrame.mLibAVSideData = currEncodedVideoPkt.side_data;
            currEncodedVideoFrame.mLibAVSideDataElems = currEncodedVideoPkt.side_data_elems;
            int64_t monoPts = this->mInternalMonotonicPtsBuffer[this->mEncodedVideoFrameBufferOffset];
            int64_t datePts = this->mInternalDatePtsBuffer[this->mEncodedVideoFrameBufferOffset];
            currEncodedVideoFrame.setMonotonicTimestamp(monoPts);
            currEncodedVideoFrame.setDateTimestamp(datePts);

            if (this->mEncodedVideoFrameBufferOffset + 1 == this->mEncodedVideoFrameBuffer.size())
                this->mFillingEncodedVideoFrameBuffer = false;

            this->mEncodedVideoFrameBufferOffset =
            (this->mEncodedVideoFrameBufferOffset + 1) % this->mEncodedVideoFrameBuffer.size();
        }

        this->mInternalPtsBufferOffset =
        (this->mInternalPtsBufferOffset + 1) % this->mEncodedVideoFrameBuffer.size();
    }

    int64_t mLatency;
    unsigned int mInternalPtsBufferOffset;
    std::vector<int64_t> mInternalMonotonicPtsBuffer;
    std::vector<int64_t> mInternalDatePtsBuffer;

};

}

#endif // FFMPEGVIDEOENCODER_HPP_INCLUDED

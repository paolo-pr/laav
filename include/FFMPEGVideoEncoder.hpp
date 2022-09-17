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
          typename width,
          typename height>
class FFMPEGVideoEncoder : public VideoEncoder<RawVideoFrameFormat, EncodedVideoFrameCodec, width, height>
{

// accesses the mVideoCodecContext member for copying the parameters needed by some formats (i.e: matroska)
template <typename Container, typename VideoCodecOrFormat,
          typename width_, typename height_>
friend class FFMPEGVideoMuxer;    

template <typename Container, typename VideoCodecOrFormat,
          typename width_, typename height_,
          typename AudioCodecOrFormat,
          typename audioSampleRate, typename audioChannels>
friend class FFMPEGAudioVideoMuxer;

public:

    /*!
     *  \exception MediumException
     */
    void encode(const VideoFrame<RawVideoFrameFormat, width, height>& inputRawVideoFrame)
    {

        if (inputRawVideoFrame.isEmpty())
            throw MediumException(INPUT_EMPTY);

        Medium::mInputVideoFrameFactoryId = inputRawVideoFrame.mFactoryId;
        Medium::mDistanceFromInputVideoFrameFactory = inputRawVideoFrame.mDistanceFromFactory + 1;        

        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);  
        
        if (Medium::mInputStatus == PAUSED)
            throw MediumException(MEDIUM_PAUSED);
        
        this->beforeEncodeCallback();
        fillLibAVFrame(inputRawVideoFrame);       
        this->doEncode();
        this->afterEncodeCallback();
    }

    void generateKeyFrame()
    {
        mGenerateKeyFrame = true;
    }
    
    // TODO: implement for packed and planar2
    // void fillLibAVFrame(const PackedRawVideoFrameBase& inputRawVideoFrame);

protected:

    FFMPEGVideoEncoder(const std::string& id = ""):
        VideoEncoder<RawVideoFrameFormat, EncodedVideoFrameCodec, width, height>(id),
        mGenerateKeyFrame(false),
        mLatency(0),       
        mInternalPtsBufferOffset(0)
    {
        //avcodec_register_all();
        VideoEncoder<RawVideoFrameFormat, EncodedVideoFrameCodec, width, height>::mNeedsToBuffer = true;
        const AVCodec* videoCodec = avcodec_find_encoder(FFMPEGUtils::translateCodec<EncodedVideoFrameCodec>());
        if (!videoCodec)
            printAndThrowUnrecoverableError("videoCodec = avcodec_find_encoder(...)");

        mVideoEncoderCodecContext = avcodec_alloc_context3(videoCodec);
        if (!mVideoEncoderCodecContext)
            printAndThrowUnrecoverableError("mVideoEncoderCodecContext = avcodec_alloc_context3(...)");

        mVideoEncoderCodecContext->codec_id =
        FFMPEGUtils::translateCodec<EncodedVideoFrameCodec>();

        mVideoEncoderCodecContext->width = width::value;
        mVideoEncoderCodecContext->height = height::value;
        mVideoEncoderCodecContext->time_base = AV_TIME_BASE_Q;
        mVideoEncoderCodecContext->max_b_frames = 0;    
        mVideoEncoderCodecContext->pix_fmt =
        FFMPEGUtils::translatePixelFormat<RawVideoFrameFormat>();

        
        mVideoEncoderCodecContextOutOfBand = avcodec_alloc_context3(videoCodec);
        if (!mVideoEncoderCodecContextOutOfBand)
            printAndThrowUnrecoverableError("mVideoEncoderCodecContextOutOfBand = avcodec_alloc_context3(...)");

        mVideoEncoderCodecContextOutOfBand->codec_id =
        FFMPEGUtils::translateCodec<EncodedVideoFrameCodec>();

        mVideoEncoderCodecContextOutOfBand->width = width::value;
        mVideoEncoderCodecContextOutOfBand->height = height::value;
        mVideoEncoderCodecContextOutOfBand->time_base = AV_TIME_BASE_Q;
        mVideoEncoderCodecContextOutOfBand->max_b_frames = 0;
        mVideoEncoderCodecContextOutOfBand->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;        
        mVideoEncoderCodecContextOutOfBand->pix_fmt =
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
            AVPacket* encodedVideoPkt = av_packet_alloc();
            encodedVideoPkt->size = 0;
            mEncodedVideoPktBuffer.push_back(encodedVideoPkt);
            //mEncodedVideoPktBuffer[i] = av_packet_alloc();
            //av_init_packet(&mEncodedVideoPktBuffer[i]);
        }

        for (i = 0; i < encodedVideoFrameBufferSize; i++)
        {
            mInternalMonotonicPtsBuffer.push_back(AV_NOPTS_VALUE);
            mInternalDatePtsBuffer.push_back(-1);
        }

    }

    void completeEncoderInitialization()
    {
        const AVCodec* videoCodec = avcodec_find_encoder(FFMPEGUtils::translateCodec<EncodedVideoFrameCodec>());
        if (avcodec_open2(mVideoEncoderCodecContext, videoCodec, NULL) < 0)
            printAndThrowUnrecoverableError("avcodec_open2(...)");
        if (avcodec_open2(mVideoEncoderCodecContextOutOfBand, videoCodec, NULL) < 0)
            printAndThrowUnrecoverableError("avcodec_open2(...)");        
    }

    ~FFMPEGVideoEncoder()
    {
        unsigned int q;
        for (q = 0; q < mEncodedVideoPktBuffer.size(); q++)
            av_packet_free(&mEncodedVideoPktBuffer[q]);
        av_frame_free(&mInputLibAVFrame);
        avcodec_free_context(&mVideoEncoderCodecContext);
        avcodec_free_context(&mVideoEncoderCodecContextOutOfBand);
    }

    AVCodecContext* mVideoEncoderCodecContext;
    AVCodecContext* mVideoEncoderCodecContextOutOfBand;
    
private:

    //AVCodec* mVideoCodec;
    AVFrame* mInputLibAVFrame;
    std::vector<AVPacket* > mEncodedVideoPktBuffer;
    bool mGenerateKeyFrame;

    /*!
     *  \exception MediumException
     */
    void fillLibAVFrame(const Planar3RawVideoFrame& inputRawVideoFrame)
    {
        if (inputRawVideoFrame.isEmpty())
        {
            throw MediumException(INPUT_EMPTY);
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
        AVPacket* currEncodedVideoPkt =
        this->mEncodedVideoPktBuffer[this->mEncodedVideoFrameBufferOffset];

        VideoFrame<EncodedVideoFrameCodec, width, height>& currEncodedVideoFrame =
        this->mEncodedVideoFrameBuffer[this->mEncodedVideoFrameBufferOffset];

        //av_packet_unref(&currEncodedVideoPkt);

        int64_t nowPts = av_gettime_relative();
        this->mInputLibAVFrame->pts = nowPts;
        
        if(mGenerateKeyFrame)
        {
            this->mInputLibAVFrame->pict_type = AV_PICTURE_TYPE_I;
            mGenerateKeyFrame = false;
        }
        else
            this->mInputLibAVFrame->pict_type = AV_PICTURE_TYPE_NONE;        
        
        int ret = avcodec_send_frame(this->mVideoEncoderCodecContext, this->mInputLibAVFrame);
        if (ret == AVERROR(EAGAIN))
        {
            return;
        }
        else if (ret != 0)
            printAndThrowUnrecoverableError("avcodec_send_frame(...)");

        ret = avcodec_receive_packet(this->mVideoEncoderCodecContext, currEncodedVideoPkt);
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
            VideoEncoder<RawVideoFrameFormat, EncodedVideoFrameCodec, width, height>::mNeedsToBuffer = false;
            Medium::mOutputStatus = READY;
            if (mLatency == 0)
            {
                mLatency = av_gettime_relative() - this->mEncodingStartTime;
            }

            auto freeNothing = [](unsigned char* buffer) {  };
            ShareableVideoFrameData mVideoData =
            ShareableVideoFrameData(currEncodedVideoPkt->data, freeNothing);

            currEncodedVideoFrame.assignDataSharedPtr(mVideoData);
            currEncodedVideoFrame.setSize(currEncodedVideoPkt->size);
            currEncodedVideoFrame.mLibAVFlags = currEncodedVideoPkt->flags;
            currEncodedVideoFrame.mAVCodecContext = mVideoEncoderCodecContext;
            currEncodedVideoFrame.mLibAVSideData = currEncodedVideoPkt->side_data;
            currEncodedVideoFrame.mLibAVSideDataElems = currEncodedVideoPkt->side_data_elems;
            int64_t monoPts = this->mInternalMonotonicPtsBuffer[this->mEncodedVideoFrameBufferOffset];
            int64_t datePts = this->mInternalDatePtsBuffer[this->mEncodedVideoFrameBufferOffset];
            currEncodedVideoFrame.setMonotonicTimestamp(monoPts);
            currEncodedVideoFrame.setDateTimestamp(datePts);
            currEncodedVideoFrame.mFactoryId = Medium::mId;
            currEncodedVideoFrame.mDistanceFromFactory = 0;
            
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

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

template <typename width, typename height>
class FFMPEGMJPEGDecoder : public Medium
{

public:

    FFMPEGMJPEGDecoder(const std::string& id = "") :
        Medium(id)
    {
        //av_register_all();
        //avcodec_register_all();

        mVideoCodec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
        if (!mVideoCodec)
            printAndThrowUnrecoverableError("mVideoCodec = avcodec_find_decoder(AV_CODEC_ID_MJPEG)");

        mVideoDecoderCodecContext = avcodec_alloc_context3(mVideoCodec);
        if (!mVideoDecoderCodecContext)
            printAndThrowUnrecoverableError("");
        mVideoDecoderCodecContext->codec_id = AV_CODEC_ID_MJPEG;
        mVideoDecoderCodecContext->width = width::value;
        mVideoDecoderCodecContext->height = height::value;
        mVideoDecoderCodecContext->time_base = AV_TIME_BASE_Q;
        mVideoDecoderCodecContext->color_range = AVCOL_RANGE_JPEG;

        if (avcodec_open2(mVideoDecoderCodecContext, mVideoCodec, NULL) < 0)
            printAndThrowUnrecoverableError("avcodec_open2(...)");

        mDecodedLibAVFrame = av_frame_alloc();
        if (!mDecodedLibAVFrame)
            printAndThrowUnrecoverableError("mDecodedLibAVFrame = av_frame_alloc()");

        setDecodedFrameSizes(mDecodedVideoFrame);
        
        Medium::mInputStatus = READY;
    }

    ~FFMPEGMJPEGDecoder()
    {
        av_frame_free(&mDecodedLibAVFrame);
        avcodec_free_context(&mVideoDecoderCodecContext);
    }

    /*!
     *  \exception MediumException
     */
    template <typename EncodedVideoFrame>
    VideoFrame<YUV422_PLANAR, width, height>& decode(EncodedVideoFrame& encodedVideoFrame)
    {
        Medium::mOutputStatus = NOT_READY;

        if (encodedVideoFrame.isEmpty())
            throw MediumException(INPUT_EMPTY);

        Medium::mDistanceFromInputVideoFrameFactory = encodedVideoFrame.mDistanceFromFactory + 1;        
        Medium::mInputVideoFrameFactoryId = encodedVideoFrame.mFactoryId;        
        mDecodedVideoFrame.mFactoryId = Medium::mId;         
        mDecodedVideoFrame.mDistanceFromFactory = 0;

        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);         
        
        if (Medium::mInputStatus ==  PAUSED)
            throw MediumException(MEDIUM_PAUSED);
        
        AVPacket tempPkt;
        av_init_packet(&tempPkt);

        tempPkt.data = (uint8_t* )encodedVideoFrame.data();
        tempPkt.size = encodedVideoFrame.size();

        // This block prevents a continuous warning from libavcodec (from 3.3.3 version)
        // when an APP field with uncorrect length is found in the MJPEG 
        // header. Some USB camera (misteriously?) put more than one APP field in the
        // header, with bad (are they really bad? libavcodec says that they should be < 6)
        // lengths...
        unsigned i = 0;
        while (i < encodedVideoFrame.size() - 3)
        {
            // APP field found
            if (tempPkt.data[i] == 0xff &&
                (tempPkt.data[i+1] >= 0xe0 && tempPkt.data[i+1] <= 0xef))
            {
                // bad length found
                if ( (tempPkt.data[i+3] | tempPkt.data[i+2] << 8 ) < 6)
                    tempPkt.data[i+1] = 0;
            }
            
            if (tempPkt.data[i] == 0xff && tempPkt.data[i+1] == 0xda ) 
                break; // // ff da: SOS (start of scanning) field found
            i++;    
        }
       
        int ret = avcodec_send_packet(mVideoDecoderCodecContext, &tempPkt);
        // TODO separate send and receive errors? Doesn't seem necessary here...
        ret = avcodec_receive_frame(mVideoDecoderCodecContext, mDecodedLibAVFrame);
        if (ret != 0)
        {
            throw MediumException(OUTPUT_NOT_READY);
        }

        auto freeNothing = [](unsigned char* buffer) {  };

        mDecodedLibAVFrameData0 = ShareableVideoFrameData(mDecodedLibAVFrame->data[0], freeNothing);
        mDecodedLibAVFrameData1 = ShareableVideoFrameData(mDecodedLibAVFrame->data[1], freeNothing);
        mDecodedLibAVFrameData2 = ShareableVideoFrameData(mDecodedLibAVFrame->data[2], freeNothing);
        fillDecodedFrame(mDecodedVideoFrame);
        av_packet_unref(&tempPkt);

        Medium::mOutputStatus = READY;
        return mDecodedVideoFrame;
    }

    template <typename EncodedVideoFrameCodec_>
    VideoEncoder<YUV422_PLANAR, EncodedVideoFrameCodec_, width, height>&
    operator >>
    (VideoEncoder<YUV422_PLANAR, EncodedVideoFrameCodec_, width, height>& videoEncoder)
    {
        Medium& m = videoEncoder;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);         
        setStatusInPipe(m, mStatusInPipe); 
        
        try
        {
            videoEncoder.encode(mDecodedVideoFrame);           
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoEncoder;
    }

    template <typename ConvertedVideoFrameFormat, typename outputWidth, typename outputheight>
    FFMPEGVideoConverter<YUV422_PLANAR, width, height,
                         ConvertedVideoFrameFormat, outputWidth, outputheight >&
    operator >>
    (FFMPEGVideoConverter<YUV422_PLANAR, width, height,
                          ConvertedVideoFrameFormat, outputWidth, outputheight >& videoConverter)
    {
        Medium& m = videoConverter;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId); 
        setStatusInPipe(m, mStatusInPipe); 
        
        try
        {
            videoConverter.convert(mDecodedVideoFrame);
            
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoConverter;
    }

    VideoFrameHolder<YUV422_PLANAR, width, height>&
    operator >>
    (VideoFrameHolder<YUV422_PLANAR, width, height>& videoFrameHolder)
    {
        Medium& m = videoFrameHolder;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);       
        setStatusInPipe(m, mStatusInPipe); 
        
        try
        {
            videoFrameHolder.hold(mDecodedVideoFrame);
               
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoFrameHolder;
    }

private:

    void fillDecodedFrame(Planar3RawVideoFrame& decodedFrame)
    {
        decodedFrame.assignSharedPtrForPlane<0>(mDecodedLibAVFrameData0);
        decodedFrame.assignSharedPtrForPlane<1>(mDecodedLibAVFrameData1);
        decodedFrame.assignSharedPtrForPlane<2>(mDecodedLibAVFrameData2);
    }

    void setDecodedFrameSizes(Planar3RawVideoFrame& decodedFrame)
    {
        int size0 = av_image_get_linesize(AV_PIX_FMT_YUV422P, width::value, 0);
        int size1 = av_image_get_linesize(AV_PIX_FMT_YUV422P, width::value, 1);
        int size2 = av_image_get_linesize(AV_PIX_FMT_YUV422P, width::value, 2);
        decodedFrame.setSize<0>(size0 * height::value);
        decodedFrame.setSize<1>(size1 * height::value);
        decodedFrame.setSize<2>(size2 * height::value);
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

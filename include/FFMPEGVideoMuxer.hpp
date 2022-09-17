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

#ifndef FFMPEGMuxerVideoImpl_HPP_INCLUDED
#define FFMPEGMuxerVideoImpl_HPP_INCLUDED

#include "FFMPEGVideoEncoder.hpp"
#include "FFMPEGMuxer.hpp"

namespace laav
{

// DIAMOND pattern
template <typename Container, typename VideoCodecOrFormat, typename width, typename height>
class FFMPEGMuxerVideoImpl : public virtual FFMPEGMuxerCommonImpl<Container>
{

protected:

    FFMPEGMuxerVideoImpl(const std::string& id = "") :
        FFMPEGMuxerCommonImpl<Container>(id)
    {
        
        Medium::mId = id;        
          
        mVideoStream = avformat_new_stream(FFMPEGMuxerCommonImpl<Container>::mMuxerContext, NULL);
        if (!mVideoStream)
            printAndThrowUnrecoverableError("mVideoStream = avformat_new_stream(...)");

        mVideoStream->id = FFMPEGMuxerCommonImpl<Container>::mMuxerContext->nb_streams - 1;

        unsigned int i;
        for (i = 1; i < encodedVideoFrameBufferSize; i++)
        {
            AVPacket* videoPkt = av_packet_alloc();
            this->mVideoAVPktsToMux.push_back(videoPkt);
        }

        for (i = 1; i < encodedVideoFrameBufferSize; i++)
        {
            //av_init_packet(&this->mVideoAVPktsToMux[i]);
            this->mVideoAVPktsToMux[i]->pts = AV_NOPTS_VALUE;
        }
                
    }

    ~FFMPEGMuxerVideoImpl()
    {
        unsigned int i;
        for (i = 1; i < encodedVideoFrameBufferSize; i++)
            av_packet_free(&this->mVideoAVPktsToMux[i]);
    }

    /*!
     *  \exception MediumException
     */
    void mux(const VideoFrame<VideoCodecOrFormat, width, height>& videoFrameToMux)
    {
        
        if (videoFrameToMux.isEmpty())
            throw MediumException(INPUT_EMPTY);

        Medium::mInputVideoFrameFactoryId = videoFrameToMux.mFactoryId;
        Medium::mDistanceFromInputVideoFrameFactory = videoFrameToMux.mDistanceFromFactory + 1;         
        
        if (Medium::mInputStatus == PAUSED)
            throw MediumException(MEDIUM_PAUSED);         

        if (Medium::mStatusInPipe == NOT_READY)
            throw MediumException(PIPE_NOT_READY);         
        
        if (videoFrameToMux.mLibAVFlags & AV_PKT_FLAG_KEY || std::is_same<VideoCodecOrFormat, MJPEG>())
            FFMPEGMuxerCommonImpl<Container>::mMuxedChunksHelper.gotVideoKeyFrameForRec = true;
        
        if (FFMPEGMuxerCommonImpl<Container>::isRecording() &&
            FFMPEGMuxerCommonImpl<Container>::mMuxedChunksHelper.needsVideoKeyFrame &&
            !FFMPEGMuxerCommonImpl<Container>::mMuxedChunksHelper.gotVideoKeyFrameForRec)
        {
            throw MediumException(INPUT_NOT_READY);
        }
        
        FFMPEGMuxerCommonImpl<Container>::mVideoReady = true;

        int64_t pts;
        pts = av_rescale_q(videoFrameToMux.monotonicTimestamp(), AV_TIME_BASE_Q, this->mMuxerContext->streams[this->mVideoStreamIndex]->time_base);

        this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset]->pts = pts;
        this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset]->dts = pts;

        this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset]->stream_index =
        FFMPEGMuxerCommonImpl<Container>::mVideoStreamIndex;
        //TODO: remove cast ?
        this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset]->data =
        (uint8_t*)videoFrameToMux.data();

        this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset]->size = videoFrameToMux.size();
        this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset]->flags = videoFrameToMux.mLibAVFlags;
        if (this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset]->side_data)
        {
            this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset]->side_data =
            videoFrameToMux.mLibAVSideData;

            this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset]->side_data_elems =
            videoFrameToMux.mLibAVSideDataElems;
        }

        this->mVideoAVPktsToMuxOffset =
        (this->mVideoAVPktsToMuxOffset + 1) % this->mVideoAVPktsToMux.size();

        //if (this->mDoMux)
        {
            this->mMuxedChunksHelper.newGroupOfChunks = false;
            this->muxNextUsefulFrameFromBuffer(true);
        }
    }
    
    void setParamsFromEmbeddedVideoCodecContext()
    {
        const AVCodec* videoCodec = avcodec_find_encoder(FFMPEGUtils::translateCodec<VideoCodecOrFormat>());
        if (!videoCodec)
            printAndThrowUnrecoverableError("AVCodec* videoCodec = avcodec_find_encoder(...)");

        AVCodecContext* embeddedVideoCodecContext = avcodec_alloc_context3(videoCodec);
        if (!embeddedVideoCodecContext)
            printAndThrowUnrecoverableError("embeddedVideoCodecContext = avcodec_alloc_context3(...)");        
              
        AVStream* videoStream = FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, width, height>::mVideoStream;
            
        embeddedVideoCodecContext->codec_id = FFMPEGUtils::translateCodec<VideoCodecOrFormat>();;
        embeddedVideoCodecContext->width = width::value;
        embeddedVideoCodecContext->height = height::value;
        embeddedVideoCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
        embeddedVideoCodecContext->time_base = AV_TIME_BASE_Q;
        
        avcodec_parameters_from_context(videoStream->codecpar, embeddedVideoCodecContext);        
        avcodec_free_context(&embeddedVideoCodecContext);        
    }

    AVStream* mVideoStream;    

};

template <typename Container, typename VideoCodecOrFormat, typename width, typename height>
class FFMPEGVideoMuxer : public FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, width, height>
{
    
public:

    FFMPEGVideoMuxer(const std::string& id = "") :
    FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, width, height>(id)
    {
        static_assert(! (std::is_same<Container, MATROSKA>::value && std::is_same<VideoCodecOrFormat, H264>::value), 
                      "Please pass the H264 Encoder to the constructor!");
        
        FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, width, height>::setParamsFromEmbeddedVideoCodecContext();
        ctorCommon();
    }

    template <typename RawVideoFrameFormat>
    FFMPEGVideoMuxer(const FFMPEGVideoEncoder<RawVideoFrameFormat, VideoCodecOrFormat, width, height>& vEnc, const std::string& id = "") :
    FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, width, height>(id)
    {
        // they will be freed by the avformat cleanup stuff
        avcodec_parameters_from_context(FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, width, height>::mVideoStream->codecpar, 
                                        vEnc.mVideoEncoderCodecContextOutOfBand);       
        ctorCommon();
    }
    
    /*!
     *  \exception MediumException
     */        
    void mux(const VideoFrame<VideoCodecOrFormat, width, height>& videoFrameToMux)
    {        
        FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, 
                             width, height>::mux(videoFrameToMux);
    }    
    
    /*!
     *  \exception MediumException
     */
    const std::vector<MuxedVideoData<Container, VideoCodecOrFormat, width, height> >&
    muxedVideoChunks()
    {
        if (Medium::mInputStatus == PAUSED)
            throw MediumException(MEDIUM_PAUSED);        

        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);         
        
        if (this->mMuxedChunksHelper.newGroupOfChunks)
        {
            unsigned int n;
            for (n = 0; n < FFMPEGMuxerCommonImpl<Container>::mMuxedChunksHelper.offset; n++)
            {
                mMuxedVideoChunks[n].setSize(this->mMuxedChunksHelper.data[n].size);
            }
            Medium::mOutputStatus = READY;
            return mMuxedVideoChunks;
        }
        else
        {
            Medium::mOutputStatus = NOT_READY;
            throw MediumException(OUTPUT_NOT_READY);
        }
    }

    unsigned int muxedVideoChunksOffset() const
    {
        return FFMPEGMuxerCommonImpl<Container>::mMuxedChunksHelper.offset;
    }

private:
    
    void ctorCommon()
    {
        this->mMuxVideo = true;
        FFMPEGMuxerCommonImpl<Container>::mVideoStreamIndex = 0;
        FFMPEGMuxerCommonImpl<Container>::mMuxedChunksHelper.needsVideoKeyFrame = true; 
        FFMPEGMuxerCommonImpl<Container>::completeMuxerInitialization();
        auto freeNothing = [](unsigned char* buffer) {  };
        mMuxedVideoChunks.resize(FFMPEGMuxerCommonImpl<Container>::mMuxedChunksHelper.data.size());
        unsigned int n;
        for (n = 0; n < mMuxedVideoChunks.size(); n++)
        {
            ShareableMuxedData shMuxedData(this->mMuxedChunksHelper.data[n].ptr, freeNothing);
            mMuxedVideoChunks[n].assignDataSharedPtr(shMuxedData);
        }
        Medium::mInputStatus = READY;    
    }
    
    std::vector<MuxedVideoData<Container, VideoCodecOrFormat, width, height> > mMuxedVideoChunks;

};

}

#endif // FFMPEGMuxerVideoImpl_HPP_INCLUDED

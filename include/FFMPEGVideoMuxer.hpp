/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef FFMPEGMuxerVideoImpl_HPP_INCLUDED
#define FFMPEGMuxerVideoImpl_HPP_INCLUDED

#include "FFMPEGMuxer.hpp"

namespace laav
{

// DIAMOND pattern
template <typename Container, typename VideoCodecOrFormat, unsigned int width, unsigned int height>
class FFMPEGMuxerVideoImpl : public virtual FFMPEGMuxerCommonImpl<Container>
{

protected:

    FFMPEGMuxerVideoImpl(bool startMuxingSoon = false)
    {
        mVideoStream = avformat_new_stream(FFMPEGMuxerCommonImpl<Container>::mMuxerContext, NULL);
        if (!mVideoStream)
            printAndThrowUnrecoverableError("mVideoStream = avformat_new_stream(...)");

        mVideoStream->id = FFMPEGMuxerCommonImpl<Container>::mMuxerContext->nb_streams - 1;
        AVCodec* videoCodec = avcodec_find_encoder(FFMPEGUtils::translateCodec<VideoCodecOrFormat>());
        if (!videoCodec)
            printAndThrowUnrecoverableError("AVCodec* videoCodec = avcodec_find_encoder(...)");

        mVideoCodecContext = avcodec_alloc_context3(videoCodec);
        if (!mVideoCodecContext)
            printAndThrowUnrecoverableError("mVideoCodecContext = avcodec_alloc_context3(...)");

        mVideoCodecContext->codec_id = FFMPEGUtils::translateCodec<VideoCodecOrFormat>();;
        mVideoCodecContext->width = width;
        mVideoCodecContext->height = height;
        mVideoCodecContext->time_base = AV_TIME_BASE_Q;
        avcodec_parameters_from_context(mVideoStream->codecpar, mVideoCodecContext);

        unsigned int i;
        for (i = 1; i < encodedVideoFrameBufferSize; i++)
        {
            AVPacket videoPkt;
            this->mVideoAVPktsToMux.push_back(videoPkt);
        }

        for (i = 1; i < encodedVideoFrameBufferSize; i++)
        {
            av_init_packet(&this->mVideoAVPktsToMux[i]);
            this->mVideoAVPktsToMux[i].pts = AV_NOPTS_VALUE;
        }

        if (startMuxingSoon)
            this->startMuxing();

    }

    ~FFMPEGMuxerVideoImpl()
    {
        avcodec_free_context(&mVideoCodecContext);
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    void takeMuxableFrame(const VideoFrame<VideoCodecOrFormat, width, height>& videoFrameToMux)
    {
        if (isFrameEmpty(videoFrameToMux))
            throw MediaException(MEDIA_NO_DATA);

        FFMPEGMuxerCommonImpl<Container>::mVideoReady = true;

        int64_t pts;
        pts = av_rescale_q(videoFrameToMux.monotonicTimestamp(), mVideoCodecContext->time_base,
                           this->mMuxerContext->streams[this->mVideoStreamIndex]->time_base);

        this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset].pts = pts;
        this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset].dts = pts;

        this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset].stream_index =
        FFMPEGMuxerCommonImpl<Container>::mVideoStreamIndex;
        //TODO: remove cast ?
        this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset].data =
        (uint8_t*)videoFrameToMux.data();

        this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset].size = videoFrameToMux.size();
        this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset].flags = videoFrameToMux.mLibAVFlags;
        // TODO: is this needed for audio too?
        if (this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset].side_data)
        {
            this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset].side_data =
            videoFrameToMux.mLibAVSideData;

            this->mVideoAVPktsToMux[this->mVideoAVPktsToMuxOffset].side_data_elems =
            videoFrameToMux.mLibAVSideDataElems;
        }

        this->mVideoAVPktsToMuxOffset =
        (this->mVideoAVPktsToMuxOffset + 1) % this->mVideoAVPktsToMux.size();

        if (this->mDoMux)
        {
            this->mMuxedChunks.newGroupOfChunks = false;
            this->muxNextUsefulFrameFromBuffer(true);
        }
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

    bool isFrameEmpty(const EncodedVideoFrame& videoFrame) const
    {
        return videoFrame.size() == 0;
    }

    AVStream* mVideoStream;
    AVCodecContext* mVideoCodecContext0;
    AVCodecContext* mVideoCodecContext;

};

template <typename Container, typename VideoCodecOrFormat, unsigned int width, unsigned int height>
class FFMPEGVideoMuxer : public FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, width, height>
{
public:

    FFMPEGVideoMuxer(bool startMuxingSoon = false) :
    FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, width, height>(startMuxingSoon)
    {
        this->mMuxVideo = true;
        FFMPEGMuxerCommonImpl<Container>::mVideoStreamIndex = 0;
        FFMPEGMuxerCommonImpl<Container>::completeMuxerInitialization();
        auto freeNothing = [](unsigned char* buffer) {  };
        mMuxedVideoChunks.resize(FFMPEGMuxerCommonImpl<Container>::mMuxedChunks.data.size());
        unsigned int n;
        for (n = 0; n < mMuxedVideoChunks.size(); n++)
        {
            ShareableMuxedData shMuxedData(this->mMuxedChunks.data[n].ptr, freeNothing);
            mMuxedVideoChunks[n].assignDataSharedPtr(shMuxedData);
        }
    }

    void takeMuxableFrame(const VideoFrame<VideoCodecOrFormat, width, height>& videoFrameToMux)
    {
        FFMPEGMuxerVideoImpl<Container,
                             VideoCodecOrFormat, width, height>::takeMuxableFrame(videoFrameToMux);
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    const std::vector<MuxedVideoData<Container, VideoCodecOrFormat, width, height> >&
    muxedVideoChunks()
    {
        if (this->mMuxedChunks.newGroupOfChunks)
        {
            unsigned int n;
            for (n = 0; n < FFMPEGMuxerCommonImpl<Container>::mMuxedChunks.offset; n++)
            {
                mMuxedVideoChunks[n].setSize(this->mMuxedChunks.data[n].size);
            }
            return mMuxedVideoChunks;
        }
        else
        {
            throw MediaException(MEDIA_NO_DATA);
        }
    }

    unsigned int muxedVideoChunksOffset() const
    {
        return FFMPEGMuxerCommonImpl<Container>::mMuxedChunks.offset;
    }

private:

    std::vector<MuxedVideoData<Container, VideoCodecOrFormat, width, height> > mMuxedVideoChunks;

};

}

#endif // FFMPEGMuxerVideoImpl_HPP_INCLUDED

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

#ifndef FFMPEGMuxerAudioImpl_HPP_INCLUDED
#define FFMPEGMuxerAudioImpl_HPP_INCLUDED

#include "FFMPEGMuxer.hpp"

namespace laav
{

// DIAMOND pattern
template <typename Container,
          typename AudioCodecOrFormat,
          unsigned int audioSampleRate,
          enum AudioChannels audioChannels>
class FFMPEGMuxerAudioImpl : public virtual FFMPEGMuxerCommonImpl<Container>
{

protected:

    FFMPEGMuxerAudioImpl(bool startMuxingSoon = false)
    {
        mAudioStream = avformat_new_stream(FFMPEGMuxerCommonImpl<Container>::mMuxerContext, NULL);
        if (!mAudioStream)
            printAndThrowUnrecoverableError("mAudioStream = avformat_new_stream(...)");
        mAudioStream->id = FFMPEGMuxerCommonImpl<Container>::mMuxerContext->nb_streams - 1;

        AVCodec* audioCodec = avcodec_find_encoder(FFMPEGUtils::translateCodec<AudioCodecOrFormat>());
        if (!audioCodec)
            printAndThrowUnrecoverableError("AVCodec* audioCodec = avcodec_find_encoder(...);");

        mAudioCodecContext = avcodec_alloc_context3(audioCodec);
        if (!mAudioCodecContext)
            printAndThrowUnrecoverableError("mAudioCodecContext = avcodec_alloc_context3(audioCodec)");

        mAudioCodecContext->codec_id =  FFMPEGUtils::translateCodec<AudioCodecOrFormat>();
        mAudioCodecContext->sample_rate = audioSampleRate;
        mAudioCodecContext->channels = audioChannels + 1;
        mAudioCodecContext->time_base = (AVRational)
        {
            1, audioSampleRate
        };

        mAudioCodecContext->channel_layout = audioChannels + 1 == 1 ?
            AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;

        mAudioCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
        // THIS is needed by MATROSKA muxer, but not needed by MPEGTS...
        // Note that AAC only supports AV_SAMPLE_FMT_FLTP (see audioCodec->sample_fmts results)
        if (FFMPEGUtils::translateCodec<AudioCodecOrFormat>() == AV_CODEC_ID_AAC)
        {
            mAudioCodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
            avcodec_open2(mAudioCodecContext, audioCodec, NULL);
        }

        avcodec_parameters_from_context(mAudioStream->codecpar, mAudioCodecContext);

        unsigned int i;
        for (i = 1; i < encodedAudioFrameBufferSize; i++)
        {
            AVPacket audioPkt;
            this->mAudioAVPktsToMux.push_back(audioPkt);
        }

        for (i = 1; i < encodedAudioFrameBufferSize; i++)
        {
            av_init_packet(&this->mAudioAVPktsToMux[i]);
            this->mAudioAVPktsToMux[i].pts = AV_NOPTS_VALUE;
        }

        if (startMuxingSoon)
            this->startMuxing();
    }

    ~FFMPEGMuxerAudioImpl()
    {
        avcodec_free_context(&mAudioCodecContext);
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    void takeMuxableFrame(const AudioFrame<AudioCodecOrFormat,
                          audioSampleRate, audioChannels>& audioFrameToMux)
    {
        if (isFrameEmpty(audioFrameToMux))
            throw MediaException(MEDIA_NO_DATA);

        FFMPEGMuxerCommonImpl<Container>::mAudioReady = true;

        int64_t pts =
        av_rescale_q (audioFrameToMux.monotonicTimestamp(), mAudioCodecContext->time_base,
        this->mMuxerContext->streams[this->mAudioStreamIndex]->time_base);

        this->mAudioAVPktsToMux[this->mAudioAVPktsToMuxOffset].pts = pts;
        this->mAudioAVPktsToMux[this->mAudioAVPktsToMuxOffset].dts = pts;
        this->mAudioAVPktsToMux[this->mAudioAVPktsToMuxOffset].stream_index = this->mAudioStreamIndex;
        // TODO: static cast?
        this->mAudioAVPktsToMux[this->mAudioAVPktsToMuxOffset].data = (uint8_t* )audioFrameToMux.data();
        this->mAudioAVPktsToMux[this->mAudioAVPktsToMuxOffset].size = audioFrameToMux.size();
        this->mAudioAVPktsToMux[this->mAudioAVPktsToMuxOffset].flags = audioFrameToMux.mLibAVFlags;

        this->mAudioAVPktsToMuxOffset =
        (this->mAudioAVPktsToMuxOffset + 1) % this->mAudioAVPktsToMux.size();

        if (this->mDoMux)
        {
            this->mMuxedChunks.newGroupOfChunks = false;
            this->muxNextUsefulFrameFromBuffer(true);
        }
    }

private:

    bool isFrameEmpty(const EncodedAudioFrame<AudioCodecOrFormat>& audioFrame)
    {
        return audioFrame.size() == 0;
    }

    bool isFrameEmpty(const PackedRawAudioFrame& audioFrame)
    {
        return audioFrame.size() == 0;
    }

    bool isFrameEmpty(const Planar2RawAudioFrame& audioFrame)
    {
        return audioFrame.size<0>() == 0;
    }

    AVStream* mAudioStream;
    AVCodecContext* mAudioCodecContext0;
    AVCodecContext* mAudioCodecContext;

};

template <typename Container, typename AudioCodecOrFormat,
          unsigned int audioSampleRate, enum AudioChannels audioChannels>
class FFMPEGAudioMuxer : public FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat,
                                                     audioSampleRate, audioChannels>
{
public:

    FFMPEGAudioMuxer(bool startMuxingSoon = false) :
    FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat, audioSampleRate, audioChannels>(startMuxingSoon)
    {
        this->mMuxAudio = true;
        FFMPEGMuxerCommonImpl<Container>::mAudioStreamIndex = 0;
        FFMPEGMuxerCommonImpl<Container>::completeMuxerInitialization();
        auto freeNothing = [](unsigned char* buffer) {  };
        mMuxedAudioChunks.resize(FFMPEGMuxerCommonImpl<Container>::mMuxedChunks.data.size());
        unsigned int n;
        for (n = 0; n < mMuxedAudioChunks.size(); n++)
        {
            ShareableMuxedData shMuxedData(this->mMuxedChunks.data[n].ptr, freeNothing);
            mMuxedAudioChunks[n].assignDataSharedPtr(shMuxedData);
        }
    }

    void takeMuxableFrame(const AudioFrame<AudioCodecOrFormat,
                          audioSampleRate,
                          audioChannels>& audioFrameToMux)
    {
        FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat,
                             audioSampleRate, audioChannels>::takeMuxableFrame(audioFrameToMux);
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    const std::vector<MuxedAudioData<Container, AudioCodecOrFormat,
                                     audioSampleRate, audioChannels> >& muxedAudioChunks()
    {
        if (FFMPEGMuxerCommonImpl<Container>::mMuxedChunks.offset != 0)
        {
            unsigned int n;
            for (n = 0; n < FFMPEGMuxerCommonImpl<Container>::mMuxedChunks.offset; n++)
            {
                mMuxedAudioChunks[n].setSize(this->mMuxedChunks.data[n].size);
            }
            return mMuxedAudioChunks;
        }
        else
        {
            throw MediaException(MEDIA_NO_DATA);
        }
    }

    unsigned int muxedAudioChunksOffset() const
    {
        return FFMPEGMuxerCommonImpl<Container>::mMuxedChunks.offset;
    }

private:

    std::vector<MuxedAudioData<Container, AudioCodecOrFormat,
                               audioSampleRate, audioChannels> > mMuxedAudioChunks;

};

}

#endif // FFMPEGMuxerAudioImpl_HPP_INCLUDED

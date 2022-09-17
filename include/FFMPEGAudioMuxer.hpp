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

template <typename PCMSoundFormat, typename AudioCodec,
          typename audioSampleRate, typename audioChannels>  
class FFMPEGAudioEncoder;
    
// DIAMOND pattern
template <typename Container,
          typename AudioCodecOrFormat,
          typename audioSampleRate,
          typename audioChannels>
class FFMPEGMuxerAudioImpl : public virtual FFMPEGMuxerCommonImpl<Container>
{

protected:

    FFMPEGMuxerAudioImpl(const std::string& id = "") :
        FFMPEGMuxerCommonImpl<Container>(id)
    {
        
        Medium::mId = id;        
        
        mAudioStream = avformat_new_stream(FFMPEGMuxerCommonImpl<Container>::mMuxerContext, NULL);
        if (!mAudioStream)
            printAndThrowUnrecoverableError("mAudioStream = avformat_new_stream(...)");
        mAudioStream->id = FFMPEGMuxerCommonImpl<Container>::mMuxerContext->nb_streams - 1;

        unsigned int i;
        for (i = 1; i < encodedAudioFrameBufferSize; i++)
        {
            AVPacket* audioPkt = av_packet_alloc();
            this->mAudioAVPktsToMux.push_back(audioPkt);
        }

        for (i = 1; i < encodedAudioFrameBufferSize; i++)
        {
            //av_init_packet(&this->mAudioAVPktsToMux[i]);
            this->mAudioAVPktsToMux[i]->pts = AV_NOPTS_VALUE;
        }

        //if (startMuxingSoon)
        //    this->startMuxing();
    }

    ~FFMPEGMuxerAudioImpl()
    {
        unsigned int i;
        for (i = 1; i < encodedAudioFrameBufferSize; i++)
            av_packet_free(&this->mAudioAVPktsToMux[i]);
    }

    /*!
     *  \exception MediumException
     */
    void mux(const AudioFrame<AudioCodecOrFormat,
                          audioSampleRate, audioChannels>& audioFrameToMux)
    {
        if (audioFrameToMux.isEmpty())
            throw MediumException(INPUT_EMPTY);

        Medium::mInputAudioFrameFactoryId = audioFrameToMux.mFactoryId;
        Medium::mDistanceFromInputAudioFrameFactory = audioFrameToMux.mDistanceFromFactory + 1;
        
        if (Medium::mInputStatus == PAUSED)
            throw MediumException(MEDIUM_PAUSED);
        
        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);
                   
        FFMPEGMuxerCommonImpl<Container>::mAudioReady = true;
        AVRational tb = (AVRational){ 1, audioSampleRate::value };
        int64_t pts =
        av_rescale_q (audioFrameToMux.monotonicTimestamp(), tb,
        this->mMuxerContext->streams[this->mAudioStreamIndex]->time_base);

        this->mAudioAVPktsToMux[this->mAudioAVPktsToMuxOffset]->pts = pts;
        this->mAudioAVPktsToMux[this->mAudioAVPktsToMuxOffset]->dts = pts;
        this->mAudioAVPktsToMux[this->mAudioAVPktsToMuxOffset]->stream_index = this->mAudioStreamIndex;
        // TODO: static cast?

        unsigned offset = 0;
        if (FFMPEGUtils::translateCodec<AudioCodecOrFormat>() == AV_CODEC_ID_AAC && !std::is_same<Container, MPEGTS>::value)
            offset = 7;
        uint8_t* data = (uint8_t* )audioFrameToMux.data();
        this->mAudioAVPktsToMux[this->mAudioAVPktsToMuxOffset]->data = &data[offset];
        this->mAudioAVPktsToMux[this->mAudioAVPktsToMuxOffset]->size = audioFrameToMux.size() - offset;
        this->mAudioAVPktsToMux[this->mAudioAVPktsToMuxOffset]->flags = audioFrameToMux.mLibAVFlags;

        this->mAudioAVPktsToMuxOffset =
        (this->mAudioAVPktsToMuxOffset + 1) % (this->mAudioAVPktsToMux.size() - offset);

        //if (this->mDoMux)
        {
            this->mMuxedChunksHelper.newGroupOfChunks = false;
            this->muxNextUsefulFrameFromBuffer(true);
        }
    }

    void setParamsFromEmbeddedAudioCodecContext()
    {
        
        const AVCodec* audioCodec = avcodec_find_encoder(FFMPEGUtils::translateCodec<AudioCodecOrFormat>());
        if (!audioCodec)
            printAndThrowUnrecoverableError("AVCodec* audioCodec = avcodec_find_encoder(...);");

        AVCodecContext* audioCodecContext = avcodec_alloc_context3(audioCodec);
        if (!audioCodecContext)
            printAndThrowUnrecoverableError("audioCodecContext = avcodec_alloc_context3(audioCodec)");

        audioCodecContext->codec_id =  FFMPEGUtils::translateCodec<AudioCodecOrFormat>();
        audioCodecContext->sample_rate = audioSampleRate::value;
        audioCodecContext->channels = audioChannels::value + 1;
        audioCodecContext->time_base = (AVRational){1, audioSampleRate::value};

        audioCodecContext->channel_layout = audioChannels::value + 1 == 1 ?
            AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;

        audioCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
        avcodec_parameters_from_context(mAudioStream->codecpar, audioCodecContext);        
        avcodec_free_context(&audioCodecContext); 
        
    }    
        
    AVStream* mAudioStream;

};

template <typename Container, typename AudioCodecOrFormat,
          typename audioSampleRate, typename audioChannels>
class FFMPEGAudioMuxer : public FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat,
                                                     audioSampleRate, audioChannels>
{
public:

    FFMPEGAudioMuxer(const std::string& id = "") :
    FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat, audioSampleRate, audioChannels>(id)
    {
        static_assert(! (std::is_same<Container, MATROSKA>::value), 
                      "Please pass the audio Encoder to the constructor, as needed by MATROSKA muxer!");        
        FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat, audioSampleRate, audioChannels>::setParamsFromEmbeddedAudioCodecContext();
        ctorCommon();
    }
    
    template <typename PCMSoundFormat>  
    FFMPEGAudioMuxer(const FFMPEGAudioEncoder<PCMSoundFormat, AudioCodecOrFormat,
                                              audioSampleRate, audioChannels>& aEnc,
                                              const std::string& id = "") :
    FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat, audioSampleRate, audioChannels>(id)
    {
        avcodec_parameters_from_context(FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat, audioSampleRate, audioChannels>::mAudioStream->codecpar, 
                                        aEnc.mAudioEncoderCodecContext);        
        ctorCommon();
    }    
    
    /*!
     *  \exception MediumException
     */        
    void mux(const AudioFrame<AudioCodecOrFormat,
                              audioSampleRate, audioChannels>& audioFrameToMux)
    {         
        FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat,
                             audioSampleRate, audioChannels>::mux(audioFrameToMux);
    }    

    /*!
     *  \exception MediumException
     */
    const std::vector<MuxedAudioData<Container, AudioCodecOrFormat,
                                     audioSampleRate, audioChannels> >& muxedAudioChunks()
    {
        if (Medium::mInputStatus == PAUSED)
            throw MediumException(MEDIUM_PAUSED);
        
        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);
        
        if (FFMPEGMuxerCommonImpl<Container>::mMuxedChunksHelper.offset != 0)
        {
            unsigned int n;
            for (n = 0; n < FFMPEGMuxerCommonImpl<Container>::mMuxedChunksHelper.offset; n++)
            {
                mMuxedAudioChunks[n].setSize(this->mMuxedChunksHelper.data[n].size);
            }
            Medium::mOutputStatus = READY;            
            return mMuxedAudioChunks;
        }
        else
        {
            Medium::mOutputStatus = NOT_READY;
            throw MediumException(OUTPUT_NOT_READY);
        }
    }

    unsigned int muxedAudioChunksOffset() const
    {
        return FFMPEGMuxerCommonImpl<Container>::mMuxedChunksHelper.offset;
    }

private:
    
    void ctorCommon()
    {
        this->mMuxAudio = true;
        FFMPEGMuxerCommonImpl<Container>::mAudioStreamIndex = 0;
        FFMPEGMuxerCommonImpl<Container>::completeMuxerInitialization();
        auto freeNothing = [](unsigned char* buffer) {  };
        mMuxedAudioChunks.resize(FFMPEGMuxerCommonImpl<Container>::mMuxedChunksHelper.data.size());
        unsigned int n;
        for (n = 0; n < mMuxedAudioChunks.size(); n++)
        {
            ShareableMuxedData shMuxedData(this->mMuxedChunksHelper.data[n].ptr, freeNothing);
            mMuxedAudioChunks[n].assignDataSharedPtr(shMuxedData);
        }
        if (std::is_same<Container, MATROSKA>::value)
            FFMPEGMuxerCommonImpl<Container>::mContainerFileExtension = "mka";
        Medium::mInputStatus = READY;        
    }
    
    std::vector<MuxedAudioData<Container, AudioCodecOrFormat,
                               audioSampleRate, audioChannels> > mMuxedAudioChunks;

};

}

#endif // FFMPEGMuxerAudioImpl_HPP_INCLUDED

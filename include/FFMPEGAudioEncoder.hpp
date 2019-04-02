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

#ifndef FFMPEGAUDIOENCODER_HPP_INCLUDED
#define FFMPEGAUDIOENCODER_HPP_INCLUDED

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include "FFMPEGCommon.hpp"
#include "HTTPAudioVideoStreamer.hpp"
#include "HTTPAudioStreamer.hpp"
#include "UDPAudioVideoStreamer.hpp"
#include "UDPAudioStreamer.hpp"

namespace laav
{

template <typename AudioCodec, typename audioSampleRate, typename audioChannels>
class AudioFrameHolder;

template <typename Container,
          typename AudioCodec, 
          typename audioSampleRate, 
          typename audioChannels>
class HTTPAudioStreamer;    

template <typename Container,
          typename AudioCodec, 
          typename audioSampleRate, 
          typename audioChannels>
class FFMPEGAudioMuxer;

template <typename Container,
          typename AudioCodec, 
          typename audioSampleRate, 
          typename audioChannels>
class UDPAudioStreamer;

template <typename Container,
          typename VideoCodecOrFormat,
          typename width,
          typename height,
          typename AudioCodec,
          typename audioSampleRate,
          typename audioChannels>
class HTTPAudioVideoStreamer;

template <typename Container,
          typename VideoCodecOrFormat,
          typename width,
          typename height,
          typename AudioCodec,
          typename audioSampleRate,
          typename audioChannels>
class FFMPEGAudioVideoMuxer;

template <typename Container,
          typename VideoCodecOrFormat,
          typename width,
          typename height,
          typename AudioCodec,
          typename audioSampleRate,
          typename audioChannels>
class UDPAudioVideoStreamer;

template <typename PCMSoundFormat, typename AudioCodec,
          typename audioSampleRate, typename audioChannels>
class FFMPEGAudioEncoder : public Medium
{

// accesses the mVideoCodecContext member for copying the parameters needed by some formats (i.e: matroska)
template <typename Container,
          typename AudioCodec_, 
          typename audioSampleRate_, 
          typename audioChannels_>
friend class FFMPEGAudioMuxer;   

template <typename Container, typename VideoCodecOrFormat,
          typename width, typename height,
          typename AudioCodec_,
          typename audioSampleRate_, typename audioChannels_>
friend class FFMPEGAudioVideoMuxer;    
    
public:

    /*!
     *  \exception MediumException
     */    
    void encode(const AudioFrame<PCMSoundFormat, audioSampleRate, audioChannels>& rawAudioFrame)
    {
        
        if (rawAudioFrame.isEmpty())
            throw MediumException(INPUT_EMPTY);

        if (Medium::mInputStatus ==  PAUSED)
            throw MediumException(MEDIUM_PAUSED);
        
        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);        

        unsigned int bufferSize = mRawInputLibAVFrame->linesize[0];
        unsigned int periodSize = getPeriodSize(rawAudioFrame);

        if ((mRawInputLibAVFrameBufferOffset + periodSize) >= bufferSize)
        {

            copyRawAudioFrameDataToLibAVFrameData(mRawInputLibAVFrameBufferOffset,
                                                  rawAudioFrame,
                                                  (bufferSize - mRawInputLibAVFrameBufferOffset));

            if (mEncodingStartTime == 0)
            {
                mEncodingStartTime = av_rescale_q(av_gettime_relative(),
                                                  AV_TIME_BASE_Q,
                                                  mAudioEncoderCodecContext->time_base);
            }
                
            int64_t monotonicNow = av_rescale_q(av_gettime_relative(), AV_TIME_BASE_Q,
                                                mAudioEncoderCodecContext->time_base);

            int64_t diff = monotonicNow -  mFrameCounter;
            mEncodingCurrTime = monotonicNow;// - mEncodingStartTime;
            
            if(FFMPEGUtils::translateCodec<AudioCodec>() == AV_CODEC_ID_OPUS)
            {   
                // Reset the pts (in case of a reconnection of the dev)
                // TODO: use a more precise value
                if (diff == 0 || diff >= 2000)
                    mFrameCounter = monotonicNow;
                
                this->mRawInputLibAVFrame->pts = mFrameCounter;
                monotonicNow = mFrameCounter;
                mEncodingCurrTime = monotonicNow;
                mFrameCounter += mRawInputLibAVFrame->nb_samples;                 
            }
            else
                this->mRawInputLibAVFrame->pts = mEncodingCurrTime;
            
            int ret = avcodec_send_frame(this->mAudioEncoderCodecContext, this->mRawInputLibAVFrame);
            if (ret == AVERROR(EAGAIN))
            {
                return;
            }
            else if (ret != 0)
                printAndThrowUnrecoverableError("avcodec_send_frame(...)");

            struct timespec dateTimeNow;
            // TODO ifdef linux
            clock_gettime(CLOCK_REALTIME, &dateTimeNow);

            mEncodingDateTs[mEncodedAVPacketBufferOffset] =
            dateTimeNow.tv_sec * 1000000000 + dateTimeNow.tv_nsec;

            mEncodingMonotonicTs[mEncodedAVPacketBufferOffset] = monotonicNow;
            AVPacket& currPkt = mEncodedAudioPktBuffer[mEncodedAVPacketBufferOffset];

            int ret2 = avcodec_receive_packet(this->mAudioEncoderCodecContext, &currPkt);
            
            // Buffering
            if (ret2 == AVERROR(EAGAIN))
            {                 
                mEncodedAVPacketBufferOffset =
                (mEncodedAVPacketBufferOffset + 1) % mEncodedAudioFrameBuffer.size();
                return;
            }
            else if (ret2 != 0)
                printAndThrowUnrecoverableError("avcodec_receive_packet(...)");
            else
            {
                Medium::mOutputStatus = READY;
                mLibavNeedsToBuffer = false;
                AudioFrame<AudioCodec, audioSampleRate, audioChannels>& currFrame =
                mEncodedAudioFrameBuffer[mEncodedAudioFrameBufferOffset];
                
                AVPacket& encodedPkt = currPkt; //mEncodedAudioPktBuffer[mEncodedAudioFrameBufferOffset];

                if (FFMPEGUtils::translateCodec<AudioCodec>() == AV_CODEC_ID_AAC)
                {
                    if (!mAdtsHeaderWritten)
                    {
                        if (avformat_write_header(mAdtsMuxerContext, NULL) < 0)
                            printAndThrowUnrecoverableError("avformat_write_header(...)");

                        mAdtsHeaderWritten = true;
                    }
                    mEncodedFrameForAdtsMuxer = &currFrame;
                    mEncodedFrameForAdtsMuxer->mFactoryId = Medium::mId;
                    mEncodedFrameForAdtsMuxer->mDistanceFromFactory =
                    Medium::mDistanceFromInputAudioFrameFactory;
                    int64_t pts = av_rescale_q(encodedPkt.pts, mAudioEncoderCodecContext->time_base, mAdtsMuxerContext->streams[0]->time_base);

                    encodedPkt.pts = encodedPkt.dts = pts;
                    if (av_write_frame(mAdtsMuxerContext, &encodedPkt) < 0)
                        printAndThrowUnrecoverableError("av_write_frame(...)");
                }
                else
                {
                    fillEncodedAudioFrame(currFrame, encodedPkt);
                }

                currFrame.setMonotonicTimestamp(mEncodingMonotonicTs[mEncodedAudioFrameBufferOffset]);
                currFrame.setDateTimestamp(mEncodingDateTs[mEncodedAudioFrameBufferOffset]);
                currFrame.mLibAVFlags = encodedPkt.flags;
                currFrame.mLibAVSideData = encodedPkt.side_data;
                currFrame.mLibAVSideDataElems = encodedPkt.side_data_elems;                
                currFrame.mFactoryId = Medium::mId;
                Medium::mInputAudioFrameFactoryId = rawAudioFrame.mFactoryId;                
                Medium::mDistanceFromInputAudioFrameFactory = rawAudioFrame.mDistanceFromFactory + 1;
                currFrame.mDistanceFromFactory = 0;

                if (mEncodedAudioFrameBufferOffset + 1 == mEncodedAudioFrameBuffer.size())
                {
                    mFillingEncodedAudioFrameBuffer = false;
                }

                mEncodedAudioFrameBufferOffset =
                (mEncodedAudioFrameBufferOffset + 1) % mEncodedAudioFrameBuffer.size();

                mEncodedAVPacketBufferOffset =
                (mEncodedAVPacketBufferOffset + 1) % mEncodedAudioFrameBuffer.size();

                if ((mRawInputLibAVFrameBufferOffset + periodSize) % bufferSize != 0)
                {                  
                    copyRawAudioFrameDataToLibAVFrameData
                    (0, rawAudioFrame, (mRawInputLibAVFrameBufferOffset + periodSize) % bufferSize, periodSize - (mRawInputLibAVFrameBufferOffset + periodSize) % bufferSize);
                }

                mFillingEncodedAudioFrame = false;
            }
        }
        else
        {
            mFillingEncodedAudioFrame = true;        
            copyRawAudioFrameDataToLibAVFrameData(mRawInputLibAVFrameBufferOffset,
                                                  rawAudioFrame, periodSize);
        }

        mRawInputLibAVFrameBufferOffset = (mRawInputLibAVFrameBufferOffset + periodSize) % bufferSize;
    }

    AudioFrameHolder<AudioCodec, audioSampleRate, audioChannels>&
    operator >> (AudioFrameHolder<AudioCodec, audioSampleRate, audioChannels>& audioFrameHolder)
    {
        Medium& m = audioFrameHolder;
        setDistanceFromInputAudioFrameFactory(m, 1);        
        setInputAudioFrameFactoryId(m, mId);          
        setStatusInPipe(m, READY);  
        
        try
        {           
            audioFrameHolder.hold(lastEncodedFrame());          
        }
        catch (const MediumException& me)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return audioFrameHolder;
    }

    template <typename Container, typename VideoCodec, typename width, typename height >
    FFMPEGAudioVideoMuxer<Container, VideoCodec, width, height,
                          AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioVideoMuxer<Container, VideoCodec, width, height,
                           AudioCodec, audioSampleRate, audioChannels>& audioVideoMuxer)
    {
        Medium& m = audioVideoMuxer;
        setDistanceFromInputAudioFrameFactory(m, 1);        
        setInputAudioFrameFactoryId(m, mId);        
        setStatusInPipe(m, READY);
        
        try
        {
            audioVideoMuxer.mux(lastEncodedFrame());            
        }
        catch (const MediumException& me)
        {
            setStatusInPipe(m, NOT_READY);            
        }

        return audioVideoMuxer;
    }

    template <typename Container, typename VideoCodec, typename width, typename height>
    HTTPAudioVideoStreamer<Container, VideoCodec, width, height,
                           AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (HTTPAudioVideoStreamer<Container, VideoCodec, width, height,
                            AudioCodec, audioSampleRate, audioChannels>& httpAudioVideoStreamer)
    {
        Medium& m = httpAudioVideoStreamer;
        setDistanceFromInputAudioFrameFactory(m, 1);        
        setInputAudioFrameFactoryId(m, mId);        
        setStatusInPipe(m, READY);
        
        try
        {
            httpAudioVideoStreamer.takeStreamableFrame(lastEncodedFrame());
            httpAudioVideoStreamer.streamMuxedData();            
        }
        catch (const MediumException& me)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        
        return httpAudioVideoStreamer;
    }

    template <typename Container>
    HTTPAudioStreamer<Container, AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (HTTPAudioStreamer<Container, AudioCodec, audioSampleRate, audioChannels>& httpAudioStreamer)
    {
        Medium& m = httpAudioStreamer;
        setDistanceFromInputAudioFrameFactory(m, 1);        
        setInputAudioFrameFactoryId(m, mId); 
        setStatusInPipe(m, READY);
        
        try
        {
            httpAudioStreamer.takeStreamableFrame(lastEncodedFrame());
            httpAudioStreamer.streamMuxedData();           
        }
        catch (const MediumException& me)
        {
            setStatusInPipe(m, NOT_READY);            
        }

        return httpAudioStreamer;
    }

    template <typename Container, typename VideoCodec, typename width, typename height>
    UDPAudioVideoStreamer<Container, VideoCodec, width, height,
                           AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (UDPAudioVideoStreamer<Container, VideoCodec, width, height,
                            AudioCodec, audioSampleRate, audioChannels>& udpAudioVideoStreamer)
    {
        Medium& m = udpAudioVideoStreamer;
        setDistanceFromInputAudioFrameFactory(m, 1);        
        setInputAudioFrameFactoryId(m, mId);         
        setStatusInPipe(m, READY);
        
        try
        {
            udpAudioVideoStreamer.takeStreamableFrame(lastEncodedFrame());
            udpAudioVideoStreamer.streamMuxedData();
        
        }
        catch (const MediumException& mediaException)
        {
            setStatusInPipe(m, NOT_READY);            
        }

        return udpAudioVideoStreamer;
    }

    template <typename Container>
    UDPAudioStreamer<Container, AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (UDPAudioStreamer<Container, AudioCodec, audioSampleRate, audioChannels>& udpAudioStreamer)
    {
        Medium& m = udpAudioStreamer;
        setDistanceFromInputAudioFrameFactory(m, 1);        
        setInputAudioFrameFactoryId(m, mId); 
        setStatusInPipe(m, READY);
        
        try
        {
            udpAudioStreamer.takeStreamableFrame(lastEncodedFrame());
            udpAudioStreamer.streamMuxedData();           
        }
        catch (const MediumException& me)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        
        return udpAudioStreamer;
    }    
    
    template <typename Container>
    FFMPEGAudioMuxer<Container, AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioMuxer<Container, AudioCodec, audioSampleRate, audioChannels>& audioMuxer)
    {
        Medium& m = audioMuxer;
        setDistanceFromInputAudioFrameFactory(m, 1);        
        setInputAudioFrameFactoryId(m, mId);         
        setStatusInPipe(m, READY);
        
        try
        {
            audioMuxer.mux(lastEncodedFrame());           
        }
        catch (const MediumException& mediaException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        
        return audioMuxer;
    }

    
    /*!
     *  \exception MediumException
     */
    AudioFrame<AudioCodec, audioSampleRate, audioChannels>& encodedFrame(unsigned int bufferOffset)
    {
        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);        
        
        if (mFillingEncodedAudioFrame || mLibavNeedsToBuffer)
        {
            throw MediumException(MEDIUM_BUFFERING);
        }
        if (mFillingEncodedAudioFrameBuffer &&
            (bufferOffset % mEncodedAudioFrameBuffer.size() >= mEncodedAudioFrameBufferOffset))
        {
            throw MediumException(MEDIUM_BUFFERING);
        }
        else
        {
            return mEncodedAudioFrameBuffer[bufferOffset % mEncodedAudioFrameBuffer.size()];
        }
    }

    /*!
     *  \exception MediumException
     */
    AudioFrame<AudioCodec, audioSampleRate, audioChannels>& lastEncodedFrame()
    {
        return encodedFrame(
        (mEncodedAudioFrameBuffer.size() + mEncodedAudioFrameBufferOffset - 1) % mEncodedAudioFrameBuffer.size());
    }

    unsigned int encodedFrameBufferIndex() const
    {
        return mEncodedAudioFrameBufferOffset;
    }

    unsigned int encodedFrameBufferSize() const
    {
        return mEncodedAudioFrameBuffer.size();
    }

protected:

    FFMPEGAudioEncoder(const std::string& id = ""):
        Medium(id),
        mEncodingCurrTime(0),
        mCounter(0),
        mEncodingStartTime(0),
        mFillingEncodedAudioFrameBuffer(true),
        mFillingEncodedAudioFrame(true),
        mRawInputLibAVFrameBufferOffset(0),
        mEncodedAudioFrameBufferOffset(0),
        mEncodedAVPacketBufferOffset(0),
        mLibavNeedsToBuffer(true),
        mFrameCounter(0)
    {
        //av_register_all();
        //avcodec_register_all();
        
        mAudioCodec = avcodec_find_encoder(FFMPEGUtils::translateCodec<AudioCodec>() );
        if (mAudioCodec == NULL)
            printAndThrowUnrecoverableError("avcodec_find_encoder(...)");

        mAudioEncoderCodecContext = avcodec_alloc_context3(mAudioCodec);
        if (mAudioEncoderCodecContext == NULL)
            printAndThrowUnrecoverableError("mAudioEncoderCodecContext =avcodec_alloc_context3(...)");

        mAudioEncoderCodecContext->sample_fmt = FFMPEGUtils::translateSampleFormat<PCMSoundFormat>();
        mAudioEncoderCodecContext->sample_rate = audioSampleRate::value;
        mAudioEncoderCodecContext->channels = audioChannels::value + 1;
        mAudioEncoderCodecContext->channel_layout = av_get_default_channel_layout(audioChannels::value + 1);
        mAudioEncoderCodecContext->time_base = (AVRational){ 1, audioSampleRate::value };
        mAudioEncoderCodecContext->codec_type = AVMEDIA_TYPE_AUDIO ;
        if (FFMPEGUtils::translateCodec<AudioCodec>() == AV_CODEC_ID_AAC)
        {
            mAudioEncoderCodecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
        }
        Medium::mInputStatus = READY;
    }

    void completeEncoderInitialization()
    {
        if (avcodec_open2(mAudioEncoderCodecContext, mAudioCodec, NULL) < 0)
            printAndThrowUnrecoverableError("avcodec_open2(...)");

        mRawInputLibAVFrame = av_frame_alloc();
        if (!mRawInputLibAVFrame)
            printAndThrowUnrecoverableError("mRawInputLibAVFrame = av_frame_alloc()");

        mRawInputLibAVFrame->nb_samples     = mAudioEncoderCodecContext->frame_size;
        mRawInputLibAVFrame->format         = mAudioEncoderCodecContext->sample_fmt;
        mRawInputLibAVFrame->channels       = mAudioEncoderCodecContext->channels;
        mRawInputLibAVFrame->channel_layout = mAudioEncoderCodecContext->channel_layout;
        mRawInputLibAVFrame->sample_rate    = audioSampleRate::value;
        // Allocate frame buffer
        if (av_frame_get_buffer(mRawInputLibAVFrame, 0) < 0)
            printAndThrowUnrecoverableError("av_frame_get_buffer(mRawInputLibAVFrame, 0)");

        unsigned int i;
        for (i = 0; i < encodedAudioFrameBufferSize; i++)
        {
            AVPacket encodedAudioPkt;
            encodedAudioPkt.size = 0;
            mEncodedAudioPktBuffer.push_back(encodedAudioPkt);
            av_init_packet(&mEncodedAudioPktBuffer[i]);
            AudioFrame<AudioCodec, audioSampleRate, audioChannels> encodedAudioFrame;
            encodedAudioFrame.setMonotonicTimeBase(mAudioEncoderCodecContext->time_base);
            mEncodedAudioFrameBuffer.push_back(encodedAudioFrame);
            mEncodingMonotonicTs.push_back(AV_NOPTS_VALUE);
            mEncodingDateTs.push_back(-1);
        }

        // AAC ENCODER needs the ADTS header. FFMPEG wants a muxer for making the header.
        if (FFMPEGUtils::translateCodec<AudioCodec>() == AV_CODEC_ID_AAC)
        {

            mAdtsMuxerFormat = av_guess_format("adts", NULL, NULL);
            if (!mAdtsMuxerFormat)
                printAndThrowUnrecoverableError("mAdtsMuxerFormat = av_guess_format(\"adts\" ...)");

            mAdtsMuxerFormat->audio_codec = AV_CODEC_ID_AAC;

            avformat_alloc_output_context2(&mAdtsMuxerContext, mAdtsMuxerFormat, "dummy", NULL);
            if (!mAdtsMuxerContext)
                printAndThrowUnrecoverableError("avformat_alloc_output_context2(...)");

            size_t adtsMuxerAVIOContextBufferSize = 4096;
            mAdtsMuxerAVIOContextBuffer = (uint8_t* )av_malloc(adtsMuxerAVIOContextBufferSize);
            if (!mAdtsMuxerAVIOContextBuffer)
                printAndThrowUnrecoverableError("mAdtsMuxerAVIOContextBuffer = (uint8_t* )...");

            mAdtsMuxerAVIOContext = avio_alloc_context(mAdtsMuxerAVIOContextBuffer,
                                                       adtsMuxerAVIOContextBufferSize, 1, this,
                                                       NULL, &writeAdtsFrame, NULL);
            if (!mAdtsMuxerAVIOContext)
                printAndThrowUnrecoverableError("mAdtsMuxerAVIOContext = avio_alloc_context(...)");

            mAdtsMuxerContext->pb = mAdtsMuxerAVIOContext;
            mAdtsAudioStream = avformat_new_stream(mAdtsMuxerContext, NULL);
            if (!mAdtsAudioStream)
                printAndThrowUnrecoverableError("mAdtsAudioStream = avformat_new_stream(...)");

            mAdtsAudioStream->id = mAdtsMuxerContext->nb_streams - 1;
            avcodec_parameters_from_context(mAdtsAudioStream->codecpar, mAudioEncoderCodecContext);
            mAdtsHeaderWritten = false;
            unsigned int i;
            for (i = 0; i < encodedAudioFrameBufferSize; i++)
            {
                mAudioDataPtrs.push_back(new unsigned char[4096]);
            }
        }

    }

    ~FFMPEGAudioEncoder()
    {
        unsigned int q;
        for (q = 0; q < mEncodedAudioPktBuffer.size(); q++)
            av_packet_unref(&mEncodedAudioPktBuffer[q]);
        av_frame_free(&mRawInputLibAVFrame);
        avcodec_free_context(&mAudioEncoderCodecContext);

        if (FFMPEGUtils::translateCodec<AudioCodec>() == AV_CODEC_ID_AAC)
        {
            av_write_trailer(mAdtsMuxerContext);
            av_free(mAdtsMuxerAVIOContextBuffer);
            avformat_free_context(mAdtsMuxerContext);
            av_free(mAdtsMuxerAVIOContext);
            unsigned int i;
            for (i = 0; i < encodedAudioFrameBufferSize; i++)
                delete[](mAudioDataPtrs[i]);
        }
    }

    virtual void beforeEncodeCallback(){}
    
    virtual void afterEncodeCallback(){}      
    
    AVCodecContext* mAudioEncoderCodecContext;

private:

    bool isFrameEmpty(const PackedRawAudioFrame& audioFrame) const
    {
        return audioFrame.size() == 0;
    }

    bool isFrameEmpty(const Planar2RawAudioFrame& audioFrame) const
    {
        return audioFrame.size<0>() == 0;
    }

    unsigned int getPeriodSize(const Planar2RawAudioFrame& rawAudioFrame)
    {
        return rawAudioFrame.size<0>();
    }

    unsigned int getPeriodSize(const PackedRawAudioFrame& rawAudioFrame)
    {
        return rawAudioFrame.size();
    }
    
    void copyRawAudioFrameDataToLibAVFrameData(unsigned int libAVFrameBufferOffset,
                                               const Planar2RawAudioFrame& rawAudioFrame,
                                               unsigned int len, unsigned int rawAudioFrameOffs = 0)
    {
        unsigned int channels = audioChannels::value + 1;

        memcpy(mRawInputLibAVFrame->data[0] + libAVFrameBufferOffset,
               rawAudioFrame.plane<0>(), len);

        if (channels == 2)
            memcpy(mRawInputLibAVFrame->data[1] + libAVFrameBufferOffset,
                   rawAudioFrame.plane<1>(), len);
    }

    void copyRawAudioFrameDataToLibAVFrameData(unsigned int libAVFrameBufferOffset,
                                               const PackedRawAudioFrame& rawAudioFrame,
                                               unsigned int len, unsigned int rawAudioFrameOffs = 0)
    {
        memcpy(mRawInputLibAVFrame->data[0] + libAVFrameBufferOffset, &rawAudioFrame.data()[0+rawAudioFrameOffs], len);   
    }

    void fillEncodedAudioFrame(EncodedAudioFrame<AudioCodec>& encodedAudioFrame,
                               const AVPacket& encodedAvPacket)
    {
        auto freeNothing = [](unsigned char* buffer) {  };
        ShareableAudioFrameData mAudioData = ShareableAudioFrameData(encodedAvPacket.data, freeNothing);
        encodedAudioFrame.assignDataSharedPtr(mAudioData);
        encodedAudioFrame.setSize(encodedAvPacket.size);
    }

    AVCodec* mAudioCodec;
    int64_t mEncodingCurrTime;
    int mCounter;
    int64_t mEncodingStartTime;
    bool mFillingEncodedAudioFrameBuffer;
    bool mFillingEncodedAudioFrame;
    unsigned int mRawInputLibAVFrameBufferOffset;
    std::vector<AudioFrame<AudioCodec, audioSampleRate, audioChannels> > mEncodedAudioFrameBuffer;
    std::vector<AVPacket> mEncodedAudioPktBuffer;
    std::vector<int64_t> mEncodingMonotonicTs;
    std::vector<int64_t> mEncodingDateTs;
    unsigned int mEncodedAudioFrameBufferOffset;
    unsigned int mEncodedAVPacketBufferOffset;
    AVFrame* mRawInputLibAVFrame;

    // NEEDED FOR AAC ENCODER (ADTS CONTAINER)
    AVFormatContext* mAdtsMuxerContext;
    uint8_t* mAdtsMuxerAVIOContextBuffer;
    AVOutputFormat* mAdtsMuxerFormat;
    AVIOContext* mAdtsMuxerAVIOContext;
    AVStream* mAdtsAudioStream;
    bool mAdtsHeaderWritten;
    // TODO: use a vector for other encoders too?
    std::vector<unsigned char* > mAudioDataPtrs;

    static int writeAdtsFrame (void* opaque, uint8_t* muxedDataSink, int chunkSize)
    {

        if (chunkSize == 0)
            return 0;
        // TODO: static cast?

        FFMPEGAudioEncoder<PCMSoundFormat, AudioCodec, audioSampleRate, audioChannels>* audioEncoder =
        (FFMPEGAudioEncoder<PCMSoundFormat, AudioCodec, audioSampleRate, audioChannels>* )opaque;

        auto freeNothing = [](unsigned char* buffer) {  };

        memcpy(audioEncoder->mAudioDataPtrs[audioEncoder->mEncodedAudioFrameBufferOffset],
               muxedDataSink, chunkSize);

        ShareableAudioFrameData mAudioData = ShareableAudioFrameData
        (audioEncoder->mAudioDataPtrs[audioEncoder->mEncodedAudioFrameBufferOffset], freeNothing);

        audioEncoder->mEncodedFrameForAdtsMuxer->assignDataSharedPtr(mAudioData);
        audioEncoder->mEncodedFrameForAdtsMuxer->setSize(chunkSize);
        
        return chunkSize;

    }

    AudioFrame<AudioCodec, audioSampleRate, audioChannels>* mEncodedFrameForAdtsMuxer;
    bool mLibavNeedsToBuffer;
    long mFrameCounter;

};

}

#endif // FFMPEGAUDIOENCODER_HPP_INCLUDED

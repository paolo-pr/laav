/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef FFMPEGMUXER_HPP_INCLUDED
#define FFMPEGMUXER_HPP_INCLUDED

#include "FFMPEGCommon.hpp"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include <iostream>
#include <fstream>

namespace laav
{

template <typename Container>
class FFMPEGMuxerCommonImpl
{
    template <typename Container_, typename VideoCodecOrFormat,
              unsigned int width, unsigned int height>
    friend class HTTPVideoStreamer;

    template <typename Container_, typename AudioCodecOrFormat,
              unsigned int audioSampleRate, enum AudioChannels audioChannels>
    friend class HTTPAudioStreamer;

public:

    bool startMuxing(const std::string& outputFilename = "")
    {
        if (isMuxing())
            return false;
        mDoMux = true;
        mLastMuxedAudioFrameOffset = mAudioAVPktsToMuxOffset;
        mLastMuxedVideoFrameOffset = mVideoAVPktsToMuxOffset;
        if (!outputFilename.empty())
        {
            mMuxedChunks.muxedFile.open(outputFilename, std::ios::trunc);
            mWriteToFile = true;
        }
        else
            mWriteToFile = false;
        return true;
    }

    bool stopMuxing()
    {
        if (!isMuxing())
            return false;
        mDoMux = false;
        writeTrailer();
        mTrailerWritten = true;
        if (mMuxedChunks.muxedFile.is_open())
            mMuxedChunks.muxedFile.close();
        mWriteToFile = false;
        return true;
    }

    bool isMuxing() const
    {
        return mDoMux;
    }
    
    bool isRecording() const
    {
        return mMuxedChunks.muxedFile.is_open();
    }

    const std::string& header() const
    {
        return mMuxedChunks.header;
    }

protected:

    FFMPEGMuxerCommonImpl():
        mMuxAudio(false),
        mMuxVideo(false),
        mDoMux(false),
        mHeaderWritten(false),
        mTrailerWritten(false),
        mAudioReady(false),
        mVideoReady(false),
        mLastMuxedAudioFrameOffset(0),
        mAudioAVPktsToMuxOffset(0),
        mLastMuxedVideoFrameOffset(0),
        mVideoAVPktsToMuxOffset(0),
        mAudioStreamIndex(1),
        mVideoStreamIndex(0),
        mWriteToFile(false)
    {
        av_register_all();
        // TODO: is the following line really necessary?
        avcodec_register_all();

        AVOutputFormat* muxerFormat =
        av_guess_format(FFMPEGUtils::translateContainer<Container>(), NULL, NULL);

        if (!muxerFormat)
            printAndThrowUnrecoverableError("muxerFormat = av_guess_format(...)");

        avformat_alloc_output_context2(&mMuxerContext, muxerFormat, "dummy", NULL);
        if (!mMuxerContext)
            printAndThrowUnrecoverableError("avformat_alloc_output_context2(...)");
        // TODO: fixed value?
        size_t muxerAVIOContextBufferSize = 4096;
        mMuxerAVIOContextBuffer = (uint8_t* )av_malloc(muxerAVIOContextBufferSize);
        if (!mMuxerContext)
            printAndThrowUnrecoverableError("(uint8_t* )av_malloc(...)");
        // TODO: fixed value?
        mMuxedChunks.data.resize(5000);
        mMuxedChunks.offset = 0;

        mMuxedChunks.newGroupOfChunks = false;

        unsigned int n;
        for (n = 0; n < mMuxedChunks.data.size(); n++)
        {
            mMuxedChunks.data[n].ptr = (uint8_t* )av_malloc(4096);
            mMuxedChunks.data[n].size = 0;
        }

        mMuxerAVIOContext = avio_alloc_context(mMuxerAVIOContextBuffer, muxerAVIOContextBufferSize,
                                               1, &mMuxedChunks, NULL, &writeMuxedChunk, NULL);

        if (!mMuxerAVIOContext)
            printAndThrowUnrecoverableError("mMuxerAVIOContext = avio_alloc_context(...);");

        mMuxerContext->pb = mMuxerAVIOContext;

        // TODO: improve design. First packets for audio and video
        // are set here in order to obtain a reference in muxNextUsefulFrameFromBuffer
        // in case one of the two media is not set
        AVPacket audioPkt;
        mAudioAVPktsToMux.push_back(audioPkt);
        av_init_packet(&mAudioAVPktsToMux[0]);
        mAudioAVPktsToMux[0].pts = AV_NOPTS_VALUE;
        AVPacket videoPkt;
        mVideoAVPktsToMux.push_back(videoPkt);
        av_init_packet(&mVideoAVPktsToMux[0]);
        mVideoAVPktsToMux[0].pts = AV_NOPTS_VALUE;
    }

    void muxNextUsefulFrameFromBuffer(bool resetChunks)
    {

        AVPacket& audioPktToMux = mAudioAVPktsToMux[mLastMuxedAudioFrameOffset];
        AVPacket& videoPktToMux = mVideoAVPktsToMux[mLastMuxedVideoFrameOffset];

        if (!mVideoReady && mMuxVideo)
        {
            return;
        }
        if (!mAudioReady && mMuxAudio)
        {
            return;
        }

        if ( (audioPktToMux.pts < videoPktToMux.pts && mMuxAudio) ||
             ((videoPktToMux.pts ==  AV_NOPTS_VALUE) && mMuxAudio && !mMuxVideo)
           )
        {
            if (mLastMuxedAudioFrameOffset != mAudioAVPktsToMuxOffset)
            {
                if (resetChunks)
                {
                    mMuxedChunks.offset = 0;
                }

                if (!this->mHeaderWritten)
                {
                    this->writeHeader();
                }

                if (audioPktToMux.size != 0)
                    if (av_write_frame(this->mMuxerContext, &audioPktToMux) < 0)
                        printAndThrowUnrecoverableError("av_write_frame(...)");

                mLastMuxedAudioFrameOffset =
                (mLastMuxedAudioFrameOffset + 1) % mAudioAVPktsToMux.size();

                muxNextUsefulFrameFromBuffer(false);
            }
        }
        else if (mMuxVideo)
        {
            if (mLastMuxedVideoFrameOffset != mVideoAVPktsToMuxOffset)
            {
                if (resetChunks)
                {
                    mMuxedChunks.offset = 0;
                }

                if (!this->mHeaderWritten)
                {
                    this->writeHeader();
                }

                if (av_write_frame(this->mMuxerContext, &videoPktToMux) < 0)
                    printAndThrowUnrecoverableError("av_write_frame(...)");

                mLastMuxedVideoFrameOffset =
                (mLastMuxedVideoFrameOffset + 1) % mVideoAVPktsToMux.size();

                muxNextUsefulFrameFromBuffer(false);
            }
        }
    }

    void completeMuxerInitialization()
    {
        if (avformat_init_output(this->mMuxerContext, NULL) < 0)
            printAndThrowUnrecoverableError("avformat_init_output(...)");
    }

    void writeHeader()
    {
        if (mHeaderWritten)
            return;
        if (avformat_write_header(this->mMuxerContext, NULL) < 0)
            printAndThrowUnrecoverableError("avformat_write_header(...)");

        AVPacket dummyPacket;
        av_init_packet(&dummyPacket);
        dummyPacket.size = 0;
        dummyPacket.pts = dummyPacket.dts = 0;
        int level = av_log_get_level();
        av_log_set_level(AV_LOG_QUIET);
        av_write_frame(FFMPEGMuxerCommonImpl<Container>::mMuxerContext, &dummyPacket);
        av_log_set_level(level);
        av_packet_unref(&dummyPacket);
        mHeaderWritten = true;
        mTrailerWritten = false;
    }

    void writeTrailer()
    {
        if (mTrailerWritten)
            return;
        av_write_trailer(this->mMuxerContext);
        mHeaderWritten = false;
        mTrailerWritten = true;
    }

    ~FFMPEGMuxerCommonImpl()
    {
        writeTrailer();
        if (mMuxedChunks.muxedFile.is_open())
            mMuxedChunks.muxedFile.close();
        av_free(mMuxerAVIOContextBuffer);
        unsigned int n;
        for (n = 0; n < mMuxedChunks.data.size(); n++)
        {
            av_free(mMuxedChunks.data[n].ptr);
        }
        avformat_free_context(mMuxerContext);
        av_free(mMuxerAVIOContext);

    }

    bool mMuxAudio;
    bool mMuxVideo;
    AVFormatContext* mMuxerContext;
    bool mDoMux;
    bool mHeaderWritten;
    bool mTrailerWritten;
    bool mAudioReady;
    bool mVideoReady;
    unsigned int mLastMuxedAudioFrameOffset;
    unsigned int mAudioAVPktsToMuxOffset;
    std::vector<AVPacket> mAudioAVPktsToMux;
    unsigned int mLastMuxedVideoFrameOffset;
    unsigned int mVideoAVPktsToMuxOffset;
    std::vector<AVPacket> mVideoAVPktsToMux;
    AVIOContext* mMuxerAVIOContext;
    uint8_t* mMuxerAVIOContextBuffer;
    unsigned int mAudioStreamIndex;
    unsigned int mVideoStreamIndex;
    ShareableMuxedData mMuxedData;

    struct MuxedDataChunk
    {
        uint8_t* ptr;
        size_t size;
    };

    struct MuxedChunks
    {
        bool newGroupOfChunks;
        unsigned int offset;
        std::vector<struct MuxedDataChunk> data;
        std::ofstream muxedFile;
        std::string header;
    } mMuxedChunks;

private:

    bool mWriteToFile;

    static int writeMuxedChunk (void* opaque, uint8_t* muxedDataSink, int chunkSize)
    {
        // TODO: static cast
        struct MuxedChunks* muxedChunks = ( struct MuxedChunks* )opaque;
        memcpy(muxedChunks->data[muxedChunks->offset].ptr, muxedDataSink, chunkSize);

        if (muxedChunks->header.empty())
        {
            muxedChunks->header.assign ((const char* )muxedDataSink, chunkSize);
        }

        muxedChunks->data[muxedChunks->offset].size = chunkSize;
        if (chunkSize != 0)
        {
            muxedChunks->newGroupOfChunks = true;
            if (muxedChunks->muxedFile.is_open())
            {
                std::streamsize dataSize = chunkSize;
                muxedChunks->muxedFile.write((const char* )muxedDataSink, dataSize);
            }
        }
        muxedChunks->offset++;

        return chunkSize;
    }

    AVCodecContext* mVideoEncoderCodecContext0;
    AVCodecContext* mAudioEncoderCodecContext0;
    AVCodecContext* mVideoEncoderCodecContext;
    AVCodecContext* mAudioEncoderCodecContext;

};

}

#endif // FFMPEGMUXER_HPP_INCLUDED

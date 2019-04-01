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
#include <chrono>
#include <ctime>
#include <sstream>

#define DEFAULT_REC_FILENAME "[[default]]"

namespace laav
{

template <typename Container>
class FFMPEGMuxerCommonImpl : public Medium
{

template <typename Container_,
          typename VideoCodecOrFormat, typename width, typename height,
          typename AudioCodecOrFormat, typename audioSampleRate, typename audioChannels>              
    friend class HTTPAudioVideoStreamer;    
    
    template <typename Container_, typename VideoCodecOrFormat,
              typename width, typename height>
    friend class HTTPVideoStreamer;

    template <typename Container_, typename AudioCodecOrFormat,
              typename audioSampleRate, typename audioChannels>
    friend class HTTPAudioStreamer;

    template <typename Container_, typename VideoCodecOrFormat,
              typename width, typename height>
    friend class UDPVideoStreamer;

    template <typename Container_, typename AudioCodecOrFormat,
              typename audioSampleRate, typename audioChannels>
    friend class UDPAudioStreamer;    
    
public:
        
    // TODO: add exception handling for recorded file
    bool startRecording(const std::string& outputFilename = "")
    {
        if (isMuxing())
            return false;
        writeTrailer(); 
        avio_flush(mMuxerAVIOContext); 
        writeHeader();
        mMuxedChunksHelper.headerWritten = false;
        mMuxedChunksHelper.gotVideoKeyFrameForRec = false;
        
        mDoMux = true;
        mLastMuxedAudioFrameOffset = mAudioAVPktsToMuxOffset;
        mLastMuxedVideoFrameOffset = mVideoAVPktsToMuxOffset;
        std::string outputFilenameCpy = outputFilename;
        if (!outputFilename.empty())
        {
            if(outputFilename.find(DEFAULT_REC_FILENAME) != std::string::npos)
            {
                std::time_t t = std::time(nullptr);
                std::tm tm = *std::localtime(&t);
                std::ostringstream oss;
                oss << std::put_time( &tm, "_%Y-%m-%d_%H:%M:%S" );
                std::string timestamp( oss.str() );
                std::string dot; 
                if (!mContainerFileExtension.empty())
                    dot = ".";
                outputFilenameCpy = split(outputFilename, DEFAULT_REC_FILENAME)[0] + 
                                    timestamp + dot + mContainerFileExtension; 
                outputFilenameCpy = mId + outputFilenameCpy;
            }
            
            try
            {
                mMuxedChunksHelper.muxedFile.open(outputFilenameCpy, std::ios::trunc);
                mOutputFilename = outputFilenameCpy;
            }
            catch (const std::exception& e)
            {
                printAndThrowUnrecoverableError("mMuxedChunksHelper.muxedFile.open(...)");
            }
            mMuxedChunksHelper.recordingFileSize = 0;
            mWriteToFile = true;
        }
        else
            mWriteToFile = false;
        
        loggerLevel1 << "Started recording '" << mOutputFilename << "' on muxer with id='" <<
        Medium::id() << "'" << std::endl;
        
        return true;
    }

    //TODO: add proper exception handling for muxedFile
    bool stopRecording()
    {
        if (!isMuxing())
            return false;
                
        mDoMux = false;
        writeTrailer();    
        mTrailerWritten = true;
        if (mMuxedChunksHelper.muxedFile.is_open())
        {
            mMuxedChunksHelper.muxedFile.close();
            loggerLevel1 << "Finished recording '" << mOutputFilename << "' on muxer with id='" << 
            Medium::id() << "' : filesize=" << mMuxedChunksHelper.recordingFileSize << std::endl;             
            mMuxedChunksHelper.recordingFileSize = -1;
        }
        avio_flush(mMuxerAVIOContext);  
        writeHeader();         
        mWriteToFile = false;

        return true;
    }

    bool isMuxing() const
    {
        return mDoMux;
    }
    
    bool isRecording() const
    {
        return mMuxedChunksHelper.muxedFile.is_open();
    }

    const std::string& header() const
    {
        return mMuxedChunksHelper.header;
    }

    int recordingFileSize() const
    {
        return mMuxedChunksHelper.recordingFileSize;
    }
    
    
protected:

    FFMPEGMuxerCommonImpl(const std::string& id = ""):
        Medium(id),
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
        
        if (std::is_same<Container, MPEGTS>::value)
            mContainerFileExtension = "ts";
        else if (std::is_same<Container, MATROSKA>::value)
            mContainerFileExtension = "mkv";
            
        //av_register_all();
        // TODO: is the following line really necessary?
        //avcodec_register_all();

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
        mMuxedChunksHelper.data.resize(5000);
        mMuxedChunksHelper.offset = 0;
        mMuxedChunksHelper.recordingFileSize = -1;
        mMuxedChunksHelper.latencyCounter = -1;
        mMuxedChunksHelper.stepsToSkipBeforeRec = -1;
        mMuxedChunksHelper.gotVideoKeyFrameForRec = false;
        mMuxedChunksHelper.latencyValObtained = false;
        mMuxedChunksHelper.needsVideoKeyFrame = false;
        mMuxedChunksHelper.muxedFile.exceptions(std::fstream::badbit);
        mMuxedChunksHelper.newGroupOfChunks = false;
        mMuxedChunksHelper.headerWritten = false;
        
        unsigned int n;
        for (n = 0; n < mMuxedChunksHelper.data.size(); n++)
        {
            mMuxedChunksHelper.data[n].ptr = (uint8_t* )av_malloc(4096);
            mMuxedChunksHelper.data[n].size = 0;
        }

        mMuxerAVIOContext = avio_alloc_context(mMuxerAVIOContextBuffer, muxerAVIOContextBufferSize, 1, &mMuxedChunksHelper, NULL, &writeMuxedChunk, NULL);

        if (!mMuxerAVIOContext)
            printAndThrowUnrecoverableError("mMuxerAVIOContext = avio_alloc_context(...);");

        mMuxerContext->pb = mMuxerAVIOContext;
#ifdef PAT_PMT_AT_FRAMES
        av_opt_set(mMuxerContext->priv_data, "pat_pmt_at_frames", "1", 0);
#endif
        //av_opt_set(mMuxerContext->priv_data, "mpegts_flags", "pat_pmt_at_frames+resend_headers", 1);
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
                    mMuxedChunksHelper.offset = 0;
                }

                if (audioPktToMux.size != 0)
                {
                    if (!mMuxedChunksHelper.latencyValObtained && mMuxedChunksHelper.needsVideoKeyFrame)
                        mMuxedChunksHelper.latencyCounter++;
                    if (av_write_frame(this->mMuxerContext, &audioPktToMux) < 0)
                        printAndThrowUnrecoverableError("av_write_frame(...)");               
                }
                
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
                    mMuxedChunksHelper.offset = 0;
                }
                if (!mMuxedChunksHelper.latencyValObtained && mMuxedChunksHelper.needsVideoKeyFrame)
                    mMuxedChunksHelper.latencyCounter++;
                
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
        writeHeader();
    }

    void writeHeader()
    {        
        if (avformat_write_header(this->mMuxerContext, NULL) < 0)
            printAndThrowUnrecoverableError("avformat_write_header(...)");
    }

    void writeTrailer()
    {
        av_write_trailer(this->mMuxerContext);
    }

    ~FFMPEGMuxerCommonImpl()
    {
        writeTrailer();
        if (mMuxedChunksHelper.muxedFile.is_open())
            mMuxedChunksHelper.muxedFile.close();
        av_free(mMuxerAVIOContextBuffer);
        unsigned int n;
        for (n = 0; n < mMuxedChunksHelper.data.size(); n++)
        {
            av_free(mMuxedChunksHelper.data[n].ptr);
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
    std::string mOutputFilename;
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
    std::string mContainerFileExtension;
    
    struct MuxedDataChunk
    {
        uint8_t* ptr;
        size_t size;
    };

    struct MuxedChunksHelper
    {
        bool newGroupOfChunks;
        unsigned int offset;
        std::vector<struct MuxedDataChunk> data;
        std::ofstream muxedFile;
        std::string header;
        int recordingFileSize;
        int latencyCounter;
        bool latencyValObtained;
        int stepsToSkipBeforeRec;
        bool needsVideoKeyFrame;
        bool gotVideoKeyFrameForRec;
        bool headerWritten;
    } mMuxedChunksHelper;

private:

    bool mWriteToFile;

    int latencyCounter() const
    {
        if (mMuxedChunksHelper.latencyValObtained)
            return mMuxedChunksHelper.latencyCounter;
        else
            return -1;
    }
    
    static int writeMuxedChunk (void* opaque, uint8_t* muxedDataSink, int chunkSize)
    {
        // TODO: static cast        
        struct MuxedChunksHelper* muxedChunksHelper = ( struct MuxedChunksHelper* )opaque;
        memcpy(muxedChunksHelper->data[muxedChunksHelper->offset].ptr, 
               muxedDataSink, chunkSize);

        if (!muxedChunksHelper->latencyValObtained && muxedChunksHelper->latencyCounter >= 0)
        {
            muxedChunksHelper->latencyValObtained = true;
            muxedChunksHelper->stepsToSkipBeforeRec = muxedChunksHelper->latencyCounter;
        }
        
        if (muxedChunksHelper->header.empty())
        {
            muxedChunksHelper->header.assign ((const char* )muxedDataSink, chunkSize);
            //if (std::is_same<Container, MPEGTS>::value)
            //return 0;
        }

        muxedChunksHelper->data[muxedChunksHelper->offset].size = chunkSize;
        if (chunkSize != 0)
        {
            muxedChunksHelper->newGroupOfChunks = true;
            if (muxedChunksHelper->muxedFile.is_open())
            {
                muxedChunksHelper->recordingFileSize += chunkSize;
                std::streamsize dataSize = chunkSize;
                
                if (!muxedChunksHelper->headerWritten && !std::is_same<Container, MPEGTS>::value)
                {
                    muxedChunksHelper->muxedFile.write((const char* )muxedChunksHelper->header.c_str(), muxedChunksHelper->header.size());
                    muxedChunksHelper->headerWritten = true;
                }
                muxedChunksHelper->muxedFile.write((const char* )muxedDataSink, dataSize);
            }
        }
        muxedChunksHelper->offset++;
        return chunkSize;
    }

    AVCodecContext* mVideoEncoderCodecContext0;
    AVCodecContext* mAudioEncoderCodecContext0;
    AVCodecContext* mVideoEncoderCodecContext;
    AVCodecContext* mAudioEncoderCodecContext;        
};

}

#endif // FFMPEGMUXER_HPP_INCLUDED

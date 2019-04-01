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

#ifndef FFMPEGAUDIOVIDEOMUXER_HPP_INCLUDED
#define FFMPEGAUDIOVIDEOMUXER_HPP_INCLUDED

#include "FFMPEGVideoMuxer.hpp"
#include "FFMPEGAudioMuxer.hpp"

namespace laav
{

template <typename Container,
          typename VideoCodecOrFormat,
          typename width,
          typename height,
          typename AudioCodecOrFormat,
          typename audioSampleRate,
          typename audioChannels>
class FFMPEGAudioVideoMuxer :
public FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, width, height>,
public FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat, audioSampleRate, audioChannels>
{

    template <typename Container_,
              typename VideoCodecOrFormat_,
              typename width_,
              typename height_,
              typename AudioCodecOrFormat_,
              typename audioSampleRate_,
              typename audioChannels_>
   friend class HTTPAudioVideoStreamer;

    template <typename Container_,
              typename VideoCodecOrFormat_,
              typename width_,
              typename height_,
              typename AudioCodecOrFormat_,
              typename audioSampleRate_,
              typename audioChannels_>
   friend class UDPAudioVideoStreamer;
   
public:

    //TODO group all the ctors with a forwarder func
    
    FFMPEGAudioVideoMuxer(const std::string& id = "") :
        FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat,
                             width, height>(id),
        FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat,
                             audioSampleRate, audioChannels>(id)
    {
        static_assert(! (std::is_same<Container, MATROSKA>::value && std::is_same<VideoCodecOrFormat, H264>::value), 
                      "Please pass the H264 encoder to the constructor!"); 
        
        static_assert(! (std::is_same<Container, MATROSKA>::value), 
                      "Please pass the audio encoder to the constructor!");         
        
        FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat, audioSampleRate, audioChannels>::setParamsFromEmbeddedAudioCodecContext();
        FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, width, height>::setParamsFromEmbeddedVideoCodecContext();
        ctorCommon();
    }

    template <typename RawVideoFrameFormat>
    FFMPEGAudioVideoMuxer(const FFMPEGVideoEncoder<RawVideoFrameFormat, VideoCodecOrFormat, width, height>& vEnc, 
                          const std::string& id = "") :
        FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat,
                             width, height>(id),
        FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat,
                             audioSampleRate, audioChannels>(id)
    {

        static_assert(! (std::is_same<Container, MATROSKA>::value), 
                      "Please pass the audio encoder to the constructor, as needed by MATROSKA muxer!");         
        
        // they will be freed by the avformat cleanup stuff
        avcodec_parameters_from_context(FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, width, height>::mVideoStream->codecpar, vEnc.mVideoEncoderCodecContextOutOfBand); 
        FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat, audioSampleRate, audioChannels>::setParamsFromEmbeddedAudioCodecContext();
        ctorCommon();
    }    
    
    template <typename PCMSoundFormat>  
    FFMPEGAudioVideoMuxer(const FFMPEGAudioEncoder<PCMSoundFormat, AudioCodecOrFormat, audioSampleRate, audioChannels>& aEnc,
                          const std::string& id = "") :
    FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat,
                         width, height>(id),
    FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat,
                         audioSampleRate, audioChannels>(id)
    {
        static_assert(! (std::is_same<Container, MATROSKA>::value && std::is_same<VideoCodecOrFormat, H264>::value), 
                      "Please pass the H264 encoder to the constructor, as needed by MATROSKA muxer!");
        
        avcodec_parameters_from_context(FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat, audioSampleRate, audioChannels>::mAudioStream->codecpar, 
                                        aEnc.mAudioEncoderCodecContext);
        FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, width, height>::setParamsFromEmbeddedVideoCodecContext();
        ctorCommon();
    }    
    
    template <typename PCMSoundFormat, typename RawVideoFrameFormat>  
    FFMPEGAudioVideoMuxer(const FFMPEGAudioEncoder<PCMSoundFormat, AudioCodecOrFormat, audioSampleRate, audioChannels>& aEnc,
                          const FFMPEGVideoEncoder<RawVideoFrameFormat, VideoCodecOrFormat, width, height>& vEnc,
                          const std::string& id = "") :
    FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat,
                         width, height>(id),
    FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat,
                         audioSampleRate, audioChannels>(id)
    {
        avcodec_parameters_from_context(FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat, audioSampleRate, audioChannels>::mAudioStream->codecpar, 
                                        aEnc.mAudioEncoderCodecContext);
        avcodec_parameters_from_context(FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, width, height>::mVideoStream->codecpar, vEnc.mVideoEncoderCodecContextOutOfBand);     
        ctorCommon();
    }    
    
    /*!
     *  \exception MediumException
     */
    const std::vector<MuxedAudioVideoData<Container, VideoCodecOrFormat, width, height,
                                          AudioCodecOrFormat, audioSampleRate, audioChannels> >&
    muxedAudioVideoChunks()
    {
        if (Medium::mInputStatus == PAUSED)
            throw MediumException(MEDIUM_PAUSED);        

        if (Medium::mStatusInPipe ==  NOT_READY)
            throw MediumException(PIPE_NOT_READY);        
        
        if (this->mMuxedChunksHelper.newGroupOfChunks)
        {
            unsigned int n;
            for (n = 0; n < this->mMuxedChunksHelper.offset; n++)
            {
                mMuxedAudioVideoChunks[n].setSize(this->mMuxedChunksHelper.data[n].size);
            }
            Medium::mOutputStatus  = READY;            
            return mMuxedAudioVideoChunks;
        }
        else
        {
            Medium::mOutputStatus  = NOT_READY;            
            throw MediumException(OUTPUT_NOT_READY);
        }
    }

    unsigned int muxedAudioVideoChunksOffset() const
    {
        return this->mMuxedChunksHelper.offset;
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
    void mux(const VideoFrame<VideoCodecOrFormat, width, height>& videoFrameToMux)
    {     
        FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat,
                             width, height>::mux(videoFrameToMux);
    }    
    
    
private:

    std::vector<MuxedAudioVideoData<Container, VideoCodecOrFormat,
                                    width, height,
                                    AudioCodecOrFormat, audioSampleRate,
                                    audioChannels> > mMuxedAudioVideoChunks;
                                    
    void ctorCommon()
    {
        this->mMuxAudio = true;
        this->mMuxVideo = true;
        FFMPEGMuxerCommonImpl<Container>::mMuxedChunksHelper.needsVideoKeyFrame = true; 
        FFMPEGMuxerCommonImpl<Container>::completeMuxerInitialization();
        auto freeNothing = [](unsigned char* buffer) {  };

        mMuxedAudioVideoChunks.resize(this->mMuxedChunksHelper.data.size());
        unsigned int n;
        for (n = 0; n < mMuxedAudioVideoChunks.size(); n++)
        {
            ShareableMuxedData shMuxedData(this->mMuxedChunksHelper.data[n].ptr, freeNothing);
            mMuxedAudioVideoChunks[n].assignDataSharedPtr(shMuxedData);
        }
        Medium::mInputStatus = READY;
    }
};

}

#endif // FFMPEGAUDIOVIDEOMUXER_HPP_INCLUDED

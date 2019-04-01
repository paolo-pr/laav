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

#ifndef FFMPEGH264ENCODER_HPP_INCLUDED
#define FFMPEGH264ENCODER_HPP_INCLUDED

#include "FFMPEGVideoEncoder.hpp"
#include "UserParams.hpp"

namespace laav
{

template <typename RawVideoFrameFormat, typename width, typename height>
class FFMPEGH264Encoder: public FFMPEGVideoEncoder<RawVideoFrameFormat, H264, width, height>
{
public:

    FFMPEGH264Encoder(const std::string& id = "") :
        FFMPEGVideoEncoder<RawVideoFrameFormat, H264, width, height>(id)
    {
        this->completeEncoderInitialization();
    }

    FFMPEGH264Encoder(unsigned int bitrate, unsigned int gopSize,
                      enum H264Presets preset, enum H264Profiles profile, 
                      enum H264Tunes tune, const std::string& id = ""):
        // 200 * 1000 is AV_CODEC_DEFAULT_BITRATE, which is not yet in the API of libavcodec
        FFMPEGVideoEncoder<RawVideoFrameFormat, H264, width, height>(id),
        mPrevBitrate(200 * 10000)
    {
        if (bitrate != DEFAULT_BITRATE)
        {
            mPrevBitrate = bitrate;
            this->mVideoEncoderCodecContext->bit_rate = bitrate;
            this->mVideoEncoderCodecContextOutOfBand->bit_rate = bitrate;
        }
        else
        {
            this->mVideoEncoderCodecContext->bit_rate = mPrevBitrate;
            this->mVideoEncoderCodecContextOutOfBand->bit_rate = mPrevBitrate;
        }
        if (gopSize != DEFAULT_GOPSIZE)
        {
            this->mVideoEncoderCodecContext->gop_size = gopSize;
            this->mVideoEncoderCodecContextOutOfBand->gop_size = gopSize;
        }
        if (preset != H264_DEFAULT_PRESET)
        {
            av_opt_set(this->mVideoEncoderCodecContext->priv_data,
                       "preset", convertToFFMPEGPreset(preset), 0);            
            av_opt_set(this->mVideoEncoderCodecContextOutOfBand->priv_data,
                       "preset", convertToFFMPEGPreset(preset), 0);
        }
        if (profile != H264_DEFAULT_PROFILE)
        {
            this->mVideoEncoderCodecContextOutOfBand->profile = convertToFFMPEGProfile(profile);            
            this->mVideoEncoderCodecContext->profile = convertToFFMPEGProfile(profile);
        }
        if (tune != H264_DEFAULT_TUNE)
        {
            av_opt_set(this->mVideoEncoderCodecContextOutOfBand->priv_data,
                       "tune", convertToFFMPEGTune(tune), 0);            
            av_opt_set(this->mVideoEncoderCodecContext->priv_data,
                       "tune", convertToFFMPEGTune(tune), 0);
        }
        // Only for preventing a meaningless (?) libx264 warning
        this->mVideoEncoderCodecContext->ticks_per_frame = 
        this->mVideoEncoderCodecContext->time_base.den / 100;
        this->mVideoEncoderCodecContextOutOfBand->ticks_per_frame = 
        this->mVideoEncoderCodecContextOutOfBand->time_base.den / 100;        
        
        adaptBufsizeAndMaxRateToBitrate();          
        this->completeEncoderInitialization();  
    }
    
    // In order to change the bitrate on the fly libavcodec requires
    // two distinct operations (see beforeEncodeCallback() and afterEncodeCallback():
    // *) set new bit_rate 
    // *) set rc_buffer_size and rc_max rate        
    void setBitrate(int bitrate)
    {
        this->mVideoEncoderCodecContext->bit_rate = bitrate;
    }
    
    int bitrate() const
    {
        return this->mVideoEncoderCodecContext->bit_rate;
    }
    
private:    
    
    void beforeEncodeCallback()
    { 
        if (mPrevBitrate < this->mVideoEncoderCodecContext->bit_rate)
        {
            adaptBufsizeAndMaxRateToBitrate();    
            mPrevBitrate = this->mVideoEncoderCodecContext->bit_rate;
        }       
        
        /*
        if (mPrevBitrate < this->mVideoEncoderCodecContextOutOfBand->bit_rate)
        {
            adaptBufsizeAndMaxRateToBitrate();    
            mPrevBitrate = this->mVideoEncoderCodecContextOutOfBand->bit_rate;
        }*/       
    }
    
    void afterEncodeCallback() 
    {
        if (this->mVideoEncoderCodecContext->rc_buffer_size > this->mVideoEncoderCodecContext->bit_rate * 5)
        {         
           adaptBufsizeAndMaxRateToBitrate();
           mPrevBitrate = this->mVideoEncoderCodecContext->bit_rate;
        }
        
        /*
        if (this->mVideoEncoderCodecContextOutOfBand->rc_buffer_size > this->mVideoEncoderCodecContextOutOfBand->bit_rate * 5)
        {         
           adaptBufsizeAndMaxRateToBitrate();
           mPrevBitrate = this->mVideoEncoderCodecContextOutOfBand->bit_rate;
        }*/        
    } 
    
    void adaptBufsizeAndMaxRateToBitrate()
    {
        this->mVideoEncoderCodecContextOutOfBand->rc_buffer_size = this->mVideoEncoderCodecContextOutOfBand->bit_rate * 5;
        this->mVideoEncoderCodecContext->rc_buffer_size = this->mVideoEncoderCodecContext->bit_rate * 5;
        this->mVideoEncoderCodecContextOutOfBand->rc_max_rate = this->mVideoEncoderCodecContextOutOfBand->bit_rate * 5;  
        this->mVideoEncoderCodecContext->rc_max_rate = this->mVideoEncoderCodecContext->bit_rate * 5;      
    }
    
    int mPrevBitrate;
};

}

#endif // FFMPEGH264ENCODER_HPP_INCLUDED

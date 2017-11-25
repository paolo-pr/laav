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

template <typename RawVideoFrameFormat, unsigned int width, unsigned int height>
class FFMPEGH264Encoder: public FFMPEGVideoEncoder<RawVideoFrameFormat, H264, width, height>
{
public:

    FFMPEGH264Encoder()
    {
        this->completeEncoderInitialization();
    }

    FFMPEGH264Encoder(unsigned int bitrate, unsigned int gopSize,
                      enum H264Presets preset, enum H264Profiles profile, enum H264Tunes tune)
    {
        if (bitrate != DEFAULT_BITRATE)
            this->mVideoEncoderCodecContext->bit_rate = bitrate;
        if (gopSize != DEFAULT_GOPSIZE)
            this->mVideoEncoderCodecContext->gop_size = gopSize;
        if (preset != H264_DEFAULT_PRESET)
            av_opt_set(this->mVideoEncoderCodecContext->priv_data,
                       "preset", convertToFFMPEGPreset(preset), 0);
        if (profile != H264_DEFAULT_PROFILE)
            this->mVideoEncoderCodecContext->profile = convertToFFMPEGProfile(profile);
        if (tune != H264_DEFAULT_TUNE)
            av_opt_set(this->mVideoEncoderCodecContext->priv_data,
                       "tune", convertToFFMPEGTune(tune), 0);            
        this->completeEncoderInitialization();
    }

};

}

#endif // FFMPEGH264ENCODER_HPP_INCLUDED

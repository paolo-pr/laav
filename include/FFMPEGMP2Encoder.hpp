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

#ifndef FFMPEGMP2ENCODER_HPP_INCLUDED
#define FFMPEGMP2ENCODER_HPP_INCLUDED

#include "FFMPEGAudioEncoder.hpp"
#include "UserParams.hpp"

namespace laav
{

template <typename PCMSoundFormat,
          unsigned int audioSampleRate,
          enum AudioChannels audioChannels,
          typename... UserParams>
class FFMPEGMP2Encoder :
public FFMPEGAudioEncoder<PCMSoundFormat, MP2, audioSampleRate, audioChannels>
{

public:

    FFMPEGMP2Encoder()
    {
        this->completeEncoderInitialization();
    }

    FFMPEGMP2Encoder(unsigned int bitrate)
    {
        if (bitrate != DEFAULT_BITRATE)
            this->mAudioEncoderCodecContext->bit_rate = bitrate;
        this->completeEncoderInitialization();
    }

};

}

#endif // FFMPEGMP2ENCODER_HPP_INCLUDED

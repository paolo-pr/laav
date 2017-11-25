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

#ifndef FFMPEGADTSAACENCODER_HPP_INCLUDED
#define FFMPEGADTSAACENCODER_HPP_INCLUDED

#include "FFMPEGAudioEncoder.hpp"
#include "UserParams.hpp"

namespace laav
{

template <unsigned int audioSampleRate, enum AudioChannels audioChannels>
class FFMPEGADTSAACEncoder : public FFMPEGAudioEncoder<FLOAT_PLANAR, ADTS_AAC,
                                                       audioSampleRate, audioChannels>
{

public:

    FFMPEGADTSAACEncoder()
    {
        this->completeEncoderInitialization();
    }

    FFMPEGADTSAACEncoder(unsigned int bitrate, enum AACProfiles profile)
    {
        if (bitrate != DEFAULT_BITRATE)
            this->mAudioEncoderCodecContext->bit_rate = bitrate;
        if (profile != AAC_DEFAULT_PROFILE)
            this->mAudioEncoderCodecContext->profile = convertToFFMPEGProfile(profile);
        this->completeEncoderInitialization();
    }

};

}

#endif // FFMPEGADTSAACENCODER_HPP_INCLUDED

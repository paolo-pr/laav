/* 
 * Created (15/03/2019) by Paolo-Pr.
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

#ifndef FFMPEGOPUSENCODER_HPP_INCLUDED
#define FFMPEGOPUSENCODER_HPP_INCLUDED

#include "FFMPEGAudioEncoder.hpp"
#include "UserParams.hpp"

namespace laav
{

template <typename PCMSoundFormat,
          typename audioSampleRate,
          typename audioChannels,
          typename... UserParams>
class FFMPEGOpusEncoder :
public FFMPEGAudioEncoder<PCMSoundFormat, OPUS, audioSampleRate, audioChannels>
{

public:

    FFMPEGOpusEncoder(const std::string& id = "") :
        FFMPEGAudioEncoder<PCMSoundFormat, OPUS, 
                           audioSampleRate, audioChannels>(id)
    {
        this->completeEncoderInitialization();
    }
                                                        
    FFMPEGOpusEncoder(enum OpusApplications application, long compressionLevel, unsigned int bitrate, const std::string& id = "") :
        FFMPEGAudioEncoder<PCMSoundFormat, OPUS, 
                           audioSampleRate, audioChannels>(id)    
    {

        if (application != OPUS_DEFAULT_APPLICATION)
            av_opt_set(this->mAudioEncoderCodecContext->priv_data,
                       "application", convertToFFMPEGOpusApplication(application), 0);
        
        if (bitrate != DEFAULT_BITRATE)
            this->mAudioEncoderCodecContext->bit_rate = bitrate;        
            
        if (compressionLevel != OPUS_DEFAULT_COMPRESSION_LEVEL)
            this->mAudioEncoderCodecContext->compression_level = compressionLevel;
        
        this->completeEncoderInitialization();
    }

};

}

#endif // FFMPEGOPUSENCODER_HPP_INCLUDED

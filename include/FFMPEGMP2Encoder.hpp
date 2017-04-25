/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
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

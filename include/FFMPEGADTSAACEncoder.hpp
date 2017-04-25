/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
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

/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
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

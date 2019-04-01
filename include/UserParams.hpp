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

#ifndef USERPARAMS_HPP_INCLUDED
#define USERPARAMS_HPP_INCLUDED

namespace laav
{

enum AACProfiles
{
    AAC_DEFAULT_PROFILE,
    AAC_ELD,
    AAC_HE,
    AAC_HE_V2,
    AAC_LD,
    AAC_LOW,
    AAC_LTP,
    AAC_MAIN,
    AAC_SSR
};

enum H264Profiles
{
    H264_DEFAULT_PROFILE,
    H264_BASELINE,
    H264_CONSTRAINED_BASELINE,
    H264_MAIN,
    H264_EXTENDED,
    H264_HIGH,
    H264_HIGH_10,
    H264_HIGH_10_INTRA,
    H264_MULTIVIEW_HIGH,
    H264_HIGH_422,
    H264_HIGH_422_INTRA,
    H264_STEREO_HIGH,
    H264_HIGH_444,
    H264_HIGH_444_PREDICTIVE,
    H264_HIGH_444_INTRA,
    H264_CAVLC_444
};

enum H264Tunes
{
    H264_DEFAULT_TUNE,
    H264_FILM,
    H264_ANIMATION,
    H264_GRAIN,
    H264_STILLIMAGE,
    H264_FASTDECODE,
    H264_ZEROLATENCY,
    H264_PSNR,
    H264_SSIM
};

enum H264Presets
{
    H264_DEFAULT_PRESET,
    H264_ULTRAFAST,
    H264_SUPERFAST,
    H264_VERY_FAST,
    H264_FASTER,
    H264_FAST,
    H264_MEDIUM,
    H264_SLOW,
    H264_SLOWER,
    H264_PLACEBO
};

enum OpusApplications
{
    OPUS_DEFAULT_APPLICATION,
    OPUS_VOIP,
    OPUS_AUDIO,
    OPUS_LOWDELAY
};

float OPUS_DEFAULT_COMPRESSION_LEVEL = -1;

}

#endif // USERPARAMS_HPP_INCLUDED

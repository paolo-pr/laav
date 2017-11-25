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

#ifndef FFMPEGCOMMON_HPP_INCLUDED
#define FFMPEGCOMMON_HPP_INCLUDED

#include "Common.hpp"
#include "YUV420PlanarFrame.hpp"
#include "YUYV422PackedFrame.hpp"
#include "YUV422PlanarFrame.hpp"
#include "YUV444PlanarFrame.hpp"
#include "NV12_PlanarFrame.hpp"
#include "NV21_PlanarFrame.hpp"
#include "MJPEGFrame.hpp"
#include "H264Frame.hpp"
#include "MP2Frame.hpp"
#include "FloatPackedFrame.hpp"
#include "FloatPlanarFrame.hpp"
#include "Signed16PackedFrame.hpp"
#include "ADTSAACFrame.hpp"
#include "UserParams.hpp"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
}

namespace laav
{

struct FFMPEGUtils
{

    template <typename T>
    static AVPixelFormat translatePixelFormat();

    template <typename T>
    static AVSampleFormat translateSampleFormat();

    template <typename T>
    static enum AVCodecID translateCodec();

    template <typename T>
    static const char* translateContainer();

};

template <>
AVPixelFormat FFMPEGUtils::translatePixelFormat<YUV420_PLANAR>()
{
    return AV_PIX_FMT_YUV420P;
}
template <>
AVPixelFormat FFMPEGUtils::translatePixelFormat<YUV422_PLANAR>()
{
    return AV_PIX_FMT_YUV422P;
}
template <>
AVPixelFormat FFMPEGUtils::translatePixelFormat<YUYV422_PACKED>()
{
    return AV_PIX_FMT_YUYV422;
}
template <>
AVPixelFormat FFMPEGUtils::translatePixelFormat<YUV444_PLANAR>()
{
    return AV_PIX_FMT_YUV444P;
}
template <>
AVPixelFormat FFMPEGUtils::translatePixelFormat<NV_12_PLANAR>()
{
    return AV_PIX_FMT_NV12;
}
template <>
AVPixelFormat FFMPEGUtils::translatePixelFormat<NV_21_PLANAR>()
{
    return AV_PIX_FMT_NV21;
}
template <>
AVSampleFormat FFMPEGUtils::translateSampleFormat<FLOAT_PACKED>()
{
    return AV_SAMPLE_FMT_FLT;
}
template <>
AVSampleFormat FFMPEGUtils::translateSampleFormat<FLOAT_PLANAR>()
{
    return AV_SAMPLE_FMT_FLTP;
}
template <>
AVSampleFormat FFMPEGUtils::translateSampleFormat<S16_LE>()
{
    return AV_SAMPLE_FMT_S16;
}

template <>
enum AVCodecID FFMPEGUtils::translateCodec<H264>()
{
    return AV_CODEC_ID_H264;
}
template <>
enum AVCodecID FFMPEGUtils::translateCodec<MJPEG>()
{
    return AV_CODEC_ID_MJPEG;
}
template <>
enum AVCodecID FFMPEGUtils::translateCodec<ADTS_AAC>()
{
    return AV_CODEC_ID_AAC;
}
template <>
enum AVCodecID FFMPEGUtils::translateCodec<MP2>()
{
    return AV_CODEC_ID_MP2;
}

template <>
const char* FFMPEGUtils::translateContainer<ADTS_AAC>()
{
    return "adts";
}
template <>
const char* FFMPEGUtils::translateContainer<MPEGTS>()
{
    return "mpegts";
}
template <>
const char* FFMPEGUtils::translateContainer<MATROSKA>()
{
    return "matroska";
}

int convertToFFMPEGProfile(enum H264Profiles profile)
{
    int ret = -1;
    switch(profile)
    {
    case (H264_DEFAULT_PROFILE):
        break;
    case (H264_BASELINE):
        ret = FF_PROFILE_H264_BASELINE;
        break;
    case (H264_CONSTRAINED_BASELINE):
        ret = FF_PROFILE_H264_CONSTRAINED_BASELINE;
        break;
    case (H264_MAIN):
        ret = FF_PROFILE_H264_MAIN;
        break;
    case (H264_EXTENDED):
        ret = FF_PROFILE_H264_EXTENDED;
        break;
    case (H264_HIGH):
        ret = FF_PROFILE_H264_HIGH;
        break;
    case (H264_HIGH_10):
        ret = FF_PROFILE_H264_HIGH_10;
        break;
    case (H264_HIGH_10_INTRA):
        ret = FF_PROFILE_H264_HIGH_10_INTRA;
        break;
    case (H264_MULTIVIEW_HIGH):
        ret = FF_PROFILE_H264_MULTIVIEW_HIGH;
        break;
    case (H264_HIGH_422):
        ret = FF_PROFILE_H264_HIGH_422;
        break;
    case (H264_HIGH_422_INTRA):
        ret = FF_PROFILE_H264_HIGH_422_INTRA;
        break;
    case (H264_STEREO_HIGH):
        ret = FF_PROFILE_H264_STEREO_HIGH;
        break;
    case (H264_HIGH_444):
        ret = FF_PROFILE_H264_HIGH_444;
        break;
    case (H264_HIGH_444_PREDICTIVE):
        ret = FF_PROFILE_H264_HIGH_444_PREDICTIVE;
        break;
    case (H264_HIGH_444_INTRA):
        ret = FF_PROFILE_H264_HIGH_444_INTRA;
        break;
    case (H264_CAVLC_444):
        ret = FF_PROFILE_H264_CAVLC_444;
        break;
    default:
        break;
    }
    return ret;
}

int convertToFFMPEGProfile(enum AACProfiles profile)
{
    int ret = -1;
    switch(profile)
    {
    case (AAC_DEFAULT_PROFILE):
        break;
    case (AAC_ELD):
        ret = FF_PROFILE_AAC_ELD;
        break;
    case (AAC_HE):
        ret = FF_PROFILE_AAC_HE;
        break;
    case (AAC_HE_V2):
        ret = FF_PROFILE_AAC_HE_V2;
        break;
    case (AAC_LD):
        ret = FF_PROFILE_AAC_LD;
        break;
    case (AAC_LOW):
        ret = FF_PROFILE_AAC_LOW;
        break;
    case (AAC_LTP):
        ret = FF_PROFILE_AAC_LTP;
        break;
    case (AAC_MAIN):
        ret = FF_PROFILE_AAC_MAIN;
        break;
    case (AAC_SSR):
        ret = FF_PROFILE_AAC_SSR;
        break;
    default:
        break;
    }
    return ret;
}

const char* convertToFFMPEGPreset(enum H264Presets preset)
{
    const char* ret = "";
    switch(preset)
    {
    case (H264_ULTRAFAST):
        ret = "ultrafast";
        break;
    case (H264_SUPERFAST):
        ret = "superfast";
        break;
    case (H264_VERY_FAST):
        ret = "veryfast";
        break;
    case (H264_FASTER):
        ret = "faster";
        break;
    case (H264_FAST):
        ret = "fast";
        break;
    case (H264_MEDIUM):
        ret = "medium";
        break;
    case (H264_SLOW):
        ret = "slow";
        break;
    case (H264_SLOWER):
        ret = "slower";
        break;
    case (H264_PLACEBO):
        ret = "placebo";
        break;
    default:
        break;
    }
    return ret;
}

const char* convertToFFMPEGTune(enum H264Tunes tune)
{
    const char* ret = "";
    switch(tune)
    {
    case (H264_FILM):
        ret = "film";
        break;
    case (H264_ANIMATION):
        ret = "animation";
        break;
    case (H264_GRAIN):
        ret = "grain";
        break;
    case (H264_STILLIMAGE):
        ret = "stillimage";
        break;
    case (H264_FASTDECODE):
        ret = "fastdecode";
        break;
    case (H264_ZEROLATENCY):
        ret = "zerolatency";
        break;
    case (H264_PSNR):
        ret = "psnr";
        break;
    case (H264_SSIM):
        ret = "ssim";
        break;        
    default:
        break;
    }
    return ret;
}

}

#endif // FFMPEGCOMMON_HPP_INCLUDED

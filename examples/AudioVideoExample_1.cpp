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
 * --------------------------------------------
 *
 * This example grabs raw audio and MJPEG video from a V4L/ALSA device and:
 * 
 * 1) transcodes (decoding --> encoding) MJPEG to H264
 * 2) encodes raw audio to AAC
 * 3) streams MJPEG video (HTTP-MATROSKA)
 * 4) streams H264 video (HTTP-MPEGTS)
 * 5) streams AAC audio (both HTTP-MATROSKA and HTTP-MPEGTS)
 * 6) streams audio and video together (both HTTP-MATROSKA(MJPEG) and HTTP-MPEGTS(H264))
 * 
 * You can find the ALSAdevice identifier with:
 * 
 *   arecord -L
 * 
 * The streams' addresses are printed in the output log
 * 
 */
#include "LAAV.hpp"

using namespace laav;

typedef UnsignedConstant<16000> SAMPLERATE;
typedef UnsignedConstant<640> WIDTH;
typedef UnsignedConstant<480> HEIGHT;

int main(int argc, char** argv)
{

    if (argc < 3) 
    {
        std::cout << "Usage: " << argv[0]
        << " alsa-device-identifier[i.e: plughw:U0x46d0x819] /path/to/v4l/device" << std::endl;
        
        return 1;
    }    
 
    LAAVLogLevel = 1;
    std::string addr = "127.0.0.1";
    
    SharedEventsCatcher eventsCatcher = EventsManager::createSharedEventsCatcher();

    AlsaGrabber <S16_LE, SAMPLERATE, MONO>
    aGrab(eventsCatcher, argv[1], DEFAULT_SAMPLERATE);

    FFMPEGAudioConverter <S16_LE, SAMPLERATE, MONO, FLOAT_PLANAR, SAMPLERATE, STEREO>
    aConv;

    FFMPEGADTSAACEncoder <SAMPLERATE, STEREO>
    aEnc;

    AudioFrameHolder <AAC, SAMPLERATE, STEREO>
    aFh;

    HTTPAudioStreamer <MPEGTS, AAC, SAMPLERATE, STEREO>
    aStream_1(eventsCatcher, addr, 8080);

    HTTPAudioStreamer <MATROSKA, AAC, SAMPLERATE, STEREO>
    aStream_2(eventsCatcher, addr, 8081, aEnc);    
    
    V4L2Grabber <MJPEG, WIDTH, HEIGHT>
    vGrab(eventsCatcher, argv[2], DEFAULT_FRAMERATE);

    FFMPEGMJPEGDecoder <WIDTH, HEIGHT>
    vDec;

    FFMPEGVideoConverter <YUV422_PLANAR, WIDTH, HEIGHT, YUV420_PLANAR, WIDTH, HEIGHT>
    vConv;

    FFMPEGH264Encoder <YUV420_PLANAR, WIDTH, HEIGHT>
    vEnc(DEFAULT_BITRATE, 5, H264_ULTRAFAST, H264_DEFAULT_PROFILE, H264_DEFAULT_TUNE);

    VideoFrameHolder <MJPEG, WIDTH, HEIGHT>
    vFh_1;

    VideoFrameHolder <H264, WIDTH, HEIGHT>
    vFh_2;

    HTTPVideoStreamer <MPEGTS, H264, WIDTH, HEIGHT>
    vStream_1(eventsCatcher, addr, 8082);

    HTTPVideoStreamer <MATROSKA, MJPEG, WIDTH, HEIGHT>
    vStream_2(eventsCatcher, addr, 8083);

    HTTPAudioVideoStreamer <MPEGTS, H264, WIDTH, HEIGHT, AAC, SAMPLERATE, STEREO>
    avStream_1(eventsCatcher, addr, 8084);

    HTTPAudioVideoStreamer <MATROSKA, MJPEG, WIDTH, HEIGHT, AAC, SAMPLERATE, STEREO>
    avStream_2(eventsCatcher, addr, 8085, aEnc);

    while (!LAAVStop)
    {
        aGrab >> aConv >> aEnc >> aFh;
                                  aFh >> aStream_1;
                                  aFh >> aStream_2;
                                  aFh >> avStream_1;
                                  aFh >> avStream_2;

        vGrab >> vFh_1;
                 // H264 sub-pipe
                 vFh_1 >> vDec >> vConv >> vEnc >> vFh_2;
                                                   vFh_2 >> vStream_1;
                                                   vFh_2 >> avStream_1;
                 // MJPEG sub-pipe
                 vFh_1 >> vStream_2;
                 vFh_1 >> avStream_2;

        eventsCatcher->catchNextEvent();
    }

    return 0;
    
}

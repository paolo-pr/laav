/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
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
 * The streams' addresses are:
 *
 *   (MPEGTS - ADTS_AAC)
 *   http://127.0.0.1:8080/stream.ts 
 * 
 *   (MATROSKA - ADTS_AAC)
 *   http://127.0.0.1:8081/stream.mkv 
 * 
 *   (MPEGTS - H264)
 *   http://127.0.0.1:8082/stream.ts 
 * 
 *   (MATROSKA - MJPEG)
 *   http://127.0.0.1:8083/stream.mkv
 *
 *   (MPEGTS - H264 - ADTS_AAC)
 *   http://127.0.0.1:8084/stream.ts
 *
 *   (MATROSKA - MJPEG - ADTS_AAC)
 *   http://127.0.0.1:8085/stream.mkv
 * 
 */

#include "AlsaGrabber.hpp"
#include "V4L2Grabber.hpp"

#define SAMPLE_RATE 16000
#define WIDTH 640
#define HEIGHT 480

using namespace laav;

int main(int argc, char** argv)
{

    if (argc < 3) 
    {
        std::cout << "Usage: " << argv[0]
        << " alsa-device-identifier[i.e: plughw:U0x46d0x819] /path/to/v4l/device" << std::endl;
        
        return 1;
    }    
    
    std::string addr = "127.0.0.1";
    
    SharedEventsCatcher eventsCatcher = EventsManager::createSharedEventsCatcher();

    AlsaGrabber <S16_LE, SAMPLE_RATE, MONO>
    aGrab(eventsCatcher, argv[1]);

    FFMPEGAudioConverter <S16_LE, SAMPLE_RATE, MONO, FLOAT_PLANAR, SAMPLE_RATE, STEREO>
    aConv;

    FFMPEGADTSAACEncoder <SAMPLE_RATE, STEREO>
    aEnc;

    AudioFrameHolder <ADTS_AAC, SAMPLE_RATE, STEREO>
    aFh;

    HTTPAudioStreamer <MPEGTS, ADTS_AAC, SAMPLE_RATE, STEREO>
    aStream_1(eventsCatcher, addr, 8080);

    HTTPAudioStreamer <MATROSKA, ADTS_AAC, SAMPLE_RATE, STEREO>
    aStream_2(eventsCatcher, addr, 8081);    
    
    V4L2Grabber <MJPEG, WIDTH, HEIGHT>
    vGrab(eventsCatcher, argv[2]);

    FFMPEGMJPEGDecoder <WIDTH, HEIGHT>
    vDec;

    FFMPEGVideoConverter <YUV422_PLANAR, WIDTH, HEIGHT, YUV420_PLANAR, WIDTH, HEIGHT>
    vConv;

    FFMPEGH264Encoder <YUV420_PLANAR, WIDTH, HEIGHT>
    vEnc(DEFAULT_BITRATE, 5, H264_ULTRAFAST, H264_DEFAULT_PROFILE);

    VideoFrameHolder <MJPEG, WIDTH, HEIGHT>
    vFh_1;

    VideoFrameHolder <H264, WIDTH, HEIGHT>
    vFh_2;

    HTTPVideoStreamer <MPEGTS, H264, WIDTH, HEIGHT>
    vStream_1(eventsCatcher, addr, 8082);

    HTTPVideoStreamer <MATROSKA, MJPEG, WIDTH, HEIGHT>
    vStream_2(eventsCatcher, addr, 8083);

    HTTPAudioVideoStreamer <MPEGTS, H264, WIDTH, HEIGHT, ADTS_AAC, SAMPLE_RATE, STEREO>
    avStream_1(eventsCatcher, addr, 8084);

    HTTPAudioVideoStreamer <MATROSKA, MJPEG, WIDTH, HEIGHT, ADTS_AAC, SAMPLE_RATE, STEREO>
    avStream_2(eventsCatcher, addr, 8085);

    while (1)
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

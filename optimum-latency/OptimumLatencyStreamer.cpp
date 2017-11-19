/* 
 * Created (11/10/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 * This example grabs raw audio and video from a V4L/ALSA device,
 * encodes raw video to H264 with ZEROLATENCY (H264) tune, encodes 
 * raw audio to AAC and streams audio and video, both together and 
 * separately, over UDP-MPEGTS.
 * See README.md included in this directory.
 * 
 * You can find the ALSAdevice identifier with:
 * 
 *   arecord -L
 * 
 * The streams' UDP ports are:
 *
 *   (MPEGTS - ADTS_AAC) port 8080
 * 
 *   (MPEGTS - H264) port 8081
 * 
 *   (MPEGTS - H264 - ADTS_AAC) port 8082
 * 
 */

#include "AlsaGrabber.hpp"
#include "V4L2Grabber.hpp"

#include <unistd.h>

#define SAMPLE_RATE 44100
#define WIDTH 640
#define HEIGHT 480

using namespace laav;

int main(int argc, char** argv)
{

    if (argc < 4) 
    {
        std::cout << "Usage: " << argv[0]
        << " alsa-device-identifier[i.e: plughw:U0x46d0x819] /path/to/v4l/device receiver_address" 
        << std::endl;
        
        return 1;
    }    
    
    SharedEventsCatcher eventsCatcher = EventsManager::createSharedEventsCatcher();


    AlsaGrabber <S16_LE, SAMPLE_RATE, MONO>
    aGrab(eventsCatcher, argv[1], 128*4);

    FFMPEGAudioConverter <S16_LE, SAMPLE_RATE, MONO, FLOAT_PLANAR, SAMPLE_RATE, STEREO>
    aConv;

    FFMPEGADTSAACEncoder <SAMPLE_RATE, STEREO>
    aEnc;

    AudioFrameHolder <ADTS_AAC, SAMPLE_RATE, STEREO>
    aFh;

    V4L2Grabber <YUYV422_PACKED, WIDTH, HEIGHT>
    vGrab(eventsCatcher, argv[2]);

    FFMPEGVideoConverter <YUYV422_PACKED, WIDTH, HEIGHT, YUV420_PLANAR, WIDTH, HEIGHT>
    vConv;

    FFMPEGH264Encoder <YUV420_PLANAR, WIDTH, HEIGHT>
    vEnc(DEFAULT_BITRATE, 25, H264_ULTRAFAST, H264_DEFAULT_PROFILE, H264_ZEROLATENCY);
    
    VideoFrameHolder <H264, WIDTH, HEIGHT>
    vFh;    

    UDPAudioVideoStreamer <MPEGTS, H264, WIDTH, HEIGHT, ADTS_AAC, SAMPLE_RATE, STEREO>
    avStream(argv[3], 8082);
    
    std::cerr << std::endl;
    std::cerr << "********************************" << std::endl;
    printCurrDate();
    std::cerr << "START GRABBING " << std::endl;
    
    bool firstAudioFrameEncoded = false;
    int  videoReady = 0;

    /*
     * grab some video frames, so to ensure that the v4l device is
     * actually ready to stream
     */
    while (videoReady < 5)
    {
        try
        {
            vGrab.grabNextFrame();          
            videoReady++;
        }
        catch (const MediaException& me) {}
        eventsCatcher->catchNextEvent();        
    }
    
    while (1)
    {

        aGrab >> aConv >> aEnc >> aFh;

        try
        {
            AudioFrame <ADTS_AAC, SAMPLE_RATE, STEREO>& encodedAudioFrame = aFh.get();          
            printCurrDate();
            if (firstAudioFrameEncoded)
                std::cerr << "AUDIO ENCODER   size="; 
            else
                std::cerr << "FIRST ENCODED AUDIO FRAME  size=";
            
            std::cerr << encodedAudioFrame.size()            
                      << std::endl << std::endl;
                      
            firstAudioFrameEncoded = true;
        } 
        catch (const MediaException& me) {}

                                  aFh >> avStream;

        vGrab >> vConv >> vEnc >> vFh;                                  
                                  
        try
        {
            VideoFrame <H264, WIDTH, HEIGHT>& encodedVideoFrame = vFh.get();
            printCurrDate();
            std::cerr << "VIDEO ENCODER   size=" << (encodedVideoFrame.size()+7)            
                      << std::endl << std::endl;
        } 
        catch (const MediaException& me) {}                                  

                                  vFh >> avStream;

        eventsCatcher->catchNextEvent();
    }

    return 0;
    
}

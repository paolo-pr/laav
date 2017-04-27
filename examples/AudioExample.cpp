/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 * This example grabs audio from an ALSA device and
 * 
 * 1) encodes (AAC) and streams it through HTTP with a MPEGTS container.
 * 2) encodes (MP2) and streams it through HTTP with a MATROSKA container.
 * 
 * You can find the alsa-identifier of the audio input device with:
 * 
 *   arecord -L
 * 
 * The streams' addresses are:
 *
 *   (MATROSKA - MP2)
 *   http://127.0.0.1:8080/stream.mkv 
 * 
 *   (MPEGTS - AAC) 
 *   http://127.0.0.1:8081/stream.ts
 *
 */

#include "AlsaGrabber.hpp"

#define SAMPLE_RATE 16000

using namespace laav;

int main(int argc, char** argv)
{

    if (argc < 2) 
    {
        std::cout << "Usage: " << argv[0] 
        << " alsa-device-identifier[i.e: plughw:U0x46d0x819]" << std::endl;
        
        return 1;
    }    

    std::string addr = "127.0.0.1";    
    
    SharedEventsCatcher eventsCatcher = EventsManager::createSharedEventsCatcher();
   
    AlsaGrabber <S16_LE, SAMPLE_RATE, MONO>
    aGrab(eventsCatcher, argv[1]);
    
    // Splits the main pipe into two sub-pipes
    AudioFrameHolder <S16_LE, SAMPLE_RATE, MONO>
    aFh;    

    // Needed for the ADTS_AAC encoder (it accepts only FLOAT_PLANAR fmt)
    FFMPEGAudioConverter <S16_LE, SAMPLE_RATE, MONO, FLOAT_PLANAR, SAMPLE_RATE, STEREO>
    aConv;

    FFMPEGMP2Encoder <S16_LE, SAMPLE_RATE, MONO>
    aEnc_1;    
    
    FFMPEGADTSAACEncoder <SAMPLE_RATE, STEREO>
    aEnc_2;
    
    HTTPAudioStreamer <MATROSKA, MP2, SAMPLE_RATE, MONO>
    aStream_1(eventsCatcher, addr, 8080);    

    HTTPAudioStreamer <MPEGTS, ADTS_AAC, SAMPLE_RATE, STEREO>
    aStream_2(eventsCatcher, addr, 8081);    
    
    while (1)
    {
        aGrab >> aFh;
                 // MP2 sub-pipe
                 aFh >> aEnc_1 >> aStream_1;
                 // AAC sub-pipe
                 aFh >> aConv >> aEnc_2 >> aStream_2;
        
        eventsCatcher->catchNextEvent();
    }
    
    return 0;

}

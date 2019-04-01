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
 * This example grabs audio from an ALSA device and
 * 
 * 1) encodes (AAC) and streams it through HTTP with a MPEGTS container.
 * 2) encodes (MP2) and streams it through HTTP with a MATROSKA container.
 * 
 * You can find the alsa-identifier of the audio input device with:
 * 
 *   arecord -L
 * 
 * The streams' addresses are printed in the output log.
 *
 */

#include "LAAV.hpp"

using namespace laav;

typedef UnsignedConstant<16000> SAMPLERATE;

int main(int argc, char** argv)
{

    if (argc < 2) 
    {
        std::cout << "Usage: " << argv[0] 
        << " alsa-device-identifier[i.e: plughw:U0x46d0x819]" << std::endl;
        
        return 1;
    }    

    LAAVLogLevel = 1;
    std::string addr = "127.0.0.1";    
    
    SharedEventsCatcher eventsCatcher = EventsManager::createSharedEventsCatcher();
   
    AlsaGrabber <S16_LE, SAMPLERATE, MONO>
    aGrab(eventsCatcher, argv[1], DEFAULT_SAMPLERATE);
    
    // Split the main pipe into two sub-pipes
    AudioFrameHolder <S16_LE, SAMPLERATE, MONO>
    aFh;    
    
    // Needed for the ADTS_AAC encoder (it accepts only FLOAT_PLANAR fmt)
    FFMPEGAudioConverter <S16_LE, SAMPLERATE, MONO, FLOAT_PLANAR, SAMPLERATE, STEREO>
    aConv;

    FFMPEGMP2Encoder <S16_LE, SAMPLERATE, MONO>
    aEnc_1;    
    
    FFMPEGADTSAACEncoder <SAMPLERATE, STEREO>
    aEnc_2;
    
    HTTPAudioStreamer <MATROSKA, MP2, SAMPLERATE, MONO>
    aStream_1(eventsCatcher, addr, 8080, aEnc_1);    

    HTTPAudioStreamer <MPEGTS, ADTS_AAC, SAMPLERATE, STEREO>
    aStream_2(eventsCatcher, addr, 8081);    
    
    while (!LAAVStop)
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

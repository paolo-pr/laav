/* 
 * Created (3/3/2019) by Paolo-Pr.
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
 * See README.md
 * 
 */

#include "LAAV.hpp"

using namespace laav;

typedef UnsignedConstant<48000> SAMPLERATE;
typedef UnsignedConstant<640> WIDTH;
typedef UnsignedConstant<480> HEIGHT;

int main(int argc, char** argv)
{

    if (argc < 4) 
    {
        std::cout << "Usage: " << argv[0]
        << " alsa-device-identifier[i.e: plughw:U0x46d0x819] /path/to/v4l/device receiverAddress" 
        << std::endl;
        
        return 1;
    }    
    
    LAAVLogLevel = 1;
    SharedEventsCatcher eventsCatcher = EventsManager::createSharedEventsCatcher();

    AlsaGrabber <S16_LE, SAMPLERATE, MONO>
    aGrab(eventsCatcher, argv[1], 128*4);

    FFMPEGAudioConverter <S16_LE, SAMPLERATE, MONO, S16_LE, SAMPLERATE, STEREO>
    aConv;
   
    FFMPEGOpusEncoder <S16_LE, SAMPLERATE, STEREO>
    aEnc(OPUS_LOWDELAY, OPUS_DEFAULT_COMPRESSION_LEVEL, OPUS_DEFAULT_BITRATE);

    AudioFrameHolder <OPUS, SAMPLERATE, STEREO>
    aFh;

    V4L2Grabber <YUYV422_PACKED, WIDTH, HEIGHT>
    vGrab(eventsCatcher, argv[2]);

    FFMPEGVideoConverter <YUYV422_PACKED, WIDTH, HEIGHT, YUV420_PLANAR, WIDTH, HEIGHT>
    vConv;

    FFMPEGH264Encoder <YUV420_PLANAR, WIDTH, HEIGHT>
    vEnc(DEFAULT_BITRATE, 5, H264_ULTRAFAST, H264_DEFAULT_PROFILE, H264_ZEROLATENCY);
    
    VideoFrameHolder <H264, WIDTH, HEIGHT>
    vFh;    

    UDPAudioStreamer <MPEGTS, OPUS, SAMPLERATE, STEREO>
    aStream(argv[3], 8082);    

    UDPVideoStreamer <MPEGTS, H264, WIDTH, HEIGHT>
    vStream(argv[3], 8083);    
    
    UDPAudioVideoStreamer <MPEGTS, H264, WIDTH, HEIGHT, OPUS, SAMPLERATE, STEREO>
    avStream(argv[3], 8084);
        
    bool firstVideoFrameGrabbbed = false;
    bool firstVideoFrameEncoded = false;
    bool firstVideoFrameStreamed = false;
    bool firstAudioFrameGrabbed = false;
    bool firstAudioFrameEncoded = false;
    bool firstAudioFrameStreamed = false;    
    bool firstAudioVideoFrameStreamed = false;
      
    int  videoReady = 0;

    /*
     * grab some video frames, so to ensure that the v4l device is
     * actually ready to stream
     */
    while (videoReady < 5 && !LAAVStop)
    {
        try
        {
            vGrab.grabNextFrame();          
            videoReady++;
        }
        catch (const MediumException& me) {}
        eventsCatcher->catchNextEvent();        
    }    
    
    while (!LAAVStop)
    {
        aGrab >> aConv >> aEnc >> aFh;        
                                  aFh >> aStream;
                                  aFh >> avStream;

        vGrab >> vConv >> vEnc >> vFh; 
                                  vFh >> vStream;
                                  vFh >> avStream;
                         
        if (vGrab.outputStatus() == READY && !firstVideoFrameGrabbbed)
        {
            printCurrDate();
            std::cerr << " [VIDEO] Grabbed first frame " << std::endl; 
            firstVideoFrameGrabbbed = true;
        }
        
        if (vEnc.outputStatus() == READY && !firstVideoFrameEncoded)
        {
            printCurrDate();
            std::cerr << " [VIDEO] Encoded first frame " << std::endl; 
            firstVideoFrameEncoded = true;
        }
        
        if (vStream.outputStatus() == READY && !firstVideoFrameStreamed)
        {
                printCurrDate();
                std::cerr << " [VIDEO] Streamed first frame" << std::endl; 
                firstVideoFrameStreamed = true;
        } 
        
        if (aGrab.outputStatus() == READY && !firstAudioFrameGrabbed)
        {
            printCurrDate();
            std::cerr << " [AUDIO] Grabbed first frame " << std::endl; 
            firstAudioFrameGrabbed = true;
        }
        
        if (aEnc.outputStatus() == READY && !firstAudioFrameEncoded)
        {
            printCurrDate();
            std::cerr << " [AUDIO] Encoded first frame " << std::endl; 
            firstAudioFrameEncoded = true;
        }
        
        if (aStream.outputStatus() == READY && !firstAudioFrameStreamed)
        {
            printCurrDate();
            std::cerr << " [AUDIO] Streamed first frame" << std::endl; 
            firstAudioFrameStreamed = true;
        }         
        
        if (avStream.outputStatus() == READY && !firstAudioVideoFrameStreamed)
        {
            printCurrDate();
            std::cerr << " [AUDIO + VIDEO] Streamed first frame" << std::endl; 
            firstAudioVideoFrameStreamed = true;
        }         
        
        try
        {
            AudioFrame<OPUS, SAMPLERATE, STEREO> encodedAudioFrame =  aFh.get();
            printCurrDate();
            std::cerr << " [AUDIO] Encoded data: size=" << encodedAudioFrame.size() << 
            " ts=" << encodedAudioFrame.monotonicTimestamp() << std::endl;
        } 
        catch (const MediumException& me) {}     
        
        try
        {
            VideoFrame<H264, WIDTH, HEIGHT> encodedVideoFrame =  vFh.get();
            printCurrDate();
            std::cerr << " [VIDEO] Encoded data: size=" << encodedVideoFrame.size() << 
            " ts=" << encodedVideoFrame.monotonicTimestamp() << std::endl;
        } 
        catch (const MediumException& me) {}        
        
        eventsCatcher->catchNextEvent();
    }

    return 0;
    
}

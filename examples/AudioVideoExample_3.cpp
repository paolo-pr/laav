/* 
 * Created (25/03/2019) by Paolo-Pr.
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
 * This example encodes, streams and records in many formats.
 * Recordings are managed by a MediaManager instance.
 * The streams' addressess are printed in the output log.
 * The recorders are controlled by a HTTP server (HTTPCommandsReceiver) 
 * which receives the id of the recorder (RECORDER_ID) or the token "ALL"
 * and executes start/stop recording commands as simple HTTP 
 * request strings.
 * 
 * The HTTP commands can be sent with cURL utility:
 * 
 *   (set output filename and start recording)
 *   curl --data "startRecording=[RECORDER_ID or ALL]" http://127.0.0.1:8094/commands
 * 
 *   (stop recording)
 *   curl --data "stopRecording=[RECORDER_ID or ALL]" http://127.0.0.1:8094/commands
 * 
 * Recorders' ids are uppercase and in the form: [container]-[audio codec]-[video codec] or 
 * [container]-[audio codec] or [container]-[video codec] (see the parameters passed to the muxers' ctors)
 * Example: MPEGTS-H264-AAC, MATROSKA-OPUS etc.
 * Recording files are generated in the current directory, and their names contain the recorder's id and the
 * recording date.
 *
 */
#include "LAAV.hpp"
#include <list>

using namespace laav;

typedef UnsignedConstant<48000> SAMPLERATE;
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
    aGrab(eventsCatcher, argv[1], 128*4);

    V4L2Grabber <YUYV422_PACKED, WIDTH, HEIGHT>
    vGrab(eventsCatcher, argv[2], DEFAULT_FRAMERATE);

    FFMPEGVideoConverter <YUYV422_PACKED, WIDTH, HEIGHT, YUV420_PLANAR, WIDTH, HEIGHT>
    vConv;

    FFMPEGH264Encoder <YUV420_PLANAR, WIDTH, HEIGHT>
    vEnc(DEFAULT_BITRATE, DEFAULT_GOPSIZE, H264_ULTRAFAST, H264_DEFAULT_PROFILE, H264_DEFAULT_TUNE);

    VideoFrameHolder <H264, WIDTH, HEIGHT>
    vFh;    
    
    AudioFrameHolder <S16_LE, SAMPLERATE, MONO>
    aFh_1;    
    
    AudioFrameHolder <ADTS_AAC, SAMPLERATE, STEREO>
    aFh_1_1;    
    
    AudioFrameHolder <MP2, SAMPLERATE, STEREO>
    aFh_1_2_1;     
    
    AudioFrameHolder <OPUS, SAMPLERATE, STEREO>
    aFh_1_2_2;
    
    AudioFrameHolder <S16_LE, SAMPLERATE, STEREO>
    aFh_1_2;    
    
    FFMPEGAudioConverter <S16_LE, SAMPLERATE, MONO, FLOAT_PLANAR, SAMPLERATE, STEREO>
    aConv_1;

    FFMPEGAudioConverter <S16_LE, SAMPLERATE, MONO, S16_LE, SAMPLERATE, STEREO>
    aConv_2;    
    
    FFMPEGADTSAACEncoder <SAMPLERATE, STEREO>
    aEnc_1;

    FFMPEGMP2Encoder <S16_LE, SAMPLERATE, STEREO>
    aEnc_2;    
    
    FFMPEGOpusEncoder <S16_LE, SAMPLERATE, STEREO>
    aEnc_3;    

    FFMPEGAudioMuxer <MPEGTS, ADTS_AAC, SAMPLERATE, STEREO>
    aMux_1("MPEGTS-AAC");    
    
    FFMPEGAudioMuxer <MATROSKA, ADTS_AAC, SAMPLERATE, STEREO>
    aMux_2(aEnc_1, "MATROSKA-AAC");   
    
    FFMPEGAudioMuxer <MPEGTS, MP2, SAMPLERATE, STEREO>
    aMux_3("MPEGTS-MP2");
    
    FFMPEGAudioMuxer <MATROSKA, MP2, SAMPLERATE, STEREO>
    aMux_4(aEnc_2, "MATROSKA-MP2");
    
    FFMPEGAudioMuxer <MPEGTS, OPUS, SAMPLERATE, STEREO>
    aMux_5("MPEGTS-OPUS");

    FFMPEGAudioMuxer <MATROSKA, OPUS, SAMPLERATE, STEREO>
    aMux_6(aEnc_3, "MATROSKA-OPUS");
  
    FFMPEGVideoMuxer <MPEGTS, H264, WIDTH, HEIGHT>
    vMux_1("MPEGTS-H264");
    
    FFMPEGVideoMuxer <MATROSKA, H264, WIDTH, HEIGHT>
    vMux_2(vEnc, "MATROSKA-H264");    

    FFMPEGAudioVideoMuxer <MPEGTS, H264, WIDTH, HEIGHT, ADTS_AAC, SAMPLERATE, STEREO>
    avMux_1("MPEGTS-H264-AAC");       
    
    FFMPEGAudioVideoMuxer <MATROSKA, H264, WIDTH, HEIGHT, ADTS_AAC, SAMPLERATE, STEREO>
    avMux_2(aEnc_1, vEnc, "MATROSKA-H264-AAC");    
  
    FFMPEGAudioVideoMuxer <MPEGTS, H264, WIDTH, HEIGHT, MP2, SAMPLERATE, STEREO>
    avMux_3("MPEGTS-H264-MP2");    
    
    FFMPEGAudioVideoMuxer <MATROSKA, H264, WIDTH, HEIGHT, MP2, SAMPLERATE, STEREO>
    avMux_4(aEnc_2, vEnc, "MATROSKA-H264-MP2"); 
    
    FFMPEGAudioVideoMuxer <MPEGTS, H264, WIDTH, HEIGHT, OPUS, SAMPLERATE, STEREO>
    avMux_5("MPEGTS-H264-OPUS");    
    
    FFMPEGAudioVideoMuxer <MATROSKA, H264, WIDTH, HEIGHT, OPUS, SAMPLERATE, STEREO>
    avMux_6(aEnc_3, vEnc,"MATROSKA-H264-OPUS");    
    
    HTTPAudioStreamer <MPEGTS, ADTS_AAC, SAMPLERATE, STEREO>
    aStream_1(eventsCatcher, addr, 8080);

    HTTPAudioStreamer <MATROSKA, ADTS_AAC, SAMPLERATE, STEREO>
    aStream_2(eventsCatcher, addr, 8087, aEnc_1); 
    
    HTTPAudioStreamer <MPEGTS, MP2, SAMPLERATE, STEREO>
    aStream_3(eventsCatcher, addr, 8081);

    HTTPAudioStreamer <MATROSKA, MP2, SAMPLERATE, STEREO>
    aStream_4(eventsCatcher, addr, 8088, aEnc_2);
 
    HTTPAudioStreamer <MPEGTS, OPUS, SAMPLERATE, STEREO>
    aStream_5(eventsCatcher, addr, 8082);

    HTTPAudioStreamer <MATROSKA, OPUS, SAMPLERATE, STEREO>
    aStream_6(eventsCatcher, addr, 8089, aEnc_3);    
    
    HTTPVideoStreamer <MPEGTS, H264, WIDTH, HEIGHT>
    vStream_1(eventsCatcher, addr, 8083);

    HTTPVideoStreamer <MATROSKA, H264, WIDTH, HEIGHT>
    vStream_2(eventsCatcher, addr, 8090, vEnc);    
    
    HTTPAudioVideoStreamer <MPEGTS, H264, WIDTH, HEIGHT, ADTS_AAC, SAMPLERATE, STEREO>
    avStream_1(eventsCatcher, addr, 8084);

    HTTPAudioVideoStreamer <MATROSKA, H264, WIDTH, HEIGHT, ADTS_AAC, SAMPLERATE, STEREO>
    avStream_2(eventsCatcher, addr, 8091, aEnc_1, vEnc); 
    
    HTTPAudioVideoStreamer <MPEGTS, H264, WIDTH, HEIGHT, MP2, SAMPLERATE, STEREO>
    avStream_3(eventsCatcher, addr, 8085);

    HTTPAudioVideoStreamer <MATROSKA, H264, WIDTH, HEIGHT, MP2, SAMPLERATE, STEREO>
    avStream_4(eventsCatcher, addr, 8092, aEnc_2, vEnc);
 
    HTTPAudioVideoStreamer <MPEGTS, H264, WIDTH, HEIGHT, OPUS, SAMPLERATE, STEREO>
    avStream_5(eventsCatcher, addr, 8086);

    HTTPAudioVideoStreamer <MATROSKA, H264, WIDTH, HEIGHT, OPUS, SAMPLERATE, STEREO>
    avStream_6(eventsCatcher, addr, 8093, aEnc_3, vEnc);
    
    HTTPCommandsReceiver
    commandsReceiver(eventsCatcher, addr, 8094);    
    
    auto mm = MediaManagerFactory::createMediaManager
    (vMux_1,  vMux_2,  
     aMux_1,  aMux_2,  aMux_3,  aMux_4,  aMux_5,  aMux_6, 
     avMux_1, avMux_2, avMux_3, avMux_4, avMux_5, avMux_6);    
    
    std::list<std::string> recordersIds;
    recordersIds.push_back(vMux_1.id());
    recordersIds.push_back(vMux_2.id());
    recordersIds.push_back(aMux_1.id());
    recordersIds.push_back(aMux_2.id());
    recordersIds.push_back(aMux_3.id());
    recordersIds.push_back(aMux_4.id());
    recordersIds.push_back(aMux_5.id());
    recordersIds.push_back(aMux_6.id());
    recordersIds.push_back(avMux_1.id());
    recordersIds.push_back(avMux_2.id());
    recordersIds.push_back(avMux_3.id());
    recordersIds.push_back(avMux_4.id());
    recordersIds.push_back(avMux_5.id());
    recordersIds.push_back(avMux_6.id());
    
    while (!LAAVStop)
    {
        /**/aGrab >> aFh_1; 
        /**/         aFh_1 >> aConv_1 >> aEnc_1 >> aFh_1_1;
        /**/         /**/                          aFh_1_1 >> aMux_1;                  //MPEGTS-AAC 
        /**/         /**/                          aFh_1_1 >> aMux_2;                  //MATROSKA-AAC       
        /**/         /**/                          aFh_1_1 >> aStream_1;               //MPEGTS-AAC 
        /**/         /**/                          aFh_1_1 >> aStream_2;               //MATROSKA-AAC   
        /**/         /**/                          aFh_1_1 >> avMux_1;                 //MPEGTS-H264-AAC
        /**/         /**/                          aFh_1_1 >> avMux_2;                 //MATROSKA-H264-AAC         
        /**/         /**/                          aFh_1_1 >> avStream_1;              //MPEGTS-H264-AAC
        /**/         /**/                          aFh_1_1 >> avStream_2;              //MATROSKA-H264-AAC        
        /**/         /**/                      
        /**/         aFh_1 >> aConv_2 >> aFh_1_2; 
        /**/                             aFh_1_2 >> aEnc_2 >> aFh_1_2_1;
        /**/                             /**/                 aFh_1_2_1 >> aMux_3;     //MPEGTS-MP2
        /**/                             /**/                 aFh_1_2_1 >> aMux_4;     //MATROSKA-MP2
        /**/                             /**/                 aFh_1_2_1 >> aStream_3;  //MPEGTS-MP2
        /**/                             /**/                 aFh_1_2_1 >> aStream_4;  //MATROSKA-MP2
        /**/                             /**/                 aFh_1_2_1 >> avMux_3;    //MPEGTS-H264-MP2
        /**/                             /**/                 aFh_1_2_1 >> avMux_4;    //MATROSKA-H264-MP2
        /**/                             /**/                 aFh_1_2_1 >> avStream_3; //MPEGTS-H264-MP2
        /**/                             /**/                 aFh_1_2_1 >> avStream_4; //MATROSKA-H264-MP2
        /**/                             /**/
        /**/                             aFh_1_2 >> aEnc_3 >> aFh_1_2_2;
        /**/                                                  aFh_1_2_2 >> aMux_5;     //MPEGTS-OPUS
        /**/                                                  aFh_1_2_2 >> aMux_6;     //MATROSKA-OPUS        
        /**/                                                  aFh_1_2_2 >> aStream_5;  //MPEGTS-OPUS 
        /**/                                                  aFh_1_2_2 >> aStream_6;  //MATROSKA-OPUS     
        /**/                                                  aFh_1_2_2 >> avMux_5;    //MPEGTS-H264-OPUS  
        /**/                                                  aFh_1_2_2 >> avMux_6;    //MATROSKA-H264-OPUS                
        /**/                                                  aFh_1_2_2 >> avStream_5; //MPEGTS-H264-OPUS  
        /**/                                                  aFh_1_2_2 >> avStream_6; //MATROSKA-H264-OPUS       
        /**/                          
        /**/vGrab >> vConv >> vEnc >> vFh; 
                                      vFh >> vMux_1;                                   //MPEGTS-H264
                                      vFh >> vMux_2;                                   //MATROSKA-H264                                       
                                      vFh >> vStream_1;                                //MPEGTS-H264
                                      vFh >> vStream_2;                                //MATROSKA-H264        
                                      vFh >> avMux_1;
                                      vFh >> avMux_2;
                                      vFh >> avMux_3;  
                                      vFh >> avMux_4;
                                      vFh >> avMux_5;
                                      vFh >> avMux_6;                                      
                                      vFh >> avStream_1;
                                      vFh >> avStream_2;
                                      vFh >> avStream_3;       
                                      vFh >> avStream_4;
                                      vFh >> avStream_5;
                                      vFh >> avStream_6;                                      
                                  
        // Decode and execute HTTP commands
        try
        {        
            std::map<std::string, std::string>& cmds = commandsReceiver.receivedCommands();
            printCurrDate();
            std::cerr << " Received HTTP cmd" << std::endl;
            try
            {
                if (cmds.find("stop") != cmds.end())
                    break;
                else if (cmds.find("startRecording") != cmds.end())
                {
                    vEnc.generateKeyFrame();
                    if (cmds["startRecording"] == "ALL")
                    {
                        for (auto id : recordersIds)
                            mm.request<bool>(id, START_REC, DEFAULT_REC_FILENAME);
                    }
                    else
                    {
                        mm.request<bool>(cmds["startRecording"], START_REC, DEFAULT_REC_FILENAME);
                    }
                }
                else if (cmds.find("stopRecording") != cmds.end())
                {
                    if (cmds["stopRecording"] == "ALL")
                    {
                        for (auto id : recordersIds)
                            mm.request<bool>(id, STOP_REC);
                    }
                    else
                        mm.request<bool>(cmds["stopRecording"], STOP_REC);
                }
            }
            catch (const MediumException& me) 
            {
                std::cerr << "Error:  " << me.cause() << std::endl;
            }
            commandsReceiver.clearCommands();
        }
        catch (const MediumException& me) {}        
                                      
        eventsCatcher->catchNextEvent();
    }

    return 0;
    
}

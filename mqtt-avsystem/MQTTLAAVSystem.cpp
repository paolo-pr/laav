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
 *  See the description in README.md
 * 
 */

#include "LAAV.hpp"
#include "MQTTLAAVClient.hpp"
#include <list>

using namespace laav;

int main(int argc, char** argv)
{
    
    if (argc < 5) 
    {
        std::cout << "Usage: " << argv[0] << " /path/to/v4l/device brokerAddress brokerPort topicPrefix" << std::endl;
        return 1;
    }
    
    MQTTLAAVClient 
    mqttClient(argv[2], std::stoi(argv[3]), argv[4]);
   
    std::list<std::string> recordersIds;
    std::list<std::string> encodersIds;    
    std::map<std::string, bool> grabbersStatuses;
    
/******************************************************************    
 START OF CUSTOM PART (MEDIA DEFINITIONS)   
*******************************************************************/ 
    
    std::string AVDEV_ID         = "Cam1";
    std::string RECORDER_1_ID    = "Cam1-Recorder-HQ";
    std::string RECORDER_2_ID    = "Cam1-Recorder-LQ";
    int         MAX_REC_FILESIZE = 100;  // Set in MegaBytes
    
    typedef UnsignedConstant<640> WIDTH_1;
    typedef UnsignedConstant<480> HEIGHT_1; 
    typedef UnsignedConstant<320> WIDTH_2;
    typedef UnsignedConstant<240> HEIGHT_2;     

    LAAVLogLevel = 1;
    SharedEventsCatcher eventsCatcher = EventsManager::createSharedEventsCatcher();
    
    V4L2Grabber <YUYV422_PACKED, WIDTH_1, HEIGHT_1>
    vGrab(eventsCatcher, argv[1], DEFAULT_FRAMERATE, AVDEV_ID);

    VideoFrameHolder <YUYV422_PACKED, WIDTH_1, HEIGHT_1>
    vFh_common("vFh_common");

    VideoFrameHolder <YUV420_PLANAR, WIDTH_1, HEIGHT_1>
    vFh_1a("vFh_1a");      
    
    VideoFrameHolder <H264, WIDTH_1, HEIGHT_1>
    vFh_1b("vFh_1b");
    
    VideoFrameHolder <YUV420_PLANAR, WIDTH_2, HEIGHT_2>
    vFh_2a("vFh_2a"); 
    
    VideoFrameHolder <H264, WIDTH_2, HEIGHT_2>
    vFh_2b("vFh_2b");
    
    FFMPEGVideoConverter <YUYV422_PACKED, WIDTH_1, HEIGHT_1, YUV420_PLANAR, WIDTH_1, HEIGHT_1>
    vConv_1("vConv_1");

    FFMPEGVideoConverter <YUYV422_PACKED, WIDTH_1, HEIGHT_1, YUV420_PLANAR, WIDTH_2, HEIGHT_2>
    vConv_2("vConv_2");

    FFMPEGH264Encoder <YUV420_PLANAR, WIDTH_1, HEIGHT_1>
    vEnc_1(DEFAULT_BITRATE, 5, H264_ULTRAFAST, H264_DEFAULT_PROFILE, H264_DEFAULT_TUNE, "Cam1-Encoder-HQ");

    FFMPEGH264Encoder <YUV420_PLANAR, WIDTH_2, HEIGHT_2>
    vEnc_2(DEFAULT_BITRATE, 5, H264_ULTRAFAST, H264_DEFAULT_PROFILE, H264_DEFAULT_TUNE, "Cam1-Encoder-LQ");
    
    FFMPEGVideoMuxer <MPEGTS, H264, WIDTH_1, HEIGHT_1> 
    vMux_1(RECORDER_1_ID);

    FFMPEGVideoMuxer <MPEGTS, H264, WIDTH_2, HEIGHT_2> 
    vMux_2(RECORDER_2_ID);

    HTTPVideoStreamer <MPEGTS, H264, WIDTH_1, HEIGHT_1>
    vStream_1(eventsCatcher, "127.0.0.1", 8084, "vStream_1");

    HTTPVideoStreamer <MPEGTS, H264, WIDTH_2, HEIGHT_2>
    vStream_2(eventsCatcher, "127.0.0.1", 8085, "vStream_2");
    
    /*
     * IMPORTANT! Don't forget to register all the created media.
     * Do that in the way suggested by the following lines:
     */
    auto mm = MediaManagerFactory::createMediaManager
    (vGrab, vMux_1, vMux_2, vEnc_1, vEnc_2, vStream_1, vStream_2, vFh_1a, vFh_1b, vFh_2b);
    
    /* 
     * IMPORTANT! Don't forget to populate the following two lists and the following 
     * map with the muxers, the encoders and the grabbers. 
     * Do that in the way suggested by the following lines:
     */
    recordersIds.push_back(vMux_1.id());
    recordersIds.push_back(vMux_2.id());
    encodersIds.push_back(vEnc_1.id());
    encodersIds.push_back(vEnc_2.id()); 
    grabbersStatuses[vGrab.id()] = false;
    
/*******************************************************************    
 END OF CUSTOM PART    
********************************************************************/     
    
    MQTTConnectionStatus mqttLastConnectionStatus = MQTT_DISCONNECTED;
    
    // Backup recording statuses
    for (const auto& recorderId : recordersIds)
    {
        try
        {
            bool wasRecording = mm.request<bool>(recorderId, BACKED_UP_RECORDING_STATUS);
            if (wasRecording)
            {
                std::string encoderIdOfRecorder = mm.request<std::string>(recorderId, INPUT_VIDEO_FRAME_FACTORY_ID);
                // Try-catch is needed for audio recorders (their encoders don't have a generateKeyframe() function)
                try 
                { 
                    mm.request<Void>(encoderIdOfRecorder, GENERATE_KEYFRAME);
                }
                catch (const MediumException& me) {}                  
                mm.request<bool>(recorderId, START_REC, DEFAULT_REC_FILENAME);
                std::cout << "Backed up recording status = 1 for " << recorderId << std::endl;
            }         
        } 
        catch (const MediumException& me) { }
    }
    
    // Backup bitrate settings
    for (const auto& encoderId : encodersIds)
    {
        try
        {
            int prevBitrate = mm.request<int>(encoderId, BACKED_UP_BITRATE);
            mm.request<bool>(encoderId, SET_BITRATE, prevBitrate);
            std::cout << "Backed up bitrate = " << prevBitrate << " for " << encoderId << std::endl;
        } 
        catch (const MediumException& me) {}
    }    

    std::cout << "MQTT client connecting to broker..." << std::endl; 
    
    while (!LAAVStop)
    {
        
/******************************************************************    
 START OF CUSTOM PART (A/V PIPES)
*******************************************************************/ 
        
        vGrab >> vFh_common;
        
                 vFh_common >> vConv_1 >> vFh_1a >> vEnc_1 >> vFh_1b;
                                                              vFh_1b >> vStream_1;
                                                              vFh_1b >> vMux_1;
                                                    
                 vFh_common >> vConv_2 >> vFh_2a >> vEnc_2 >> vFh_2b;
                                                              vFh_2b >> vStream_2;
                                                              vFh_2b >> vMux_2;
                                                              
/******************************************************************    
 END OF CUSTOM PART (A/V PIPES)
*******************************************************************/ 
                                                    
        /*
         * Soon after a (re)connection, inform the MQTT broker
         * about the status of the device and its recorders
         */
        if (mqttClient.status() != mqttLastConnectionStatus)
        {
            if (mqttClient.status() == MQTT_CONNECTED)
            {
                std::cout << "MQTT client connected to broker" << std::endl;                               
                
                for (const auto& grabberStatus : grabbersStatuses)
                {
                    MQTTLAAVMsg grabberMsg;
                    grabberMsg.resourceId = grabberStatus.first;
                    grabberMsg.what = MQTTLAAV_AVDEV_GRABBING;
                    if (mm.request<bool>(grabberStatus.first, CAN_GRAB))
                        grabberMsg.valInt = 1;
                    else
                        grabberMsg.valInt = 0;
                    mqttClient.publishOUTMsg(grabberMsg);
                }
                
                for (const auto& recorderId : recordersIds)
                {
                    MQTTLAAVMsg recorderMsg;
                    recorderMsg.resourceId = recorderId;
                    recorderMsg.what = MQTTLAAV_REC;
                    mm.request<bool>(recorderId, IS_RECORDING) ?
                    recorderMsg.valInt = 1 : recorderMsg.valInt = 0;
                    mqttClient.publishOUTMsg(recorderMsg);
                }
            }
            else
                std::cout << "MQTT client not connected to broker" << std::endl;
            
            mqttLastConnectionStatus = mqttClient.status();
        }        
                 
        // Inform the MQTT broker that the device has changed status                 
        for (const auto& grabberStatus : grabbersStatuses)
        {
            bool currGrabStatus = mm.request<bool>(grabberStatus.first, CAN_GRAB);            
            bool prevGrabStatus = grabberStatus.second;
            if (currGrabStatus != prevGrabStatus)
            {
                MQTTLAAVMsg grabberMsg;
                grabberMsg.resourceId = grabberStatus.first;
                grabberMsg.what = MQTTLAAV_AVDEV_GRABBING;
                if (currGrabStatus)
                {
                    std::cout << "AV device working" << std::endl;                    
                    grabberMsg.valInt = 1;
                }
                else
                {
                    std::cout << "AV device not working" << std::endl;
                    grabberMsg.valInt = 0;
                }
                mqttClient.publishOUTMsg(grabberMsg); 
                grabbersStatuses[grabberStatus.first] = currGrabStatus;
            }
        }                 

        /*
         * Execute incoming cmds (start/stop recording, set bitrate) on 
         * IN topic, backup the status and inform the broker that the 
         * cmd has been executed
         */        
        if (mqttClient.hasBufferedPublishedINMsg())
        {
            try
            {
                MQTTLAAVMsg inMsg      = mqttClient.bufferedPublishedINMsg();
                std::string resourceId = inMsg.resourceId;
                std::string what       = inMsg.what; 
                int val                = inMsg.valInt;
                std::cout << "Got MQTT command: " << inMsg.rawStr << std::endl;
                bool ok = true;
                
                if (what == MQTTLAAV_REC && val == 1)
                {
                    std::string encoderIdOfRecorder = mm.request<std::string>(resourceId, INPUT_VIDEO_FRAME_FACTORY_ID);
                    // Try-catch is needed at least for audio recorders (their encoders don't have a generateKeyframe() function)
                    try 
                    { 
                        mm.request<Void>(encoderIdOfRecorder, GENERATE_KEYFRAME);
                    }
                    catch (const MediumException& me) {}                    
                    mm.request<bool>(resourceId, START_REC, DEFAULT_REC_FILENAME);
                    mm.request<Void>(resourceId, BACKUP_RECORDING_STATUS);
                }
                else if (what == MQTTLAAV_REC && val == 0)
                {
                    mm.request<bool>(resourceId, STOP_REC); 
                    mm.request<Void>(resourceId, BACKUP_RECORDING_STATUS);                    
                }
                else if (what == MQTTLAAV_BITRATE)
                {
                    mm.request<bool>(resourceId, SET_BITRATE, val * 1000);  
                    mm.request<Void>(resourceId, BACKUP_BITRATE);                    
                }
                else                    
                    ok = false;
                
                // Send back the status of the recorder to the broker
                if (ok)
                {
                    mqttClient.publishOUTMsg(inMsg);
                    std::cout << "Command executed" << std::endl;
                }
                else
                    std::cout << "Bad Command" << std::endl;
            } 
            catch (const MediumException& me)
            {  
                std::cout << me.cause() << std::endl;
            }
            catch (const MQTTLAAVMsgException& mlme) 
            { 
                std::cout << mlme.cause() << std::endl;
            }                

            mqttClient.clearPublishedINMsgBuffer();
        }        
                   
        /* 
         * Stop encoding if the encoder is a "void factory", which happens when:
         * 1) there are not clients viewing the (HTTP) streams linked to the encoder
         * AND
         * 2) the recorders linked to the encoder are not recording
         */ 
        for (const auto& encoderId : encodersIds)
        {
            if (mm.isVoidFactory(encoderId))
                mm.request<Void>(encoderId, PAUSE, true);
            else
                mm.request<Void>(encoderId, PAUSE, false);
        }
        
        // Fragment recorded files if they exceed MAX_REC_FILESIZE
        for (const auto& recorderId : recordersIds)
        {
            if (mm.request<int>(recorderId, RECORDING_FILE_SIZE) > MAX_REC_FILESIZE * 1000000)
            {
                mm.request<bool>(recorderId, STOP_REC);
                std::string idOfEncoderOfRecorder = mm.request<std::string>(recorderId, INPUT_VIDEO_FRAME_FACTORY_ID);
                //Try-catch is needed for audio recorders (their encoders don't have a generateKeyframe() function)
                try 
                { 
                    mm.request<Void>(idOfEncoderOfRecorder, GENERATE_KEYFRAME);
                }
                catch (const MediumException& me) {}
                mm.request<bool>(recorderId, START_REC, DEFAULT_REC_FILENAME);
            }
        }   

        mqttClient.refresh();
        eventsCatcher->catchNextEvent();
    }
  
    return 0;

}

/* 
 * Created (25/02/2018) by Paolo-Pr.
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
 *  ????????????
 * 
 */

#ifndef MEDIAMANAGER_HPP_INCLUDED
#define MEDIAMANAGER_HPP_INCLUDED

#include <map>
#include <tuple>
#include <type_traits>
#include "LAAV.hpp"

#define BACKUP_FILENAME ".laavbackup"
#define BACKUP_SEPARATOR "---"

namespace laav
{
    
enum LAAVMediumReq
{
    START_REC, 
    STOP_REC, 
    START_STREAMING, // for UDP, TODO
    STOP_STREAMING, // for UDP, TODO
    IS_STREAMING, // for UDP, TODO
    GENERATE_KEYFRAME,
    PAUSE,
    SET_BITRATE,
    BACKUP_BITRATE,
    BACKED_UP_BITRATE,
    IS_RECORDING,
    CAN_GRAB,
    BACKUP_RECORDING_STATUS,
    BACKED_UP_RECORDING_STATUS,
    RECORDING_FILE_SIZE,
    CONNECTED_CLIENTS,
    INPUT_VIDEO_FRAME_FACTORY_ID,
    INPUT_AUDIO_FRAME_FACTORY_ID
};

template <typename ...MediaTypes>
class MediaManager 
{

friend class MediaManagerFactory;    
    
public:
    
    /*!
    *  \exception MediumException
    */   
    template<typename RetType>
    RetType request(const std::string& id,
              enum LAAVMediumReq req, const int& val)
    {
        return requestImpl<RetType, int, 0> (id, req, val);
    }
    
    /*!
    *  \exception MediumException
    */   
    template<typename RetType>
    RetType request(const std::string& id,
              enum LAAVMediumReq req, const std::string& val)
    {
        return requestImpl<RetType, std::string, 0> (id, req, val);
    }    
    
    /*!
    *  \exception MediumException
    */   
    template<typename RetType>
    RetType request(const std::string& id,
              enum LAAVMediumReq req)
    {
        Void val;
        return requestImpl<RetType, Void, 0> (id, req, val);
    }
    
    bool isVoidFactory(const std::string& factoryId) const
    {
        // factory is set as void, before checking
        mFactoriesStatuses[factoryId] = false; 
        mapFactoryDepth<0>(factoryId);
        updateStatusForFactoryEndPoints<0>(factoryId);
        return !mFactoriesStatuses[factoryId];
    }
    
private:
            
    template <size_t I>   
    void mapFactoryDepth(const std::string& factoryId) const
    {
        const auto& medium = (std::get<I> (mMedia)); 
        
        if (mFactoriesDepths.count(factoryId) == 1)
        {
            //check it belongs to an audio or video frame factory            
            if (medium.mInputVideoFrameFactoryId == factoryId &&
                medium.mDistanceFromInputVideoFrameFactory > mFactoriesDepths[factoryId])
                mFactoriesDepths[factoryId] = medium.mDistanceFromInputVideoFrameFactory;
            else 
            if (medium.mInputAudioFrameFactoryId == factoryId &&
                medium.mDistanceFromInputAudioFrameFactory > mFactoriesDepths[factoryId])
                mFactoriesDepths[factoryId] = medium.mDistanceFromInputAudioFrameFactory;
        }
        else
        {
            mFactoriesDepths[factoryId] = 0;
        }
            
        if (I + 1 < sizeof... (MediaTypes))
            mapFactoryDepth<(I + 1 < sizeof... (MediaTypes) ? I + 1 : I)> 
            (factoryId);          
    }
    
    template <typename Medium_> 
    bool isIdle(const Medium_& medium) const
    {
        if (medium.inputStatus() == PAUSED)
            return true;
        else
            return false;
    }    
    
    template <typename Container, typename VideoCodecOrFormat,
              typename width, typename height>
    bool isIdle(const HTTPVideoStreamer<Container, VideoCodecOrFormat, 
                                        width, height>& medium) const
    {
        if (medium.inputStatus() == PAUSED || medium.numOfConnectedClients() == 0)
            return true;
        else
            return false;
    } 
    
    template <typename Container, typename AudioCodecOrFormat,
              typename audioSampleRate, typename audioChannels>
    bool isIdle(const HTTPAudioStreamer<Container, AudioCodecOrFormat, 
                                        audioSampleRate, audioChannels>& medium) const
    {
        if (medium.inputStatus() == PAUSED || medium.numOfConnectedClients() == 0)
            return true;
        else
            return false;
    }
    
    template <typename Container, 
              typename VideoCodecOrFormat, 
              typename width, typename height,
              typename AudioCodecOrFormat, 
              typename audioSampleRate, typename audioChannels>    
    bool isIdle(const HTTPAudioVideoStreamer<Container, 
                                             VideoCodecOrFormat, 
                                             width, height,
                                             AudioCodecOrFormat,
                                             audioSampleRate, audioChannels>& medium) const
    {
        if (medium.inputStatus() == PAUSED || medium.numOfConnectedClients() == 0)
            return true;
        else
            return false;
    }     
    
    template <typename Container, typename AudioCodecOrFormat,
              typename audioSampleRate, typename audioChannels>    
    bool isIdle(const FFMPEGAudioMuxer<Container, AudioCodecOrFormat, 
                                       audioSampleRate, audioChannels>& medium) const
    {
        if (medium.inputStatus() == PAUSED || !medium.isMuxing())
            return true;
        else
            return false;
    }    
    
    template <typename Container, typename VideoCodecOrFormat, 
              typename width, typename height>
    bool isIdle(const FFMPEGVideoMuxer<Container, VideoCodecOrFormat, 
                                       width, height>& medium) const
    {
        if (medium.inputStatus() == PAUSED || !medium.isMuxing())
            return true;
        else
            return false;
    }               
    
    template <typename Container,
              typename VideoCodecOrFormat,
              typename width, typename height,
              typename AudioCodecOrFormat, 
              typename audioSampleRate, typename audioChannels>  
    bool isIdle(const FFMPEGAudioVideoMuxer<Container, 
                                            VideoCodecOrFormat, 
                                            width, height,
                                            AudioCodecOrFormat,
                                            audioSampleRate, audioChannels>& medium) const
    {
        if (medium.inputStatus() == PAUSED || !medium.isMuxing())
            return true;
        else
            return false;
    }               
      
    template <size_t I>   
    void updateStatusForFactoryEndPoints(const std::string& factoryId) const
    {
        const auto& medium = (std::get<I> (mMedia)); 

        if (mFactoriesDepths[factoryId] == 0 && !isIdle(medium) && medium.id() == factoryId)
        {
            mFactoriesStatuses[factoryId] = true;
        }           
        
        // check if medium is at endpoint and if is it idle.
        if (medium.mDistanceFromInputVideoFrameFactory == mFactoriesDepths[factoryId]
            && !isIdle(medium) && medium.mInputVideoFrameFactoryId == factoryId)
        {
            mFactoriesStatuses[factoryId] = true;
        }
             
        if (medium.mDistanceFromInputAudioFrameFactory == mFactoriesDepths[factoryId]
            && !isIdle(medium) && medium.mInputAudioFrameFactoryId == factoryId)
        {
            mFactoriesStatuses[factoryId] = true;
        }

        if (I + 1 < sizeof... (MediaTypes))
            updateStatusForFactoryEndPoints<(I + 1 < sizeof... (MediaTypes) ? I + 1 : I)> 
            (factoryId);         
    }    
    
    void backup(const std::string& id, const std::string& field, int val) const
    {
        std::fstream backupFile;
        std::string newFileConent;    
        backupFile.exceptions(std::fstream::badbit);    
        backupFile.open(BACKUP_FILENAME, std::fstream::in | std::fstream::out | std::fstream::binary);
        
        //Check if there's already a line with that id, then replace it
        std::string line;
        bool lineFound = false;
        std::string idAndField = id + BACKUP_SEPARATOR + field;
        std::string completeLine = idAndField + BACKUP_SEPARATOR + std::to_string(val);
        while(backupFile >> line)
        {
            if(line.find(idAndField) != std::string::npos)
            {
                line = completeLine;
                lineFound = true;
            }
            line += "\n";
            newFileConent += line;
        }
        
        if (!lineFound)
            newFileConent += completeLine;
        
        backupFile.close();
        backupFile.open(BACKUP_FILENAME, std::fstream::out | 
                        std::fstream::binary | std::ios::trunc);

        backupFile << newFileConent;
        backupFile.close();        
    }
    
    int backedUpVal(const std::string& id, const std::string& field) const
    {
        std::ifstream backupFile;   
        int ret = 0;
        backupFile.exceptions(std::fstream::badbit);    
        backupFile.open(BACKUP_FILENAME, std::fstream::binary);
        std::string line;
        std::string tokenToFind = id + BACKUP_SEPARATOR + field;
        bool found = false;
        while (backupFile >> line)
        {
            if(line.find(tokenToFind) != std::string::npos)
            {
                ret = std::stoi(split(line, BACKUP_SEPARATOR)[2]);
                found = true;
            }
        }    
        backupFile.close();
        if (!found)
            throw MediumException(BACKED_UP_VALUE_NOT_FOUND);
        return ret;
    }
    
    MediaManager(std::tuple<MediaTypes...>&& media) : mMedia(media)
    {}    
    
#define V4L2GRABBER_TYPENAMES typename CodecOrFormat, typename width, typename height
#define V4L2GrabberGeneric V4L2Grabber<CodecOrFormat, width, height>
#define ALSAGRABBER_TYPENAMES typename CodecOrFormat, typename audioSampleRate, typename audioChannels
#define AlsaGrabberGeneric AlsaGrabber<CodecOrFormat, audioSampleRate, audioChannels>
#define FFMPEGH264ENCODER_TYPENAMES typename RawVideoFrameFormat, typename width, typename height
#define FFMPEGH264EncoderGeneric FFMPEGH264Encoder<RawVideoFrameFormat, width, height>
#define AUDIOFRAMEHOLDER_TYPENAMES typename CodecOrFormat, typename audioSampleRate, typename audioChannels
#define VIDEOFRAMEHOLDER_TYPENAMES typename CodecOrFormat, typename width, typename height
#define FFMPEGMuxerGeneric FFMPEGMuxerCommonImpl<Container>
#define HTTPStreamerGeneric HTTPStreamer<Container>
#define AudioFrameHolderGeneric AudioFrameHolder<CodecOrFormat, audioSampleRate, audioChannels>
#define VideoFrameHolderGeneric VideoFrameHolder<CodecOrFormat, width, height>

    //TODO: all these params and funcs (old legacy code) 
    // can be packed in a shorter and better way

    //TODO: add pause at least for a mjpeg decoder and a converter
    
    /////////////////////
    // bool --> void
    /////////////////////

    // V4L2Grabber
    template<V4L2GRABBER_TYPENAMES>
    Void requestSpec(Void* fooParam, V4L2GrabberGeneric& grab, enum LAAVMediumReq req, const bool& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // AlsaGrabber
    template<ALSAGRABBER_TYPENAMES>
    Void requestSpec(Void* fooParam, AlsaGrabberGeneric& grab, enum LAAVMediumReq req, const bool& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // Muxer
    template<typename Container>
    Void requestSpec(Void* fooParam, FFMPEGMuxerGeneric& mux, enum LAAVMediumReq req, const bool& val)
    {      
        if (req == PAUSE)
            mux.pause(val);
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
        return Void();
    }

    // H264Encoder
    template<FFMPEGH264ENCODER_TYPENAMES>
    Void requestSpec(Void* fooParam, FFMPEGH264EncoderGeneric& enc, enum LAAVMediumReq req, const bool& val)
    {
        if (req == PAUSE)
            enc.pause(val);
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST);  
        return Void();
    }    
    
    // HTTPStreamer
    template<typename Container>
    Void requestSpec(Void* fooParam, HTTPStreamerGeneric& str, enum LAAVMediumReq req, const bool& val)
    {
        if (req == PAUSE)
            str.pause(val);
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST);
        return Void();
    }
    
    // VideoFrameHolder
    template<VIDEOFRAMEHOLDER_TYPENAMES>
    Void requestSpec(Void* fooParam, VideoFrameHolderGeneric& fh, enum LAAVMediumReq req, const bool& val)
    {
        if (req == PAUSE)
            fh.pause(val);
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
        return Void();
    }    
    
    // AudioFrameHolder
    template<AUDIOFRAMEHOLDER_TYPENAMES>
    Void requestSpec(Void* fooParam, AudioFrameHolderGeneric& fh, enum LAAVMediumReq req, const bool& val)
    {
        if (req == PAUSE)
            fh.pause(val);
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST);
        return Void();
    }    

    /////////////////////
    // string --> bool
    /////////////////////
       
    // V4L2Grabber
    template<V4L2GRABBER_TYPENAMES>
    bool requestSpec(bool* fooParam, V4L2GrabberGeneric& grab, enum LAAVMediumReq req, const std::string& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // AlsaGrabber
    template<ALSAGRABBER_TYPENAMES>
    bool requestSpec(bool* fooParam, AlsaGrabberGeneric& grab, enum LAAVMediumReq req, const std::string& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }
       
    // Muxer
    template<typename Container>
    bool requestSpec(bool* fooParam, FFMPEGMuxerGeneric& mux, enum LAAVMediumReq req, const std::string& val)
    {      
        if (req == START_REC)
            return mux.startRecording(val);
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }

    // H264Encoder
    template<FFMPEGH264ENCODER_TYPENAMES>
    bool requestSpec(bool* fooParam, FFMPEGH264EncoderGeneric& enc, enum LAAVMediumReq req, const std::string& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // HTTPStreamer
    template<typename Container>
    bool requestSpec(bool* fooParam, HTTPStreamerGeneric& str, enum LAAVMediumReq req, const std::string& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }
    
    
    // VideoFrameHolder
    template<VIDEOFRAMEHOLDER_TYPENAMES>
    bool requestSpec(bool* fooParam, VideoFrameHolderGeneric& fh, enum LAAVMediumReq req, const std::string& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // AudioFrameHolder
    template<AUDIOFRAMEHOLDER_TYPENAMES>
    bool requestSpec(bool* fooParam, AudioFrameHolderGeneric& fh, enum LAAVMediumReq req, const std::string& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }     
    
    /////////////////////
    // void --> string
    /////////////////////
       
    // V4L2Grabber
    template<V4L2GRABBER_TYPENAMES>
    std::string requestSpec(std::string* fooParam, V4L2GrabberGeneric& grab, enum LAAVMediumReq req, const Void& val)
    {
        if (req == INPUT_VIDEO_FRAME_FACTORY_ID)
            return grab.inputVideoFrameFactoryId();
        else if (req == INPUT_AUDIO_FRAME_FACTORY_ID)
            return grab.inputAudioFrameFactoryId();
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // AlsaGrabber
    template<ALSAGRABBER_TYPENAMES>
    std::string requestSpec(std::string* fooParam, AlsaGrabberGeneric& grab, enum LAAVMediumReq req, const Void& val)
    {
        if (req == INPUT_VIDEO_FRAME_FACTORY_ID)
            return grab.inputVideoFrameFactoryId();
        else if (req == INPUT_AUDIO_FRAME_FACTORY_ID)
            return grab.inputAudioFrameFactoryId();
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST);  
    }
       
    // Muxer
    template<typename Container>
    std::string requestSpec(std::string* fooParam, FFMPEGMuxerGeneric& mux, enum LAAVMediumReq req, const Void& val)
    {      
        if (req == INPUT_VIDEO_FRAME_FACTORY_ID)
            return mux.inputVideoFrameFactoryId();
        else if (req == INPUT_AUDIO_FRAME_FACTORY_ID)
            return mux.inputAudioFrameFactoryId();
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST);  
    }

    // H264Encoder
    template<FFMPEGH264ENCODER_TYPENAMES>
    std::string requestSpec(std::string* fooParam, FFMPEGH264EncoderGeneric& enc, enum LAAVMediumReq req, const Void& val)
    {
        if (req == INPUT_VIDEO_FRAME_FACTORY_ID)
            return enc.inputVideoFrameFactoryId();
        else if (req == INPUT_AUDIO_FRAME_FACTORY_ID)
            return enc.inputAudioFrameFactoryId();
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST);  
    }    
    
    // HTTPStreamer
    template<typename Container>
    std::string requestSpec(std::string* fooParam, HTTPStreamerGeneric& str, enum LAAVMediumReq req, const Void& val)
    {
        if (req == INPUT_VIDEO_FRAME_FACTORY_ID)
            return str.inputVideoFrameFactoryId();
        else if (req == INPUT_AUDIO_FRAME_FACTORY_ID)
            return str.inputAudioFrameFactoryId();
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST);  
    }
    
    
    // VideoFrameHolder
    template<VIDEOFRAMEHOLDER_TYPENAMES>
    std::string requestSpec(std::string* fooParam, VideoFrameHolderGeneric& fh, enum LAAVMediumReq req, const Void& val)
    {
        if (req == INPUT_VIDEO_FRAME_FACTORY_ID)
            return fh.inputVideoFrameFactoryId();
        else if (req == INPUT_AUDIO_FRAME_FACTORY_ID)
            return fh.inputAudioFrameFactoryId();
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST);  
    }    
    
    // AudioFrameHolder
    template<AUDIOFRAMEHOLDER_TYPENAMES>
    std::string requestSpec(std::string* fooParam, AudioFrameHolderGeneric& fh, enum LAAVMediumReq req, const Void& val)
    {
        if (req == INPUT_VIDEO_FRAME_FACTORY_ID)
            return fh.inputVideoFrameFactoryId();
        else if (req == INPUT_AUDIO_FRAME_FACTORY_ID)
            return fh.inputAudioFrameFactoryId();
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST);  
    } 
    
    /////////////////////
    // int --> bool
    /////////////////////   
       
    // V4L2Grabber
    template<V4L2GRABBER_TYPENAMES>
    bool requestSpec(bool* fooParam, V4L2GrabberGeneric& grab, enum LAAVMediumReq req, const int& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // AlsaGrabber
    template<ALSAGRABBER_TYPENAMES>
    bool requestSpec(bool* fooParam, AlsaGrabberGeneric& grab, enum LAAVMediumReq req, const int& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }
    
    // Muxer   
    template<typename Container>
    bool requestSpec(bool* fooParam, FFMPEGMuxerGeneric& mux, enum LAAVMediumReq req, const int& val)
    {      
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // H264Encoder
    template <FFMPEGH264ENCODER_TYPENAMES>
    bool requestSpec(bool* fooParam, FFMPEGH264EncoderGeneric& enc, enum LAAVMediumReq req, const int& val)
    {
        if (req == SET_BITRATE)
        {
            enc.setBitrate(val);
            return true;
        }
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }     
    
    // HTTPStreamer
    template<typename Container>
    bool requestSpec(bool* fooParam, HTTPStreamerGeneric& str, enum LAAVMediumReq req, const int& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    } 
    
    // VideoFrameHolder
    template<VIDEOFRAMEHOLDER_TYPENAMES>
    bool requestSpec(bool* fooParam, VideoFrameHolderGeneric& fh, enum LAAVMediumReq req, const int& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // AudioFrameHolder
    template<AUDIOFRAMEHOLDER_TYPENAMES>
    bool requestSpec(bool* fooParam, AudioFrameHolderGeneric& fh, enum LAAVMediumReq req, const int& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    } 
    
    /////////////////////
    // void --> bool
    /////////////////////     
    
    // V4L2Grabber
    template<V4L2GRABBER_TYPENAMES>
    bool requestSpec(bool* fooParam, V4L2GrabberGeneric& grab, enum LAAVMediumReq req, const Void& val)
    {
        if (req == CAN_GRAB)
            return grab.deviceStatus() == DEV_CAN_GRAB;
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // AlsaGrabber
    template<ALSAGRABBER_TYPENAMES>
    bool requestSpec(bool* fooParam, AlsaGrabberGeneric& grab, enum LAAVMediumReq req, const Void& val)
    {
        if (req == CAN_GRAB)
            return grab.deviceStatus() == DEV_CAN_GRAB;
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }
    
    // Muxer
    template<typename Container>
    bool requestSpec(bool* fooParam, FFMPEGMuxerGeneric& mux,enum LAAVMediumReq req, const Void& val)
    {     
        if (req == STOP_REC)
            return mux.stopRecording();
        else if (req == IS_RECORDING)
            return mux.isRecording();        
        else if (req == BACKED_UP_RECORDING_STATUS)
        {
            bool backedUpRecStatus = false;
            if (backedUpVal(mux.id(), "RECORDING_STATUS") == 1)
            {
                backedUpRecStatus = true;
            }
            return backedUpRecStatus;        
        }
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // H264Encoder
    template <FFMPEGH264ENCODER_TYPENAMES>
    bool requestSpec(bool* fooParam, FFMPEGH264EncoderGeneric& enc, enum LAAVMediumReq req, const Void& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // HTTPStreamer
    template <typename Container>
    bool requestSpec(bool* fooParam, HTTPStreamerGeneric& str, enum LAAVMediumReq req, const Void& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }     
    
    // VideoFrameHolder
    template<VIDEOFRAMEHOLDER_TYPENAMES>
    bool requestSpec(bool* fooParam, VideoFrameHolderGeneric& fh, enum LAAVMediumReq req, const Void& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // AudioFrameHolder
    template<AUDIOFRAMEHOLDER_TYPENAMES>
    bool requestSpec(bool* fooParam, AudioFrameHolderGeneric& fh, enum LAAVMediumReq req, const Void& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }     
    
    /////////////////////
    // void --> int
    /////////////////////     
    
    // V4L2Grabber
    template<V4L2GRABBER_TYPENAMES>
    int requestSpec(int* fooParam, V4L2GrabberGeneric& grab, enum LAAVMediumReq req, const Void& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // AlsaGrabber
    template<ALSAGRABBER_TYPENAMES>
    int requestSpec(int* fooParam, AlsaGrabberGeneric& grab, enum LAAVMediumReq req, const Void& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // Muxer
    template<typename Container>
    int requestSpec(int* fooParam, FFMPEGMuxerGeneric& mux,enum LAAVMediumReq req, const Void& val)
    {      
        if (req == RECORDING_FILE_SIZE)
            return mux.recordingFileSize();
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }   
    
    // H264Encoder
    template<FFMPEGH264ENCODER_TYPENAMES>
    int requestSpec(int* fooParam, FFMPEGH264EncoderGeneric& enc, enum LAAVMediumReq req, const Void& val)
    {
        if (req == BACKED_UP_BITRATE)
            return backedUpVal(enc.id(), "BITRATE");
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST);
    }    
    
    // HTTPStreamer
    template<typename Container>
    int requestSpec(int* fooParam, HTTPStreamerGeneric& str, enum LAAVMediumReq req, const Void& val)
    {      
        if (req == CONNECTED_CLIENTS)
            return str.numOfConnectedClients();
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }     
    
    // VideoFrameHolder
    template<VIDEOFRAMEHOLDER_TYPENAMES>
    int requestSpec(int* fooParam, VideoFrameHolderGeneric& fh, enum LAAVMediumReq req, const Void& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // AudioFrameHolder
    template<AUDIOFRAMEHOLDER_TYPENAMES>
    int requestSpec(int* fooParam, AudioFrameHolderGeneric& fh, enum LAAVMediumReq req, const Void& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }
    
    /////////////////////
    // void --> void
    /////////////////////     
    
    // V4L2Grabber
    template<V4L2GRABBER_TYPENAMES>
    Void requestSpec(Void* fooParam, V4L2GrabberGeneric& grab, enum LAAVMediumReq req, const Void& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // AlsaGrabber
    template<ALSAGRABBER_TYPENAMES>
    Void requestSpec(Void* fooParam, AlsaGrabberGeneric& grab, enum LAAVMediumReq req, const Void& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }
    
    // Muxer
    template<typename Container>
    Void requestSpec(Void* fooParam, FFMPEGMuxerGeneric& mux,enum LAAVMediumReq req, const Void& val)
    {      
        if (req == BACKUP_RECORDING_STATUS)
        {
            int valToBackup = 0;
            if (mux.isRecording())
                valToBackup = 1;
            backup(mux.id(), "RECORDING_STATUS", valToBackup);
            return Void();
        }
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST);   
    }   
    
    // H264Encoder
    template<FFMPEGH264ENCODER_TYPENAMES>
    Void requestSpec(Void* fooParam, FFMPEGH264EncoderGeneric& enc, enum LAAVMediumReq req, const Void& val)
    {
        if (req == BACKUP_BITRATE)
        {
            backup(enc.id(), "BITRATE", enc.bitrate());
            return Void();
        }
        else if (req == GENERATE_KEYFRAME)
        {
            enc.generateKeyFrame();
            return Void();
        }
        else
            throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // HTTPStreamer
    template<typename Container>
    Void requestSpec(Void* fooParam, HTTPStreamerGeneric& str, enum LAAVMediumReq req, const Void& val)
    {      
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }     
    
    // VideoFrameHolder
    template<VIDEOFRAMEHOLDER_TYPENAMES>
    Void requestSpec(Void* fooParam, VideoFrameHolderGeneric& fh, enum LAAVMediumReq req, const Void& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    // AudioFrameHolder
    template<AUDIOFRAMEHOLDER_TYPENAMES>
    Void requestSpec(Void* fooParam, AudioFrameHolderGeneric& fh, enum LAAVMediumReq req, const Void& val)
    {
        throw MediumException(MEDIUM_UNSUPPORTED_REQUEST); 
    }    
    
    //------------------------------------------------------
    
    template<typename RetType, typename ValType, size_t I>
    RetType requestImpl (const std::string& id, 
                         enum LAAVMediumReq req, const ValType& val)
    {
        auto& medium = (std::get<I> (mMedia)); 
        // This unused var resolves ambiguity of the compiler when 
        // unpacking templates, for returning different types
        // (ugly hack)
        RetType* fooParam = NULL;    
        RetType ret;
        if (medium.id() == id)
            ret = requestSpec(fooParam, medium, req, val);
        else if (I + 1 < sizeof... (MediaTypes))
            ret = requestImpl<RetType, ValType, (I + 1 < sizeof... (MediaTypes) ? I + 1 : I)> 
            (id, req, val);      
        else 
            throw MediumException(MEDIUM_NOT_FOUND); 
        
        return ret;
    }
    
    std::tuple<MediaTypes...> mMedia;
    mutable std::map<std::string, unsigned> mFactoriesDepths;
    // FALSE --> void factory, TRUE --> non-void factory
    mutable std::map<std::string, bool> mFactoriesStatuses;
    std::fstream mBackupFile;    
};

// Use this instead of a public MediaManager constructor in order
// to declare an "auto" instance (without listing the template params)
// in C++11/14s (see examples)
class MediaManagerFactory
{

public:    

    template <typename... MediaTypes>
    static MediaManager<MediaTypes...> createMediaManager(MediaTypes&& ... media)
    {
        return MediaManager <MediaTypes...>(std::tie(media...));
    }    
    
};

}
#endif

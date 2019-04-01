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
 * 
 */

#ifndef MEDIUM_HPP_INCLUDED
#define MEDIUM_HPP_INCLUDED

#include <iostream>
#include "Common.hpp"

namespace laav
{

enum IOStatus 
{
    READY, 
    NOT_READY, 
    PAUSED  
};

typedef IOStatus InputStatus;
typedef IOStatus OutputStatus;

enum MediumExceptionCause
{
    MEDIUM_PAUSED,
    MEDIUM_BUFFERING,
    MEDIUM_UNSUPPORTED_REQUEST,
    MEDIUM_NOT_FOUND,
    INPUT_NOT_READY,
    INPUT_EMPTY,
    OUTPUT_NOT_READY,
    PIPE_NOT_READY,
    BACKED_UP_VALUE_NOT_FOUND
};    
  

class MediumException 
{
public:
    MediumException(enum MediumExceptionCause cause)
    : mCause(cause)
    {
    }
    
    std::string cause() const
    {
        if (mCause == MEDIUM_PAUSED)
            return "MEDIUM_PAUSED";
        else if (mCause == MEDIUM_BUFFERING)
            return "MEDIUM_BUFFERING";
        else if (mCause == MEDIUM_UNSUPPORTED_REQUEST)
            return "MEDIUM_UNSUPPORTED_REQUEST";
        else if (mCause == MEDIUM_NOT_FOUND)
            return "MEDIUM_NOT_FOUND";
        else if (mCause == INPUT_NOT_READY)
            return "INPUT_NOT_READY";
        else if (mCause == INPUT_EMPTY)
            return "INPUT_EMPTY";
        else if (mCause == OUTPUT_NOT_READY)
            return "PIPE_NOT_READY";
        else if (mCause == PIPE_NOT_READY)
            return "OUTPUT_NOT_READY";        
        else if (mCause == BACKED_UP_VALUE_NOT_FOUND)
            return "BACKED_UP_VALUE_NOT_FOUND";
        else return "";
    }

private:    
    
    enum MediumExceptionCause mCause;
    
};    
    
class Medium    
{

template <typename ...MediaTypes>    
friend class MediaManager;    

public:
    
    const std::string& id() const
    {
        return mId;
    }
    
    const std::string& inputVideoFrameFactoryId() const
    {
        return mInputVideoFrameFactoryId;
    }    
    
    const std::string& inputAudioFrameFactoryId() const
    {
        return mInputAudioFrameFactoryId;
    }    
    
    void pause(bool pauseVal)
    {
        if (mPaused == pauseVal)
            return; 
        
        mPaused = pauseVal;
        if (pauseVal)
        {
            mBeforePauseInputStatus = mInputStatus;
            mBeforePauseOutputStatus = mOutputStatus;
            mInputStatus  = PAUSED;
            mOutputStatus = PAUSED;
        }
        else
        {
            mInputStatus  = mBeforePauseInputStatus;
            mOutputStatus = mBeforePauseOutputStatus;
        }
        
    }
    
    const InputStatus& inputStatus() const
    {
        return mInputStatus;
    }
    
    std::string inputStatusString() const
    {
        return IOStatusString(mInputStatus);
    }
    
    std::string outputStatusString() const
    {
        return IOStatusString(mOutputStatus);        
    }    
    
    const OutputStatus& outputStatus() const
    {
        return mOutputStatus;
    }    

protected: 
    
    Medium(const std::string& id = "") : 
        mDistanceFromInputVideoFrameFactory(0),
        mDistanceFromInputAudioFrameFactory(0),
        mPaused(false),    
        mId(id),
        mInputStatus(NOT_READY),
        mOutputStatus(NOT_READY),
        mStatusInPipe(READY),
        mBeforePauseInputStatus(NOT_READY),
        mBeforePauseOutputStatus(NOT_READY)
    {
    }

    unsigned mDistanceFromInputVideoFrameFactory;  
    unsigned mDistanceFromInputAudioFrameFactory;      

    std::string mInputVideoFrameFactoryId;    
    std::string mInputAudioFrameFactoryId;     

    bool mPaused;    
    std::string mId;   
    InputStatus mInputStatus;
    OutputStatus mOutputStatus;
    IOStatus mStatusInPipe;

private:    
    
    friend void setInputVideoFrameFactoryId(Medium& medium, const std::string& id)
    {
        medium.mInputVideoFrameFactoryId = id;
    }
    
    friend void setDistanceFromInputVideoFrameFactory(Medium& medium, int distance)
    {
        medium.mDistanceFromInputVideoFrameFactory = distance;
    }    
    
    friend void setInputAudioFrameFactoryId(Medium& medium, const std::string& id)
    {
        medium.mInputAudioFrameFactoryId = id;
    }
    
    friend void setDistanceFromInputAudioFrameFactory(Medium& medium, int distance)
    {
        medium.mDistanceFromInputAudioFrameFactory = distance;
    }      
    
    friend void setStatusInPipe(Medium& medium, IOStatus status)
    {
        medium.mStatusInPipe = status;
    }      
    
    
    std::string IOStatusString(IOStatus iost) const
    {
        if (iost == READY)
            return "READY";
        else if (iost == NOT_READY)
            return "NOT_READY";
        else if (iost == PAUSED)
            return "PAUSED";
        else 
            return "";
    }    
    
    InputStatus mBeforePauseInputStatus;
    OutputStatus mBeforePauseOutputStatus;     
    
};    
    
}

#endif

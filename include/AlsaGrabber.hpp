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
 */

#ifndef ALSAGRABBER_HPP_INCLUDED
#define ALSAGRABBER_HPP_INCLUDED

#ifdef LINUX

extern "C"
{
#include <alsa/asoundlib.h>
}

#include <iostream>
#include <fstream>
#include "Medium.hpp"
#include "AllAudioCodecsAndFormats.hpp"
#include "EventsManager.hpp"
#include "AudioFrameHolder.hpp"

namespace laav
{

struct AlsaUtils
{
    template <typename T>
    static snd_pcm_format_t translateSoundFormat();
};

template <>
snd_pcm_format_t AlsaUtils::translateSoundFormat<S16_LE>()
{
    return SND_PCM_FORMAT_S16_LE;
}
template <>
snd_pcm_format_t AlsaUtils::translateSoundFormat<FLOAT_PACKED>()
{
    return SND_PCM_FORMAT_FLOAT;
}

enum AlsaDeviceError
{
    ALSA_NO_ERROR,
    SET_SND_PCM_ACCESS_RW_INTERLEAVED_ERROR,
    NOT_POLLABLE_DEVICE_ERROR,
    SET_SND_PCM_FORMAT_ERROR,
    SET_AUDIO_CHANNELS_ERROR,
    SET_SAMPLE_RATE_ERROR,
    SET_PERIOD_SIZE_ERROR,
    SET_PCM_HW_PARAMS_ERROR,
    SND_PCM_START_ERROR,
    ALSA_DEV_DISCONNECTED,
    ALSA_OPEN_DEVICE_ERROR
};

template <typename CodecOrFormat, typename audioSampleRate, typename audioChannels>
class AlsaGrabber : private EventsProducer, public Medium
{

public:
    
    AlsaGrabber(SharedEventsCatcher eventsCatcher, const std::string& devName, 
                snd_pcm_uframes_t samplesPerPeriod = 0, const std::string& id = "") :
        EventsProducer::EventsProducer(eventsCatcher),
        Medium(id),
        mAlsaError(ALSA_NO_ERROR),
        mDeviceStatus(DEV_INITIALIZING),
        mErrno(0),
        mUnrecoverableState(false),
        mPollFds(NULL),
        mDevName(devName),
        mSamplesAvaible(false)
    {

        snd_lib_error_set_handler(dontPrintErrors);

        if (samplesPerPeriod <= 0)
            mSamplesPerPeriod = 64;
        else
            mSamplesPerPeriod = samplesPerPeriod;

        try
        {
            openDevice();
            configureDevice();
            if (!mUnrecoverableState)
            {
                startCapturing();
                mInputStatus  = READY;
            }
        }
        catch (AlsaDeviceError err)
        {
        }
    }

    ~AlsaGrabber()
    {
        closeDevice();
        snd_config_update_free_global();
    }

    /*!
     *  \exception MediumException
     */
    AudioFrame<CodecOrFormat, audioSampleRate, audioChannels>& grabNextPeriod()
    {
        Medium::mOutputStatus  = NOT_READY;  
        
        if (mUnrecoverableState)
            throw MediumException(INPUT_NOT_READY);

        if (Medium::mInputStatus == PAUSED)
            throw MediumException(MEDIUM_PAUSED);

        if (mPollFds == NULL)
        {
            mInputStatus  = NOT_READY;
            mOutputStatus = NOT_READY;              
            try
            {
                openDevice();
                configureDevice();
                if (!mUnrecoverableState)
                {
                    startCapturing();
                    Medium::mInputStatus  = READY;                        
                }
            }
            catch (AlsaDeviceError err)
            {
                throw MediumException(INPUT_NOT_READY);
            }
        }

        if (mSamplesAvaible)
        {
            if (snd_pcm_state(mAlsaDevHandle) == SND_PCM_STATE_DISCONNECTED)
            {
                closeDevice();
                mDeviceStatus = DEV_DISCONNECTED;
                mAlsaError = ALSA_DEV_DISCONNECTED;
                mErrno = errno;
                throw MediumException(OUTPUT_NOT_READY);
            }
            else
            {
                mAlsaError = ALSA_NO_ERROR;
                mErrno = 0;
                mDeviceStatus = DEV_CAN_GRAB;
            }

            observeEventsOn(mPollFds[0].fd);
            snd_pcm_uframes_t availableSamples = snd_pcm_avail(mAlsaDevHandle);
            bool mustReset = false;
            if (availableSamples > mSamplesPerPeriod)
            {
                //FIXME?
                if (availableSamples > mSamplesPerPeriod*8)
                {
                    
                       loggerLevel2 << "Alsa Buffer underrun: available: " 
                       << availableSamples << " samplesPerPeriod: " << 
                       mSamplesPerPeriod << std::endl; 
                                
                    mustReset = true;               
                }
                availableSamples = mSamplesPerPeriod;                
            }
            mCapturedRawAudioFrame.setSize(availableSamples * 2 * (audioChannels::value + 1));
            mCapturedRawAudioFrame.setTimestampsToNow();
            int ret = snd_pcm_readi(mAlsaDevHandle, mCapturedRawAudioDataPtr, availableSamples);
            if (ret == -EBADFD || ret == -EPIPE || ret == -ESTRPIPE)
            {
                throw MediumException(OUTPUT_NOT_READY);
            }
            if(mustReset)
                snd_pcm_reset(mAlsaDevHandle);
            mSamplesAvaible = false;
        }
        else
        {
            throw MediumException(OUTPUT_NOT_READY);
        }

        mCapturedRawAudioFrame.mFactoryId = Medium::mId;
        mInputAudioFrameFactoryId = "ALSADEV";        
        Medium::mOutputStatus  = READY;   
        Medium::mDistanceFromInputAudioFrameFactory = 1;
        return mCapturedRawAudioFrame;
    }

    AudioFrameHolder<CodecOrFormat, audioSampleRate, audioChannels>&
    operator >>
    (AudioFrameHolder<CodecOrFormat, audioSampleRate, audioChannels>& audioFrameHolder)
    {
        Medium& m = audioFrameHolder;
        setDistanceFromInputAudioFrameFactory(m, 1);        
        setInputAudioFrameFactoryId(m, mId);       
        setStatusInPipe(m, READY);
        
        try
        {
            audioFrameHolder.hold(grabNextPeriod());
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);
        }
        return audioFrameHolder;
    }

    template <typename AudioCodec>
    FFMPEGAudioEncoder<CodecOrFormat, AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioEncoder<CodecOrFormat, AudioCodec, audioSampleRate, audioChannels>& audioEncoder)
    {
        Medium& m = audioEncoder;
        setDistanceFromInputAudioFrameFactory(m, 1);        
        setInputAudioFrameFactoryId(m, mId);   
        setStatusInPipe(m, READY);        
        
        try
        {
            audioEncoder.encode(grabNextPeriod());            
        }
        catch (const MediumException& e)
        {
            setStatusInPipe(m, NOT_READY);
        }
        return audioEncoder;
    }

    template <typename ConvertedPCMSoundFormat,
              typename convertedAudioSampleRate,
              typename convertedAudioChannels>
    FFMPEGAudioConverter<CodecOrFormat, audioSampleRate, audioChannels,
                         ConvertedPCMSoundFormat, convertedAudioSampleRate, convertedAudioChannels>&
    operator >>
    (FFMPEGAudioConverter<CodecOrFormat, audioSampleRate, audioChannels,
                          ConvertedPCMSoundFormat, convertedAudioSampleRate, convertedAudioChannels>&
                          audioConverter)
    {
        Medium& m = audioConverter;
        setDistanceFromInputAudioFrameFactory(m, 1);        
        setInputAudioFrameFactoryId(m, mId);          
        setStatusInPipe(m, READY);
        
        try
        {
            audioConverter.convert(grabNextPeriod());
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return audioConverter;
    }

    enum AlsaDeviceError getAlsaError() const
    {
        return mAlsaError;
    }

    int getErrno() const
    {
        return mErrno;
    }

    std::string getAlsaErrorString() const
    {
        std::string ret = "";
        switch(mAlsaError)
        {
        case ALSA_NO_ERROR:
            ret = "ALSA_NO_ERROR";
            break;
        case SET_SND_PCM_ACCESS_RW_INTERLEAVED_ERROR:
            ret = "SET_SND_PCM_ACCESS_RW_INTERLEAVED_ERROR";
            break;
        case NOT_POLLABLE_DEVICE_ERROR:
            ret = "NOT_POLLABLE_DEVICE_ERROR";
            break;
        case SET_SND_PCM_FORMAT_ERROR:
            ret = "SET_SND_PCM_FORMAT_ERROR";
            break;
        case SET_AUDIO_CHANNELS_ERROR:
            ret = "SET_AUDIO_CHANNELS_ERROR";
            break;
        case SET_SAMPLE_RATE_ERROR:
            ret = "SET_SAMPLE_RATE_ERROR";
            break;
        case SET_PERIOD_SIZE_ERROR:
            ret = "SET_PERIOD_SIZE_ERROR";
            break;
        case SET_PCM_HW_PARAMS_ERROR:
            ret = "SET_PCM_HW_PARAMS_ERROR";
            break;
        case SND_PCM_START_ERROR:
            ret = "SND_PCM_START_ERROR";
            break;
        case ALSA_DEV_DISCONNECTED:
            ret = "ALSA_DEV_DISCONNECTED";
            break;
        case ALSA_OPEN_DEVICE_ERROR:
            ret = "ALSA_OPEN_DEVICE_ERROR";
            break;
        }
        return ret;
    }

    enum DeviceStatus deviceStatus() const
    {
        return mDeviceStatus;
    }

    bool isInUnrecoverableState() const
    {
        return mUnrecoverableState;
    }

private:

    static void dontPrintErrors(const char *file, int line, const char *function, int err, const char *fmt, ...)
    {
    }


    /*!
     * \exception AlsaDeviceError
     */
    void openDevice()
    {
        if (snd_pcm_open(&mAlsaDevHandle, mDevName.c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0)
        {
            mAlsaError = ALSA_OPEN_DEVICE_ERROR;
            mErrno = errno;
            throw mAlsaError;
        }

        unsigned int count;

        if ((count = snd_pcm_poll_descriptors_count (mAlsaDevHandle)) <= 0)
        {
            mAlsaError = NOT_POLLABLE_DEVICE_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            throw mAlsaError;
        }

        mPollFds = (pollfd* )malloc(sizeof(struct pollfd) * count);
        snd_pcm_poll_descriptors(mAlsaDevHandle, mPollFds, count);
        makePollable(mPollFds[0].fd);
        mAlsaError = ALSA_NO_ERROR;
        mErrno = 0;
    }

    /*!
     * \exception AlsaDeviceError
     */    
    void configureDevice()
    {
        int dir = 0;
        unsigned int val;
        snd_pcm_hw_params_alloca(&mHWparams);
        snd_pcm_hw_params_any(mAlsaDevHandle, mHWparams);
        if (snd_pcm_hw_params_set_access(mAlsaDevHandle, mHWparams, SND_PCM_ACCESS_RW_INTERLEAVED) != 0)
        {
            mAlsaError = SET_SND_PCM_ACCESS_RW_INTERLEAVED_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            throw mAlsaError;
        }
        if (snd_pcm_hw_params_set_format(mAlsaDevHandle, mHWparams,
                                         AlsaUtils::translateSoundFormat<CodecOrFormat>()) != 0)
        {
            mAlsaError = SET_SND_PCM_FORMAT_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            throw mAlsaError;
        }
        if (snd_pcm_hw_params_set_channels(mAlsaDevHandle, mHWparams, (audioChannels::value + 1)) != 0)
        {
            mAlsaError = SET_AUDIO_CHANNELS_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            throw mAlsaError;
        }

        val = audioSampleRate::value;
        if (snd_pcm_hw_params_set_rate_near(mAlsaDevHandle, mHWparams, &val, &dir) != 0)
        {
            mAlsaError = SET_SAMPLE_RATE_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            throw mAlsaError;
        }

        snd_pcm_uframes_t periodSize = mSamplesPerPeriod;
        if (snd_pcm_hw_params_set_period_size_near(mAlsaDevHandle,  mHWparams,
                                                   &periodSize, &dir) != 0)
        {
            mAlsaError = SET_PERIOD_SIZE_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            throw mAlsaError;
        }
        
        snd_pcm_hw_params_get_period_size(mHWparams, &periodSize, &dir);
        if (periodSize != mSamplesPerPeriod)
            std::cerr << "[ALSAGrabber] WARNING: actual period size is set to: "
            << periodSize << ", which differs from wanted period size: "
            << mSamplesPerPeriod << "\n";

        mSamplesPerPeriod = periodSize;

        // write the params to the driver
        if (snd_pcm_hw_params(mAlsaDevHandle, mHWparams) < 0)
        {
            mAlsaError = SET_PCM_HW_PARAMS_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            throw mAlsaError;
        }
         // 2 bytes per sample * num  of channels
        unsigned int periodBufferSize = mSamplesPerPeriod * 2 * (audioChannels::value + 1);
        // TODO: manage out of memory error?
        mCapturedRawAudioDataPtr = new unsigned char[periodBufferSize*2]();
        fillAudioFrame(mCapturedRawAudioFrame);
        observeEventsOn(mPollFds[0].fd);
        mDeviceStatus = DEV_CONFIGURED;
        mAlsaError = ALSA_NO_ERROR;
        mErrno = 0;
    }

    /*!
     * \exception AlsaDeviceError
     */    
    void startCapturing()
    {
        if (snd_pcm_start(mAlsaDevHandle) != 0)
        {
            mAlsaError = SND_PCM_START_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            throw mAlsaError;
        }
        else
        {
            mDeviceStatus = DEV_CAN_GRAB;
            mAlsaError = ALSA_NO_ERROR;
            mErrno = 0;
            throw mAlsaError;
        }
    }

    void closeDevice()
    {
        if (mPollFds != NULL)
        {        
            snd_pcm_close(mAlsaDevHandle); 
            free(mPollFds);                
            mPollFds = NULL;
        }
        Medium::mInputStatus  = NOT_READY; 
        Medium::mOutputStatus = NOT_READY;         
    }

    void fillAudioFrame(PackedRawAudioFrame& rawAudioFrame)
    {
        unsigned int periodBufferSize = mSamplesPerPeriod * 2 * (audioChannels::value + 1);
        auto freePeriodDataMemory = [periodBufferSize] (unsigned char* audioFrameDataPtr)
        {
            delete [] audioFrameDataPtr;
        };
        ShareableAudioFrameData shareablePeriodData(mCapturedRawAudioDataPtr, freePeriodDataMemory);
        rawAudioFrame.assignDataSharedPtr(shareablePeriodData);
        rawAudioFrame.setSize(periodBufferSize);
    }

    void eventCallBack(int fd, enum EventType eventType)
    {
        mSamplesAvaible = true;
        dontObserveEventsOn(mPollFds[0].fd);
    }

    enum AlsaDeviceError mAlsaError;
    enum DeviceStatus mDeviceStatus;
    int mErrno;
    bool mUnrecoverableState;
    unsigned char* mCapturedRawAudioDataPtr;
    AudioFrame<CodecOrFormat, audioSampleRate, audioChannels> mCapturedRawAudioFrame;
    snd_pcm_hw_params_t* mHWparams;
    snd_pcm_t* mAlsaDevHandle;
    struct pollfd* mPollFds;
    std::string mDevName;
    bool mSamplesAvaible;
    snd_pcm_uframes_t mSamplesPerPeriod;
};

}

#endif // LINUX

#endif // ALSAGRABBER_HPP_INCLUDED


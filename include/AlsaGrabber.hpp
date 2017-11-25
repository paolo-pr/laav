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
#include "AllAudioCodecsAndFormats.hpp"
#include "Common.hpp"
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

template <typename CodecOrFormat, unsigned int audioSampleRate, enum AudioChannels audioChannels>
class AlsaGrabber : public EventsProducer
{

public:

    AlsaGrabber(SharedEventsCatcher eventsCatcher,
                const std::string& devName, snd_pcm_uframes_t samplesPerPeriod = 0):
        EventsProducer::EventsProducer(eventsCatcher),
        mAlsaError(ALSA_NO_ERROR),
        mStatus(DEV_INITIALIZING),
        mErrno(0),
        mUnrecoverableState(false),
        mPollFds(NULL),
        mDevName(devName),
        mSamplesAvaible(false)
    {

        snd_lib_error_set_handler(dontPrintErrors);

        if (samplesPerPeriod == 0)
            mSamplesPerPeriod = 64;
        else
            mSamplesPerPeriod = samplesPerPeriod;

        if (openDevice())
        {
            configureDevice();
            if (!mUnrecoverableState)
                startCapturing();
        }
    }

    ~AlsaGrabber()
    {
        closeDevice();
        snd_config_update_free_global();
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    AudioFrame<CodecOrFormat, audioSampleRate, audioChannels>& grabNextPeriod()
    {
        if (mUnrecoverableState)
        {
            throw MediaException(MEDIA_NO_DATA);
        }

        if (mPollFds == NULL)
        {
            if (openDevice())
            {
                configureDevice();
                if (!mUnrecoverableState)
                    startCapturing();
            }
            else
                throw MediaException(MEDIA_NO_DATA);
        }

        if (mSamplesAvaible)
        {
            if (snd_pcm_state(mAlsaDevHandle) == SND_PCM_STATE_DISCONNECTED)
            {
                closeDevice();
                mStatus = DEV_DISCONNECTED;
                mAlsaError = ALSA_DEV_DISCONNECTED;
                mErrno = errno;
                throw MediaException(MEDIA_NO_DATA);
            }
            else
            {
                mAlsaError = ALSA_NO_ERROR;
                mErrno = 0;
                mStatus = DEV_CAN_GRAB;
            }

            observeEventsOn(mPollFds[0].fd);
            snd_pcm_uframes_t availableSamples = snd_pcm_avail(mAlsaDevHandle);
            bool mustReset = false;
            if (availableSamples > mSamplesPerPeriod)
            {
                //FIXME?
                if (availableSamples > mSamplesPerPeriod*8)
                {
                    /* 
                       std::cerr << "Alsa Buffer underrun: available: " 
                       << availableSamples << " samplesPerPeriod: " << 
                       mSamplesPerPeriod << "\n"; 
                    */                    
                    mustReset = true;
                }
                availableSamples = mSamplesPerPeriod;                
            }
            mCapturedRawAudioFrame.setSize(availableSamples * 2 * (audioChannels + 1));
            mCapturedRawAudioFrame.setTimestampsToNow();
            int ret = snd_pcm_readi(mAlsaDevHandle, mCapturedRawAudioDataPtr, availableSamples);
            if (ret == -EBADFD || ret == -EPIPE || ret == -ESTRPIPE)
            {
                throw MediaException(MEDIA_NO_DATA);
            }
            if(mustReset)
                snd_pcm_reset(mAlsaDevHandle);
            mSamplesAvaible = false;
        }
        else
        {
            throw MediaException(MEDIA_NO_DATA);
        }

        return mCapturedRawAudioFrame;
    }

    AudioFrameHolder<CodecOrFormat, audioSampleRate, audioChannels>&
    operator >>
    (AudioFrameHolder<CodecOrFormat, audioSampleRate, audioChannels>& audioFrameHolder)
    {
        try
        {
            audioFrameHolder.hold(grabNextPeriod());
            audioFrameHolder.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            audioFrameHolder.mMediaStatusInPipe = mediaException.cause();
        }
        return audioFrameHolder;
    }

    template <typename AudioCodec>
    FFMPEGAudioEncoder<CodecOrFormat, AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioEncoder<CodecOrFormat, AudioCodec, audioSampleRate, audioChannels>& audioEncoder)
    {
        try
        {
            audioEncoder.encode(grabNextPeriod());
            audioEncoder.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& e)
        {
            audioEncoder.mMediaStatusInPipe = e.cause();
        }
        return audioEncoder;
    }

    template <typename ConvertedPCMSoundFormat,
              unsigned int convertedAudioSampleRate,
              enum AudioChannels convertedAudioChannels>
    FFMPEGAudioConverter<CodecOrFormat, audioSampleRate, audioChannels,
                         ConvertedPCMSoundFormat, convertedAudioSampleRate, convertedAudioChannels>&
    operator >>
    (FFMPEGAudioConverter<CodecOrFormat, audioSampleRate, audioChannels,
                          ConvertedPCMSoundFormat, convertedAudioSampleRate, convertedAudioChannels>&
                          audioConverter)
    {
        try
        {
            audioConverter.convert(grabNextPeriod());
            audioConverter.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            audioConverter.mMediaStatusInPipe = mediaException.cause();
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

    enum DeviceStatus status() const
    {
        return mStatus;
    }

    bool isInUnrecoverableState() const
    {
        return mUnrecoverableState;
    }

private:

    static void dontPrintErrors(const char *file, int line, const char *function, int err, const char *fmt, ...)
    {
    }


    bool openDevice()
    {
        if (snd_pcm_open(&mAlsaDevHandle, mDevName.c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0)
        {
            mAlsaError = ALSA_OPEN_DEVICE_ERROR;
            mErrno = errno;
            return false;
        }

        unsigned int count;

        if ((count = snd_pcm_poll_descriptors_count (mAlsaDevHandle)) <= 0)
        {
            mAlsaError = NOT_POLLABLE_DEVICE_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            return false;
        }

        mPollFds = (pollfd* )malloc(sizeof(struct pollfd) * count);
        snd_pcm_poll_descriptors(mAlsaDevHandle, mPollFds, count);
        makePollable(mPollFds[0].fd);
        mAlsaError = ALSA_NO_ERROR;
        mErrno = 0;
        return true;
    }

    bool configureDevice()
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
            return false;
        }
        if (snd_pcm_hw_params_set_format(mAlsaDevHandle, mHWparams,
                                         AlsaUtils::translateSoundFormat<CodecOrFormat>()) != 0)
        {
            mAlsaError = SET_SND_PCM_FORMAT_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            return false;
        }
        if (snd_pcm_hw_params_set_channels(mAlsaDevHandle, mHWparams, (audioChannels + 1)) != 0)
        {
            mAlsaError = SET_AUDIO_CHANNELS_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            return false;
        }

        val = audioSampleRate;
        if (snd_pcm_hw_params_set_rate_near(mAlsaDevHandle, mHWparams, &val, &dir) != 0)
        {
            mAlsaError = SET_SAMPLE_RATE_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            return false;
        }

        snd_pcm_uframes_t periodSize = mSamplesPerPeriod;
        if (snd_pcm_hw_params_set_period_size_near(mAlsaDevHandle,  mHWparams,
                                                   &periodSize, &dir) != 0)
        {
            mAlsaError = SET_PERIOD_SIZE_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            return false;
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
            return false;
        }
         // 2 bytes per sample * num  of channels
        unsigned int periodBufferSize = mSamplesPerPeriod * 2 * (audioChannels + 1);
        // TODO: manage out of memory error?
        mCapturedRawAudioDataPtr = new unsigned char[periodBufferSize*2]();
        fillAudioFrame(mCapturedRawAudioFrame);
        observeEventsOn(mPollFds[0].fd);
        mStatus = DEV_CONFIGURED;
        mAlsaError = ALSA_NO_ERROR;
        mErrno = 0;
        return true;
    }

    bool startCapturing()
    {
        if (snd_pcm_start(mAlsaDevHandle) != 0)
        {
            mAlsaError = SND_PCM_START_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            return false;
        }
        else
        {
            mStatus = DEV_CAN_GRAB;
            mAlsaError = ALSA_NO_ERROR;
            mErrno = 0;
            return true;
        }
    }

    bool closeDevice()
    {
        if (mPollFds != NULL)
        {
            free(mPollFds);
            mPollFds = NULL;
            snd_pcm_close(mAlsaDevHandle);
        }

        return true;
    }

    void fillAudioFrame(PackedRawAudioFrame& rawAudioFrame)
    {
        unsigned int periodBufferSize = mSamplesPerPeriod * 2 * (audioChannels + 1);
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
    enum DeviceStatus mStatus;
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


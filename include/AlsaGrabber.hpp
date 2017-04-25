/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
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

enum AlsaDeviceStatus
{
    SET_SND_PCM_ACCESS_RW_INTERLEAVED_ERROR,
    NOT_POLLABLE_DEVICE_ERROR,
    SET_SND_PCM_FORMAT_ERROR,
    SET_AUDIO_CHANNELS_ERROR,
    SET_SAMPLE_RATE_ERROR,
    SET_PERIOD_SIZE_ERROR,
    SET_PCM_HW_PARAMS_ERROR,
    OUTPUT_FILE_ERROR,
    SND_PCM_START_ERROR,
    ALSA_OPEN_DEVICE_ERROR,
    ALSA_CLOSE_DEVICE_ERROR,
    ALSA_DEVICE_INITIALIZING,
    ALSA_DEVICE_DISCONNECTED,
    ALSA_DEVICE_CONFIGURED,
    ALSA_DEVICE_GRABBING
};

template <typename CodecOrFormat, unsigned int audioSampleRate, enum AudioChannels audioChannels>
class AlsaGrabber : public EventsProducer
{

public:

    AlsaGrabber(SharedEventsCatcher eventsCatcher,
                const std::string& devName, snd_pcm_uframes_t samplesPerPeriod = 0):
        EventsProducer::EventsProducer(eventsCatcher),
        mStatus(ALSA_DEVICE_INITIALIZING),
        mUnrecoverableState(false),
        mPollFds(NULL),
        mDevName(devName),
        mSamplesAvaible(false)
    {
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
                mStatus = ALSA_DEVICE_DISCONNECTED;
                throw MediaException(MEDIA_NO_DATA);
            }
            else
                mStatus = ALSA_DEVICE_GRABBING;

            observeEventsOn(mPollFds[0].fd);
            snd_pcm_uframes_t availableSamples = snd_pcm_avail(mAlsaDevHandle);
            if (availableSamples > mSamplesPerPeriod)
                availableSamples = mSamplesPerPeriod;
            mCapturedRawAudioFrame.setSize(availableSamples * 2 * (audioChannels + 1));
            mCapturedRawAudioFrame.setTimestampsToNow();
            int ret = snd_pcm_readi(mAlsaDevHandle, mCapturedRawAudioDataPtr, availableSamples);
            if (ret == -EBADFD || ret == -EPIPE || ret == -ESTRPIPE)
            {
                throw MediaException(MEDIA_NO_DATA);
            }

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

    enum AlsaDeviceStatus status() const
    {
        return mStatus;
    }

private:

    bool openDevice()
    {
        if (snd_pcm_open(&mAlsaDevHandle, mDevName.c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0)
        {
            mStatus = ALSA_OPEN_DEVICE_ERROR;
            return false;
        }

        unsigned int count;

        if ((count = snd_pcm_poll_descriptors_count (mAlsaDevHandle)) <= 0)
        {
            mStatus = NOT_POLLABLE_DEVICE_ERROR;
            mUnrecoverableState = true;
            return false;
        }

        mPollFds = (pollfd* )malloc(sizeof(struct pollfd) * count);
        snd_pcm_poll_descriptors(mAlsaDevHandle, mPollFds, count);
        makePollable(mPollFds[0].fd);
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
            mStatus = SET_SND_PCM_ACCESS_RW_INTERLEAVED_ERROR;
            mUnrecoverableState = true;
            return false;
        }
        if (snd_pcm_hw_params_set_format(mAlsaDevHandle, mHWparams,
                                         AlsaUtils::translateSoundFormat<CodecOrFormat>()) != 0)
        {
            mStatus = SET_SND_PCM_FORMAT_ERROR;
            mUnrecoverableState = true;
            return false;
        }
        if (snd_pcm_hw_params_set_channels(mAlsaDevHandle, mHWparams, (audioChannels + 1)) != 0)
        {
            mStatus = SET_AUDIO_CHANNELS_ERROR;
            mUnrecoverableState = true;
            return false;
        }

        val = audioSampleRate;
        if (snd_pcm_hw_params_set_rate_near(mAlsaDevHandle, mHWparams, &val, &dir) != 0)
        {
            mStatus = SET_SAMPLE_RATE_ERROR;
            mUnrecoverableState = true;
            return false;
        }

        if (snd_pcm_hw_params_set_period_size_near(mAlsaDevHandle,  mHWparams,
                                                   &mSamplesPerPeriod, &dir) != 0)
        {
            mStatus = SET_PERIOD_SIZE_ERROR;
            mUnrecoverableState = true;
            return false;
        }

        // write the params to the driver
        if (snd_pcm_hw_params(mAlsaDevHandle, mHWparams) < 0)
        {
            mStatus = SET_PCM_HW_PARAMS_ERROR;
            mUnrecoverableState = true;
            return false;
        }
         // 2 bytes per sample * num  of channels
        unsigned int periodBufferSize = mSamplesPerPeriod * 2 * (audioChannels + 1);
        // TODO: manage out of memory error?
        mCapturedRawAudioDataPtr = new unsigned char[periodBufferSize*2]();
        fillAudioFrame(mCapturedRawAudioFrame);
        observeEventsOn(mPollFds[0].fd);
        mStatus = ALSA_DEVICE_CONFIGURED;
        return true;
    }

    bool startCapturing()
    {
        if (snd_pcm_start(mAlsaDevHandle) != 0)
        {
            mUnrecoverableState = true;
            return false;
        }
        else
        {
            mStatus = ALSA_DEVICE_GRABBING;
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

    enum AlsaDeviceStatus mStatus;
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


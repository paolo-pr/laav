/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef FFMPEGAUDIOVIDEOMUXER_HPP_INCLUDED
#define FFMPEGAUDIOVIDEOMUXER_HPP_INCLUDED

#include "FFMPEGVideoMuxer.hpp"
#include "FFMPEGAudioMuxer.hpp"

namespace laav
{

template <typename Container,
          typename VideoCodecOrFormat,
          unsigned int width,
          unsigned int height,
          typename AudioCodecOrFormat,
          unsigned int audioSampleRate,
          enum AudioChannels audioChannels>
class FFMPEGAudioVideoMuxer :
public FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat, width, height>,
public FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat, audioSampleRate, audioChannels>
{

    template <typename Container_,
              typename VideoCodecOrFormat_,
              unsigned int width_,
              unsigned int height_,
              typename AudioCodecOrFormat_,
              unsigned int audioSampleRate_,
              enum AudioChannels audioChannels_>
   friend class HTTPAudioVideoStreamer;

public:

    FFMPEGAudioVideoMuxer(bool startMuxingSoon = false) :
        FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat,
                             width, height>(startMuxingSoon),
        FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat,
                             audioSampleRate, audioChannels>(startMuxingSoon)
    {

        this->mMuxAudio = true;
        this->mMuxVideo = true;
        FFMPEGMuxerCommonImpl<Container>::completeMuxerInitialization();
        auto freeNothing = [](unsigned char* buffer) {  };

        mMuxedAudioVideoChunks.resize(this->mMuxedChunks.data.size());
        unsigned int n;
        for (n = 0; n < mMuxedAudioVideoChunks.size(); n++)
        {
            ShareableMuxedData shMuxedData(this->mMuxedChunks.data[n].ptr, freeNothing);
            mMuxedAudioVideoChunks[n].assignDataSharedPtr(shMuxedData);
        }
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    const std::vector<MuxedAudioVideoData<Container, VideoCodecOrFormat, width, height,
                                          AudioCodecOrFormat, audioSampleRate, audioChannels> >&
    muxedAudioVideoChunks()
    {
        if (this->mMuxedChunks.newGroupOfChunks)
        {
            unsigned int n;
            for (n = 0; n < this->mMuxedChunks.offset; n++)
            {
                mMuxedAudioVideoChunks[n].setSize(this->mMuxedChunks.data[n].size);
            }
            return mMuxedAudioVideoChunks;
        }
        else
        {
            throw MediaException(MEDIA_NO_DATA);
        }
    }

    unsigned int muxedAudioVideoChunksOffset() const
    {
        return this->mMuxedChunks.offset;
    }

    void takeMuxableFrame(const AudioFrame<AudioCodecOrFormat,
                          audioSampleRate, audioChannels>& audioFrameToMux)
    {
        FFMPEGMuxerAudioImpl<Container, AudioCodecOrFormat,
                             audioSampleRate, audioChannels>::takeMuxableFrame(audioFrameToMux);
    }

    void takeMuxableFrame(const VideoFrame<VideoCodecOrFormat, width, height>& videoFrameToMux)
    {
        FFMPEGMuxerVideoImpl<Container, VideoCodecOrFormat,
                             width, height>::takeMuxableFrame(videoFrameToMux);
    }

private:

    std::vector<MuxedAudioVideoData<Container, VideoCodecOrFormat,
                                    width, height,
                                    AudioCodecOrFormat, audioSampleRate,
                                    audioChannels> > mMuxedAudioVideoChunks;

};

}

#endif // FFMPEGAUDIOVIDEOMUXER_HPP_INCLUDED

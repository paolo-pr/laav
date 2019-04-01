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
 *
 */

#ifndef FRAME_HPP_INCLUDED
#define FRAME_HPP_INCLUDED

#include <vector>
#include <ctime>
#include <iomanip>
#include "Common.hpp"
#include "Pixel.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
}

namespace laav
{

class FrameBase
{

    template <typename InputPCMSoundFormat, typename inputAudioSampleRate,
              typename inputAudioChannels, typename ConvertedPCMSoundFormat,
              typename convertedAudioSampleRate, typename convertedAudioChannels>
    friend class FFMPEGAudioConverter;   
    
    template <typename InputVideoFrameFormat, typename inputWidth, typename inputHeight,
              typename ConvertedVideoFrameFormat, typename outputWidth, typename outputHeight>
    friend class FFMPEGVideoConverter;    

    template <typename CodecOrFormat, typename width, typename height>
    friend class VideoFrameHolder;

    template <typename CodecOrFormat, typename audioSampleRate, typename audioChannels>
    friend class AudioFrameHolder;    
    
    template <typename Container, typename VideoCodecOrFormat,
              typename width, typename height>
    friend class FFMPEGMuxerVideoImpl;

    template <typename Container, typename AudioCodecOrFormat,
              typename audioSampleRate, typename audioChannels>
    friend class FFMPEGMuxerAudioImpl;

    template <typename RawVideoFrameFormat, typename EncodedVideoFrameCodec,
              typename width, typename height>
    friend class FFMPEGVideoEncoder;

    template <typename PCMSoundFormat, typename AudioCodec,
              typename audioSampleRate, typename audioChannels>
    friend class FFMPEGAudioEncoder;
    
    template <typename CodecOrFormat, typename width, typename height>
    friend class V4L2Grabber;
    
    template <typename CodecOrFormat, typename audioSampleRate, typename audioChannels>
    friend class AlsaGrabber;

    template <typename width, typename height>
    friend class FFMPEGMJPEGDecoder;    

    template <typename Container, typename AudioCodecOrFormat,
              typename audioSampleRate, typename audioChannels>
    friend class HTTPAudioStreamer;

    template <typename Container, typename VideoCodecOrFormat,
              typename width, typename height>
    friend class HTTPVideoStreamer;
    
    template <typename Container, typename AudioCodecOrFormat,
              typename audioSampleRate, typename audioChannels>
    friend class UDPAudioStreamer;

    template <typename Container, typename VideoCodecOrFormat,
              typename width, typename height>
    friend class UDPVideoStreamer;    
    
    template <typename Container,
            typename VideoCodecOrFormat, typename width, typename height,
            typename AudioCodecOrFormat, typename audioSampleRate, typename audioChannels>
    friend class HTTPAudioVideoStreamer;
    
    template <typename Container,
            typename VideoCodecOrFormat, typename width, typename height,
            typename AudioCodecOrFormat, typename audioSampleRate, typename audioChannels>
    friend class UDPAudioVideoStreamer;    
    
public:
    
    FrameBase():
        mTimeBase(AV_TIME_BASE_Q),
        mLibAVFlags(0),
        // TODO: don't use the FFMPEG no value field
        mAVCodecContext(NULL),
        mMonotonicTs(AV_NOPTS_VALUE),
        mDateTs(-1),
        mDistanceFromFactory(0)
    {
    }

    void setTimestampsToNow()
    {
        mMonotonicTs = av_gettime_relative();
        struct timespec dateTimeNow;
        clock_gettime(CLOCK_REALTIME, &dateTimeNow);
        mDateTs = dateTimeNow.tv_sec * 1000000000 + dateTimeNow.tv_nsec;
    }

    void setMonotonicTimeBase(AVRational& timeBase)
    {
        mTimeBase = timeBase;
    }

    void setMonotonicTimestamp(int64_t timestamp)
    {
        mMonotonicTs = timestamp;
    }

    void setDateTimestamp(int64_t timestamp)
    {
        mDateTs = timestamp;
    }

    int64_t monotonicTimestamp() const
    {
        return mMonotonicTs;
    }

    int64_t dateTimestamp() const
    {
        return mDateTs;
    }

    void printDate() const
    {
        timespec ts;
        ts.tv_sec = mDateTs / 1000000000;
        ts.tv_nsec = mDateTs % 1000000000;
        char day[4], mon[4];
        int wday, hh, mm, ss, year;
        sscanf(ctime((time_t*) & (ts.tv_sec)), "%s %s %d %d:%d:%d %d",
               day, mon, &wday, &hh, &mm, &ss, &year);
        std::cerr << day << ' '
                  << mon << ' '
                  << wday << ' '
                  << hh << ':' << mm << ':' << ss << '.' << ts.tv_nsec << ' '
                  << year << "\n";
    }

private:
    
    AVRational mTimeBase;
    // set by FFMPEGVideoEncoder, accessed by FFMPEGMuxerVideoImpl
    int mLibAVFlags;
    // set by FFMPEGVideoEncoder, accessed by FFMPEGMuxerVideoImpl
    AVPacketSideData* mLibAVSideData;
    AVCodecContext* mAVCodecContext;
    int mLibAVSideDataElems;
    int64_t mMonotonicTs;
    int64_t mDateTs;
    // accessed by all friends
    std::string mFactoryId;
    unsigned mDistanceFromFactory;

};

template <typename Container, typename AudioCodecOrFormat,
          typename audioSampleRate, typename audioChannels>
class MuxedAudioData
{
public:

    MuxedAudioData():
        // DUMMY data that will be replaced by the producer's data.
        mData(new unsigned char()),
        mSize(0)
    {
    }

    const unsigned char* data() const
    {
        return mData.get();
    }

    unsigned char* data()
    {
        return mData.get();
    }

    unsigned int size() const
    {
        return mSize;
    }

    void assignDataSharedPtr(ShareableMuxedData& shareableMuxedData)
    {
        mData = shareableMuxedData;
    }

    void setSize(unsigned int size)
    {
        mSize = size;
    }

    bool isEmpty() const
    {
        return mSize == 0;
    }
    
private:

    ShareableMuxedData mData;
    unsigned int mSize;
};

template <typename Container, typename VideoCodecOrFormat,
          typename width, typename height>
class MuxedVideoData
{
public:

    MuxedVideoData():
        // DUMMY data that will be replaced by the producer's data.
        mData(new unsigned char()),
        mSize(0)
    {
    }

    const unsigned char* data() const
    {
        return mData.get();
    }

    unsigned char* data()
    {
        return mData.get();
    }

    unsigned int size() const
    {
        return mSize;
    }

    void assignDataSharedPtr(ShareableMuxedData& shareableMuxedData)
    {
        mData = shareableMuxedData;
    }

    void setSize(unsigned int size)
    {
        mSize = size;
    }

    bool isEmpty() const
    {
        return mSize == 0;
    }
    
private:

    ShareableMuxedData mData;
    unsigned int mSize;
};

template <typename Container, typename VideoCodecOrFormat, typename width, typename height,
          typename AudioCodecOrFormat, typename audioSampleRate, typename audioChannels>
class MuxedAudioVideoData
{
public:

    MuxedAudioVideoData():
        // DUMMY data that will be replaced by the producer's data.
        mData(new unsigned char()),
        mSize(0)
    {
    }

    const unsigned char* data() const
    {
        return mData.get();
    }

    unsigned char* data()
    {
        return mData.get();
    }

    unsigned int size() const
    {
        return mSize;
    }

    void assignDataSharedPtr(ShareableMuxedData& shareableMuxedData)
    {
        mData = shareableMuxedData;
    }

    void setSize(unsigned int size)
    {
        mSize = size;
    }
    
    bool isEmpty() const
    {
        return mSize == 0;
    }
    

private:

    ShareableMuxedData mData;
    unsigned int mSize;
};

template <typename AudioCodec>
class EncodedAudioFrame : public FrameBase
{
public:

    EncodedAudioFrame():
        // DUMMY data that will be replaced by the producer's data.
        mData(new unsigned char()),
        mSize(0)
    {
    }

    const unsigned char* data() const
    {
        return mData.get();
    }

    unsigned char* data()
    {
        return mData.get();
    }

    unsigned int size() const
    {
        return mSize;
    }

    void assignDataSharedPtr(ShareableAudioFrameData& shareableAudioFrameData)
    {
        mData = shareableAudioFrameData;
    }

    void setSize(unsigned int size)
    {
        mSize = size;
    }
    
    bool isEmpty() const
    {
        return mSize == 0;
    }

private:

    ShareableAudioFrameData mData;
    unsigned int mSize;
};

template <typename width_, typename height_>
class VideoFrameBase : public FrameBase
{
public:

    unsigned int width() const
    {
        return width_::value;
    }

    unsigned int height() const
    {
        return height_::value;
    }

};

class EncodedVideoFrame
{

public:

    EncodedVideoFrame(unsigned int width, unsigned int height)
        :
        mSize(0),
        mData(new unsigned char())
    {
    }

    // TODO implement
    /*
    const unsigned char* header() const
    {
        return NULL;
    }
    */

    void assignDataSharedPtr(ShareableVideoFrameData& shareableVideoFrameData )
    {
        mData = shareableVideoFrameData;
    }

    const unsigned char* data() const
    {
        return mData.get();
    }

    unsigned char* data()
    {
        return mData.get();
    }

    unsigned int size() const
    {
        return mSize;
    }

    void setSize(unsigned int size)
    {
        mSize = size;
    }
    
    bool isEmpty() const
    {
        return mSize == 0;
    }
    
private:

    unsigned int mSize;
    ShareableVideoFrameData mData;
};

class Planar3RawVideoFrame
{
public:

    Planar3RawVideoFrame(unsigned int width, unsigned int height)
    {
        // DUMMY data that will be replaced by the producer's data.
        mPlanes.push_back(ShareableVideoFrameData(new unsigned char));
        mPlanes.push_back(ShareableVideoFrameData(new unsigned char));
        mPlanes.push_back(ShareableVideoFrameData(new unsigned char));
        mSizes.push_back(0);
        mSizes.push_back(0);
        mSizes.push_back(0);
    }

    template <unsigned int planeNum>
    const unsigned char* plane() const
    {
        static_assert(planeNum <= 2, "Can't get data for plane with index > 2");
        return mPlanes[planeNum].get();
    }

    template <unsigned int planeNum>
    unsigned char* plane()
    {
        static_assert(planeNum <= 2, "Can't get data for plane with index > 2");
        return mPlanes[planeNum].get();
    }

    template <unsigned int planeNum>
    void assignSharedPtrForPlane(ShareableVideoFrameData& shareableVideoFrameData)
    {
        static_assert(planeNum <= 2, "Can't assign data for plane with index > 2");
        mPlanes[planeNum] = shareableVideoFrameData;
    }

    template <unsigned int planeNum>
    void setSize(unsigned int size)
    {
        static_assert(planeNum <= 2, "Can't assign size for plane with index > 2");
        mSizes[planeNum] = size;
    }

    template <unsigned int planeNum>
    unsigned int size() const
    {
        static_assert(planeNum <= 2, "Can't get size for plane with index > 2");
        return mSizes[planeNum];
    }

    bool isEmpty() const
    {
        return size<0>() == 0;
    }     
    
private:

    std::vector<unsigned int> mSizes;
    std::vector<ShareableVideoFrameData> mPlanes;

};

class PackedRawVideoFrame
{

public:

    PackedRawVideoFrame(unsigned int width, unsigned int height):
        // DUMMY data that will be replaced by the producer's data.
        mData(new unsigned char()),
        mSize(0)
    {
    }

    const unsigned char* data() const
    {
        return mData.get();
    }

    unsigned char* data()
    {
        return mData.get();
    }

    unsigned int size() const
    {
        return mSize;
    }

    // The design follows this rule: a frame container (PackedRawVideoFrame,
    // Planar3RawVideoFrame etc.) doesn't share its internal pointer
    // (it only allows to access the ptr's data)...
    // ...BUT it accepts shared ptr made by someone else (for example: a Converter, a Framer etc.)
    void assignDataSharedPtr(ShareableVideoFrameData& shareableVideoFrameData )
    {
        mData = shareableVideoFrameData;
    }

    void setSize(unsigned int size)
    {
        mSize = size;
    }

    bool isEmpty() const
    {
        return mSize == 0;
    }
        
private:

    ShareableVideoFrameData mData;
    unsigned int mSize;

};

template <typename... Components>
class FormattedRawVideoFrame
{

public:

    virtual const Pixel<Components... >& pixelAt(uint16_t x, uint16_t y) const = 0;

    void setPixelAt(const Pixel<Components...>& pixel, uint16_t x, uint16_t y);

protected:

    mutable Pixel<Components... > mCurrPixel;
};

template <typename CodecOrFormat, typename width, typename height>
class VideoFrame
{
};

class Planar2RawAudioFrame
{
public:

    Planar2RawAudioFrame()
    {
        // DUMMY 0-data that will be replaced by the producer's data.
        mPlanes.push_back(ShareableAudioFrameData(new unsigned char));
        mPlanes.push_back(ShareableAudioFrameData(new unsigned char));
        mPlanes.push_back(ShareableAudioFrameData(new unsigned char));
        mSizes.push_back(0);
        mSizes.push_back(0);
        mSizes.push_back(0);
    }

    template <unsigned int planeNum>
    const unsigned char* plane() const
    {
        static_assert(planeNum <= 1, "Can't get data for plane with index > 1");
        return mPlanes[planeNum].get();
    }

    template <unsigned int planeNum>
    unsigned char* plane()
    {
        static_assert(planeNum <= 1, "Can't get data for plane with index > 1");
        return mPlanes[planeNum].get();
    }

    template <unsigned int planeNum>
    void assignSharedPtrForPlane(ShareableAudioFrameData& shareableAudioFrameData)
    {
        static_assert(planeNum <= 1, "Can't assign data for plane with index > 1");
        mPlanes[planeNum] = shareableAudioFrameData;
    }

    template <unsigned int planeNum>
    void setSize(unsigned int size)
    {
        static_assert(planeNum <= 1, "Can't assign size for plane with index > 1");
        mSizes[planeNum] = size;
    }

    template <unsigned int planeNum>
    unsigned int size() const
    {
        static_assert(planeNum <= 1, "Can't get size for plane with index > 1");
        return mSizes[planeNum];
    }
    
    bool isEmpty() const
    {
        return size<0>() == 0;
    }

private:

    std::vector<unsigned int> mSizes;
    std::vector<ShareableAudioFrameData> mPlanes;

};

class PackedRawAudioFrame
{
public:

    PackedRawAudioFrame():
        mData(new unsigned char()),
        mSize(0)
    {
    }

    const unsigned char* data() const
    {
        return mData.get();
    }

    unsigned char* data()
    {
        return mData.get();
    }

    unsigned int size() const
    {
        return mSize;
    }

    void assignDataSharedPtr(ShareableAudioFrameData& shareableAudioFrameData)
    {
        mData = shareableAudioFrameData;
    }

    void setSize(unsigned int size)
    {
        mSize = size;
    }

    bool isEmpty() const
    {
        return mSize == 0;
    }    
    
private:

    ShareableAudioFrameData mData;
    unsigned int mSize;

};

template <typename CodecOrFormat, typename audioSampleRate, typename audioChannels>
class AudioFrame : public FrameBase
{
};

}

#endif // FRAME_HPP_INCLUDED

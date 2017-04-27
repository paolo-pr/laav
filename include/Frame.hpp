/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
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

template <typename InputVideoFrameFormat, unsigned int inputWidth, unsigned int inputheight,
          typename ConvertedVideoFrameFormat, unsigned int outputWidth, unsigned int outputheight>
class FFMPEGVideoConverter;

class Frame
{

    template <typename Container, typename VideoCodecOrFormat,
              unsigned int width, unsigned int height>
    friend class FFMPEGMuxerVideoImpl;

    template <typename Container, typename AudioCodecOrFormat,
              unsigned int audioSampleRate, enum AudioChannels audioChannels>
    friend class FFMPEGMuxerAudioImpl;

    template <typename RawVideoFrameFormat, typename EncodedVideoFrameCodec,
              unsigned int width, unsigned int height>
    friend class FFMPEGVideoEncoder;

    template <typename PCMSoundFormat, typename AudioCodec,
              unsigned int audioSampleRate, enum AudioChannels audioChannels>
    friend class FFMPEGAudioEncoder;

public:

    Frame():
        mTimeBase(AV_TIME_BASE_Q),
        mLibAVFlags(0),
        // TODO: don't use the FFMPEG no value field
        mMonotonicTs(AV_NOPTS_VALUE),
        mDateTs(-1)
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
    int mLibAVSideDataElems;
    int64_t mMonotonicTs;
    int64_t mDateTs;

};

template <typename Container, typename AudioCodecOrFormat,
          unsigned int audioSampleRate, enum AudioChannels>
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

private:

    ShareableMuxedData mData;
    unsigned int mSize;
};

template <typename Container, typename VideoCodecOrFormat,
          unsigned int width, unsigned int height>
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

private:

    ShareableMuxedData mData;
    unsigned int mSize;
};

template <typename Container, typename VideoCodecOrFormat, unsigned int width, unsigned int height,
          typename AudioCodecOrFormat, unsigned int audioSampleRate, enum AudioChannels>
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

private:

    ShareableMuxedData mData;
    unsigned int mSize;
};

template <typename AudioCodec>
class EncodedAudioFrame : public Frame
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

private:

    ShareableAudioFrameData mData;
    unsigned int mSize;
};

template <unsigned int width_, unsigned int height_>
class VideoFrameBase : public Frame
{
public:

    unsigned int width() const
    {
        return width_;
    }

    unsigned int height() const
    {
        return height_;
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

template <typename CodecOrFormat, unsigned int width, unsigned int height>
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
    void foo()
    {
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

private:

    ShareableAudioFrameData mData;
    unsigned int mSize;

};

template <typename CodecOrFormat, unsigned int audioSampleRate, enum AudioChannels>
class AudioFrame : public Frame
{
};

}

#endif // FRAME_HPP_INCLUDED

/*
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef V4L2GRABBER_HPP_INCLUDED
#define V4L2GRABBER_HPP_INCLUDED

#ifdef LINUX

#include <iostream>
#include <fstream>
#include <vector>
#include "AllVideoCodecsAndFormats.hpp"
#include "Common.hpp"
#include "EventsManager.hpp"
#include "VideoFrameHolder.hpp"

extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <event.h>
}

#define CLEAR(x) memset(&(x), 0, sizeof(x))

namespace laav
{

enum V4LDeviceError
{
    V4L_NO_ERROR,
    VIDIOC_DQBUF_ERROR,
    VIDIOC_QBUF_ERROR,
    VIDIOC_STREAMON_ERROR,
    VIDIOC_REQBUFS_ERROR,
    INSUFFICIENT_BUFFER_MEMORY_ERROR,
    VIDIOC_QUERYBUF_ERROR,
    MMAP_ERROR,
    V4L_MMAP_UNSUPPORTED_ERROR,
    VIDIOC_S_FMT_ERROR,
    VIDIOC_S_PARM_ERROR,
    NO_V4L_DEVICE_ERROR,
    VIDIOC_QUERYCAP_ERROR,
    NO_V4L_CAPTURE_DEVICE_ERROR,
    NO_DEVICE_ERROR,
    NO_RESOURCE_ERROR,
    CONFIG_IMG_TO_CAPTURE_ERROR,
    VIDIOC_STREAMOFF_ERROR
};

struct V4LUtils
{
    template <typename T>
    static const __u32 translatePixelFormat();

    static int xioctl(int fh, unsigned long int request, void *arg)
    {
        int r;
        do
        {
            r = ioctl(fh, request, arg);
        }
        while (-1 == r && EINTR == errno);

        return r;
    }
};

template <>
const __u32 V4LUtils::translatePixelFormat<YUYV422_PACKED>()
{
    return V4L2_PIX_FMT_YUYV;
}
template <>
const __u32 V4LUtils::translatePixelFormat<MJPEG>()
{
    return V4L2_PIX_FMT_MJPEG;
}

template <typename CodecOrFormat, unsigned int width, unsigned int height>
class V4L2Grabber : public EventsProducer
{

public:

    V4L2Grabber(SharedEventsCatcher eventsCatcher, const std::string& devName, unsigned int fps = 0):
        EventsProducer::EventsProducer(eventsCatcher),
        mFps(fps),
        mV4LError(V4L_NO_ERROR),
        mStatus(DEV_INITIALIZING),
        mErrno(0),
        mUnrecoverableState(false),
        mLatency(0),
        mGrabbingStartTime(0),
        mDevName(devName),
        mFd(-1),
        mNewVideoFrameAvailable(false),
        mStreamOn(false)
    {
        mEncodedFramesBuffer.resize(10);
        unsigned int i;
        for (i = 0; i < mEncodedFramesBuffer.size(); i++ )
        {
            mEncodedFramesBuffer[i] = new unsigned char[500000];
        }

        if (openDevice())
        {
            configureDevice();
            if (!mUnrecoverableState)
            {
                configureImageToCapture();
                if (mFd == -1)
                    return;
            }
            if (!mUnrecoverableState)
                initMmap();
            if (!mUnrecoverableState)
                startCapturing();
        }
    }

    ~V4L2Grabber()
    {
        stopCapture();
        closeDeviceAndReleaseMmap();
        unsigned int i;

        for (i = 0; i < mEncodedFramesBuffer.size(); i++ )
        {
            delete[] mEncodedFramesBuffer[i];
        }
    }

    VideoFrameHolder<CodecOrFormat, width, height>&
    operator >>
    (VideoFrameHolder<CodecOrFormat, width, height>& videoFrameHolder)
    {
        try
        {
            videoFrameHolder.hold(grabNextFrame());
            videoFrameHolder.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            videoFrameHolder.mMediaStatusInPipe = mediaException.cause();
        }
        return videoFrameHolder;
    }

    FFMPEGMJPEGDecoder<width, height>&
    operator >>
    (FFMPEGMJPEGDecoder<width, height>& videoDecoder)
    {
        try
        {
            videoDecoder.decode(grabNextFrame());
            videoDecoder.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            videoDecoder.mMediaStatusInPipe = mediaException.cause();
        }
        return videoDecoder;
    }

    template <typename EncodedVideoFrameCodec>
    VideoEncoder<CodecOrFormat, EncodedVideoFrameCodec, width, height>&
    operator >>
    (VideoEncoder<CodecOrFormat, EncodedVideoFrameCodec, width, height>& videoEncoder)
    {
        try
        {
            videoEncoder.encode(grabNextFrame());
        }
        catch (const MediaException& mediaException)
        {
            videoEncoder.mMediaStatusInPipe = mediaException.cause();
        }
        return videoEncoder;
    }

    template <typename ConvertedVideoFrameFormat, unsigned int outputWidth, unsigned int outputHeigth>
    FFMPEGVideoConverter<CodecOrFormat, width, height,
                         ConvertedVideoFrameFormat, outputWidth, outputHeigth>&
     operator >>
     (FFMPEGVideoConverter<CodecOrFormat, width, height,
                           ConvertedVideoFrameFormat, outputWidth, outputHeigth>& videoConverter)
    {
        try
        {
            videoConverter.convert(grabNextFrame());
            videoConverter.mMediaStatusInPipe = MEDIA_READY;
        }
        catch (const MediaException& mediaException)
        {
            videoConverter.mMediaStatusInPipe = mediaException.cause();
        }
        return videoConverter;
    }

    template <typename Container>
    HTTPVideoStreamer<Container, CodecOrFormat, width, height>&
    operator >>
    (HTTPVideoStreamer<Container, CodecOrFormat, width, height>& httpVideoStreamer)
    {
        try
        {
            httpVideoStreamer.takeStreamableFrame(grabNextFrame());
            httpVideoStreamer.sendMuxedData();
        }
        catch (const MediaException& mediaException)
        {
            // Do nothing, because the streamer is at the end of the pipe
        }
        return httpVideoStreamer;
    }

    template <typename Container, typename AudioCodec,
              unsigned int audioSampleRate, enum AudioChannels audioChannels>
    HTTPAudioVideoStreamer<Container,
                           CodecOrFormat, width, height,
                           AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (HTTPAudioVideoStreamer<Container,
                            CodecOrFormat, width, height,
                            AudioCodec, audioSampleRate, audioChannels>& httpAudioVideoStreamer)
    {
        try
        {
            httpAudioVideoStreamer.takeStreamableFrame(grabNextFrame());
            httpAudioVideoStreamer.streamMuxedData();
        }
        catch (const MediaException& mediaException)
        {
            // Do nothing, because the streamer is at the end of the pipe
        }
        return httpAudioVideoStreamer;
    }

    template <typename Container>
    FFMPEGVideoMuxer<Container, CodecOrFormat, width, height>&
    operator >>
    (FFMPEGVideoMuxer<Container, CodecOrFormat, width, height>& videoMuxer)
    {
        try
        {
            videoMuxer.takeMuxableFrame(grabNextFrame());
        }
        catch (const MediaException& mediaException)
        {
            videoMuxer.mMediaStatusInPipe = mediaException.cause();
        }
        return videoMuxer;
    }

    template <typename Container,
              typename AudioCodec, unsigned int audioSampleRate, enum AudioChannels audioChannels>
    FFMPEGAudioVideoMuxer<Container,
                          CodecOrFormat, width, height,
                          AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioVideoMuxer<Container,
                           CodecOrFormat, width, height,
                           AudioCodec, audioSampleRate, audioChannels>& audioVideoMuxer)
    {
        try
        {
            audioVideoMuxer.takeMuxableFrame(grabNextFrame());
        }
        catch (const MediaException& mediaException)
        {
            // Do nothing, because the streamer is at the end of the pipe
            audioVideoMuxer.mMediaStatusInPipe = mediaException.cause();
        }
        return audioVideoMuxer;
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    VideoFrame<CodecOrFormat, width, height>& grabNextFrame()
    {
        // TODO: it should throw another exception, but ok...
        if (mUnrecoverableState)
            throw MediaException(MEDIA_NO_DATA);

        if (mFd == -1)
        {
            if (openDevice())
            {
                configureDevice();
                if (!mUnrecoverableState)
                {
                    configureImageToCapture();
                    if (mFd == -1)
                        throw MediaException(MEDIA_NO_DATA);
                }
                if (!mUnrecoverableState)
                    initMmap();
                if (!mUnrecoverableState)
                    startCapturing();
            }
            else
                throw MediaException(MEDIA_NO_DATA);
        }

        if (!mNewVideoFrameAvailable)
        {
            if (!thereAreEventsPendingOn(mFd))
            {
                observeEventsOn(mFd);
            }
            throw MediaException(MEDIA_NO_DATA);
        }
        else
        {
            fillVideoFrameAndAskDriverToBufferData(mGrabbedVideoFrame);
            setPts(mGrabbedVideoFrame);
            mNewVideoFrameAvailable = false;
            return mGrabbedVideoFrame;
        }
    }

    enum V4LDeviceError getV4LError() const
    {
        return mV4LError;
    }

    int getErrno() const
    {
        return mErrno;
    }

    std::string getV4LErrorString() const
    {
        std::string ret = "";
        switch(mV4LError)
        {
        case V4L_NO_ERROR:
            ret = "V4L_NO_ERROR";
            break;
        case VIDIOC_DQBUF_ERROR:
            ret = "VIDIOC_DQBUF_ERROR";
            break;
        case VIDIOC_QBUF_ERROR:
            ret = "VIDIOC_QBUF_ERROR";
            break;
        case VIDIOC_STREAMON_ERROR:
            ret = "VIDIOC_STREAMON_ERROR";
            break;
        case VIDIOC_REQBUFS_ERROR:
            ret = "VIDIOC_REQBUFS_ERROR";
            break;
        case INSUFFICIENT_BUFFER_MEMORY_ERROR:
            ret = "INSUFFICIENT_BUFFER_MEMORY_ERROR";
            break;
        case VIDIOC_QUERYBUF_ERROR:
            ret = "VIDIOC_QUERYBUF_ERROR";
            break;
        case MMAP_ERROR:
            ret = "MMAP_ERROR";
            break;
        case V4L_MMAP_UNSUPPORTED_ERROR:
            ret = "V4L_MMAP_UNSUPPORTED_ERROR";
            break;
        case VIDIOC_S_FMT_ERROR:
            ret = "VIDIOC_S_FMT_ERROR";
            break;
        case VIDIOC_S_PARM_ERROR:
            ret = "VIDIOC_S_PARM_ERROR";
            break;
        case NO_V4L_DEVICE_ERROR:
            ret = "NO_V4L_DEVICE_ERROR";
            break;
        case VIDIOC_QUERYCAP_ERROR:
            ret = "VIDIOC_QUERYCAP_ERROR";
            break;
        case NO_V4L_CAPTURE_DEVICE_ERROR:
            ret = "NO_V4L_CAPTURE_DEVICE_ERROR";
            break;
        case NO_DEVICE_ERROR:
            ret = "NO_DEVICE_ERROR";
            break;
        case NO_RESOURCE_ERROR:
            ret = "NO_RESOURCE_ERROR";
            break;
        case CONFIG_IMG_TO_CAPTURE_ERROR:
            ret = "CONFIG_IMG_TO_CAPTURE_ERROR";
            break;
        case VIDIOC_STREAMOFF_ERROR:
            ret = "VIDIOC_STREAMOFF_ERROR";
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

    /*
        TODO: implement for v4l2_plane?
        void fillVideoFrameAndAskDriverToBufferData(Planar3RawVideoFrameBase& videoFrame)
        {
        }
    */

    void setPts(VideoFrameBase<width, height>& videoFrame)
    {
        videoFrame.setTimestampsToNow();
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    void fillVideoFrameAndAskDriverToBufferData(EncodedVideoFrame& videoFrame)
    {
        struct v4l2_buffer buf;
        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (-1 == V4LUtils::xioctl(mFd, VIDIOC_DQBUF, &buf))
        {
            switch (errno)
            {
                case EAGAIN:
                    // Should not happen (because already catched
                    // by the events manager)... anyway...
                    throw MediaException(MEDIA_NO_DATA);
                case EIO:
                // Could ignore EIO, see spec
                // fall through
                default:
                    mStatus = DEV_DISCONNECTED;
                    mV4LError = VIDIOC_DQBUF_ERROR;
                    mErrno = errno;
                    stopCapture();
                    closeDeviceAndReleaseMmap();
                    return;
            }
        }

        if (mLatency == 0)
        {
            mLatency = av_gettime_relative() - mGrabbingStartTime;
            AVRational timeBase = AV_TIME_BASE_Q;
            timeBase = {1, 1000000000};
            struct timespec monotimeNow;
            clock_gettime(CLOCK_MONOTONIC, &monotimeNow);

            mLatency = (monotimeNow.tv_sec * 1000000000 + monotimeNow.tv_nsec) -
            (buf.timestamp.tv_sec * 1000000000 + buf.timestamp.tv_usec * 1000);
        }

        mV4LError = V4L_NO_ERROR;
        mErrno = 0;
        mStatus = DEV_CAN_GRAB;

        memcpy(mEncodedFramesBuffer[mEncodedFramesBufferOffset],
               mBuffersFormVideoFrame[buf.index].get(), buf.bytesused);

        auto freeNothing = [](unsigned char* buffer){};
        ShareableVideoFrameData shData =
        ShareableAudioFrameData(mEncodedFramesBuffer[mEncodedFramesBufferOffset], freeNothing);

        mEncodedFramesBufferOffset =
        (mEncodedFramesBufferOffset + 1) % mEncodedFramesBuffer.size();

        videoFrame.assignDataSharedPtr(shData);
        videoFrame.setSize(buf.bytesused);
        if (!thereAreEventsPendingOn(mFd))
        {
            observeEventsOn(mFd);
        }

        if (-1 == V4LUtils::xioctl(mFd, VIDIOC_QBUF, &buf))
        {
            mV4LError = VIDIOC_QBUF_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            return;
        }
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    void fillVideoFrameAndAskDriverToBufferData(PackedRawVideoFrame& videoFrame)
    {
        struct v4l2_buffer buf;
        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (-1 == V4LUtils::xioctl(mFd, VIDIOC_DQBUF, &buf))
        {
            switch (errno)
            {
            case EAGAIN:
                // Should not happen (because
                // already caught by the events manager)... anyway...
                throw MediaException(MEDIA_NO_DATA);
            case EIO:
            // Could ignore EIO, see spec
            // fall through
            default:
                mV4LError = VIDIOC_DQBUF_ERROR;
                mErrno = errno;
                mStatus = DEV_DISCONNECTED;
                stopCapture();
                closeDeviceAndReleaseMmap();
                return;
            }
        }

        if (mLatency == 0)
        {
            mLatency = av_gettime_relative() - mGrabbingStartTime;
            AVRational timeBase = AV_TIME_BASE_Q;
            // TODO: improve
            timeBase = {1, 1000000000};
            struct timespec monotimeNow;
            clock_gettime(CLOCK_MONOTONIC, &monotimeNow);

            mLatency = (monotimeNow.tv_sec * 1000000000 + monotimeNow.tv_nsec) -
            (buf.timestamp.tv_sec * 1000000000 + buf.timestamp.tv_usec * 1000);
        }

        mV4LError = V4L_NO_ERROR;
        mErrno = 0;
        mStatus = DEV_CAN_GRAB;
        videoFrame.assignDataSharedPtr(mBuffersFormVideoFrame[buf.index]);
        videoFrame.setSize(buf.bytesused);
        if (!thereAreEventsPendingOn(mFd))
        {
            observeEventsOn(mFd);
        }

        if (-1 == V4LUtils::xioctl(mFd, VIDIOC_QBUF, &buf))
        {
            mV4LError = VIDIOC_QBUF_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            return;
        }
    }

    bool startCapturing()
    {
        unsigned int i;
        enum v4l2_buf_type type;
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (mGrabbingStartTime == 0)
            mGrabbingStartTime = av_gettime_relative();

        for (i = 0; i < mNumOfBuffers; ++i)
        {
            struct v4l2_buffer buf;

            CLEAR(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (-1 == V4LUtils::xioctl(mFd, VIDIOC_QBUF, &buf))
            {
                mV4LError = VIDIOC_QBUF_ERROR;
                mErrno = errno;
                mUnrecoverableState = true;
                return false;
            }
        }
        if (-1 == V4LUtils::xioctl(mFd, VIDIOC_STREAMON, &type))
        {
            mV4LError = VIDIOC_STREAMON_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            return false;
        }

        mStreamOn = true;
        mV4LError = V4L_NO_ERROR;
        mErrno = 0;
        mStatus = DEV_CAN_GRAB;
        return true;
    }

    bool initMmap()
    {
        struct v4l2_requestbuffers req;
        CLEAR(req);
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (-1 == V4LUtils::xioctl(mFd, VIDIOC_REQBUFS, &req))
        {
            if (EINVAL == errno)
            {
                mV4LError = V4L_MMAP_UNSUPPORTED_ERROR;
                mErrno = errno;
                mUnrecoverableState = true;
                return false;
            }
            else
            {
                mV4LError = VIDIOC_REQBUFS_ERROR;
                mErrno = errno;
                mUnrecoverableState = true;
                return false;
            }
        }

        if (req.count < 2)
        {
            mV4LError = INSUFFICIENT_BUFFER_MEMORY_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            return false;
        }

        mBuffers.resize(req.count);

        for (mNumOfBuffers = 0; mNumOfBuffers < req.count; ++mNumOfBuffers)
        {
            struct v4l2_buffer buf;
            CLEAR(buf);
            buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory      = V4L2_MEMORY_MMAP;
            buf.index       = mNumOfBuffers;

            if (-1 == V4LUtils::xioctl(mFd, VIDIOC_QUERYBUF, &buf))
            {
                mV4LError = VIDIOC_QUERYBUF_ERROR;
                mErrno = errno;
                mUnrecoverableState = true;
                return false;
            }
            mBuffers[mNumOfBuffers].length = buf.length;
            mBuffers[mNumOfBuffers].start =
                mmap(NULL, // start anywhere
                     buf.length,
                     PROT_READ | PROT_WRITE, // required
                     MAP_SHARED, // recommended
                     mFd, buf.m.offset);

            if (MAP_FAILED == mBuffers[mNumOfBuffers].start)
            {
                mV4LError = MMAP_ERROR;
                mErrno = errno;
                mUnrecoverableState = true;
                return false;
            }

            unsigned char* videoFrameDataPtr = (unsigned char* )mBuffers[mNumOfBuffers].start;
            int mmapLength = buf.length;
            auto releaseMmap = [mmapLength] (unsigned char* videoFrameDataPtr_)
            {
                munmap((void* )videoFrameDataPtr_, mmapLength);
            };
            mBuffersFormVideoFrame.push_back(ShareableVideoFrameData(videoFrameDataPtr, releaseMmap));
        }
        return true;
    }

    bool configureImageToCapture()
    {
        struct v4l2_format fmt;
        CLEAR(fmt);
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width       = width;
        fmt.fmt.pix.height      = height;
        fmt.fmt.pix.pixelformat = V4LUtils::translatePixelFormat<CodecOrFormat>();
        fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

        if (-1 == V4LUtils::xioctl(mFd, VIDIOC_S_FMT, &fmt))
        {
            mV4LError = VIDIOC_S_FMT_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            return false;
        }

        CLEAR(fmt);
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == V4LUtils::xioctl(mFd, VIDIOC_G_FMT, &fmt))
        {
            mV4LError = VIDIOC_S_FMT_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            return false;
        }

        if (fmt.fmt.pix.width != width || fmt.fmt.pix.height != height)
        {
            mV4LError = CONFIG_IMG_TO_CAPTURE_ERROR;
            mErrno = 0;
            closeDeviceAndReleaseMmap();
            return false;
        }

        if (mFps != 0)
        {
            struct v4l2_streamparm param;
            CLEAR(param);
            param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            param.parm.capture.timeperframe.numerator = 1;
            param.parm.capture.timeperframe.denominator = mFps;
            if (-1 == V4LUtils::xioctl(mFd, VIDIOC_S_PARM, &param))
            {
                mV4LError = VIDIOC_S_PARM_ERROR;
                mErrno = errno;
                mUnrecoverableState = true;
                return false;
            }
        }
        return true;
    }

    bool configureDevice()
    {
        struct v4l2_capability cap;
        struct v4l2_cropcap cropcap;
        struct v4l2_crop crop;

        if (-1 == V4LUtils::xioctl(mFd, VIDIOC_QUERYCAP, &cap))
        {
            if (EINVAL == errno)
            {
                mUnrecoverableState = true;
                mV4LError = NO_V4L_DEVICE_ERROR;
                mErrno = errno;
                return false;
            }
            else
            {
                mV4LError = VIDIOC_QUERYCAP_ERROR;
                mErrno = errno;
                mUnrecoverableState = true;
                return false;
            }
        }

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        {
            mV4LError = NO_V4L_CAPTURE_DEVICE_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            return false;
        }

        CLEAR(cropcap);
        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (0 == V4LUtils::xioctl(mFd, VIDIOC_CROPCAP, &cropcap))
        {
            crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            crop.c = cropcap.defrect; // reset to default

            if (-1 == V4LUtils::xioctl(mFd, VIDIOC_S_CROP, &crop))
            {
                switch (errno)
                {
                case EINVAL:
                    // Cropping not supported.
                    break;
                default:
                    // Errors ignored.
                    break;
                }
            }
        }
        else
        {
            // Errors ignored.
        }
        mStatus = DEV_CONFIGURED;
        return true;
    }

    bool openDevice()
    {
        struct stat st;

        if (-1 == stat(mDevName.c_str(), &st))
        {
            mV4LError = NO_RESOURCE_ERROR;
            mErrno = errno;
            return false;
        }

        if (!S_ISCHR(st.st_mode))
        {
            mV4LError = NO_DEVICE_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            return false;
        }

        mFd = open(mDevName.c_str(), O_RDWR | O_NONBLOCK, 0);
        makePollable(mFd);

        if (-1 == mFd)
        {
            mStatus = OPEN_DEV_ERROR;
            mErrno = errno;
            return false;
        }
        mV4LError = V4L_NO_ERROR;
        mErrno = 0;
        return true;
    }

    void stopCapture()
    {
        if (mUnrecoverableState)
            return;
        enum v4l2_buf_type type;
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(mStreamOn)
        {
            if (-1 == V4LUtils::xioctl(mFd, VIDIOC_STREAMOFF, &type))
            {
                mV4LError = VIDIOC_STREAMOFF_ERROR;
                mErrno = errno;
            }
        }
        mStreamOn = false;
    }

    void closeDeviceAndReleaseMmap()
    {
        if (mFd == -1)
            return;
        if (-1 == close(mFd))
        {
            mStatus = CLOSE_DEV_ERROR;
            mErrno = errno;
        }
        mFd = -1;
        while (mBuffersFormVideoFrame.size() > 0)
            mBuffersFormVideoFrame.pop_back();
    }

    void eventCallBack(int fd, enum EventType eventType)
    {
        mNewVideoFrameAvailable = true;
        dontObserveEventsOn(mFd);
    }

    // TODO use a less generic name
    struct Buffer
    {
        void* start;
        size_t  length;
    };

    unsigned int mFps;
    enum V4LDeviceError mV4LError;
    enum DeviceStatus mStatus;
    int mErrno;
    bool mUnrecoverableState;
    int64_t mLatency;
    int64_t mGrabbingStartTime;
    std::vector<ShareableVideoFrameData> mBuffersFormVideoFrame;
    std::vector<Buffer> mBuffers;
    unsigned int mNumOfBuffers;
    std::string mDevName;
    int mFd;
    bool mNewVideoFrameAvailable;
    VideoFrame<CodecOrFormat, width, height> mGrabbedVideoFrame;
    std::vector<unsigned char* > mEncodedFramesBuffer;
    unsigned int mEncodedFramesBufferOffset;
    bool mStreamOn;

};

}

#endif // LINUX

#endif // V4L2GRABBER_HPP_INCLUDED

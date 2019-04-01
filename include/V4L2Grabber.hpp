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

#ifndef V4L2GRABBER_HPP_INCLUDED
#define V4L2GRABBER_HPP_INCLUDED

#ifdef LINUX

#include <iostream>
#include <fstream>
#include <vector>
#include "Medium.hpp"
#include "AllVideoCodecsAndFormats.hpp"
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
    VIDIOC_STREAMOFF_ERROR,
    V4L_EAGAIN
};

struct V4LUtils
{
    template <typename T>
    static __u32 translatePixelFormat();

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
__u32 V4LUtils::translatePixelFormat<YUYV422_PACKED>()
{
    return V4L2_PIX_FMT_YUYV;
}
template <>
__u32 V4LUtils::translatePixelFormat<MJPEG>()
{
    return V4L2_PIX_FMT_MJPEG;
}

template <typename CodecOrFormat, typename width, typename height>
class V4L2Grabber : private EventsProducer, public Medium
{

public:

    V4L2Grabber(SharedEventsCatcher eventsCatcher, const std::string& devName, 
                unsigned int fps = 0, const std::string& id = ""):
        EventsProducer::EventsProducer(eventsCatcher),
        Medium(id),
        mFps(fps),
        mV4LError(V4L_NO_ERROR),
        mDeviceStatus(DEV_INITIALIZING),
        mErrno(0),
        mUnrecoverableState(false),
        mLatency(0),
        mGrabbingStartTime(0),
        mDevName(devName),
        mFd(-1),
        mNewVideoFrameAvailable(false),
        mEncodedFramesBufferOffset(0),        
        mStreamOn(false)
    {
        mEncodedFramesBuffer.resize(10);
        unsigned int i;
        for (i = 0; i < mEncodedFramesBuffer.size(); i++ )
        {
            mEncodedFramesBuffer[i] = new unsigned char[500000];
        }

        try
        {
            mInputStatus  = NOT_READY;
            mOutputStatus  = NOT_READY;                
            openDevice();
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
            {
                startCapturing();   
                mInputStatus  = READY;
            }
        }
        catch (V4LDeviceError err) 
        { }
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
        Medium& m = videoFrameHolder;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);
        setStatusInPipe(m, READY); 
        
        try
        {
            videoFrameHolder.hold(grabNextFrame());          
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoFrameHolder;
    }

    FFMPEGMJPEGDecoder<width, height>&
    operator >>
    (FFMPEGMJPEGDecoder<width, height>& videoDecoder)
    {
        Medium& m = videoDecoder;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);
        setStatusInPipe(m, READY);         
        
        try
        {
            videoDecoder.decode(grabNextFrame());       
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoDecoder;
    }

    template <typename EncodedVideoFrameCodec>
    VideoEncoder<CodecOrFormat, EncodedVideoFrameCodec, width, height>&
    operator >>
    (VideoEncoder<CodecOrFormat, EncodedVideoFrameCodec, width, height>& videoEncoder)
    {
        Medium& m = videoEncoder;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);
        setStatusInPipe(m, READY); 
        
        try
        {
            videoEncoder.encode(grabNextFrame());          
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoEncoder;
    }

    template <typename ConvertedVideoFrameFormat, typename outputWidth, typename outputHeigth>
    FFMPEGVideoConverter<CodecOrFormat, width, height,
                         ConvertedVideoFrameFormat, outputWidth, outputHeigth>&
     operator >>
     (FFMPEGVideoConverter<CodecOrFormat, width, height,
                           ConvertedVideoFrameFormat, outputWidth, outputHeigth>& videoConverter)
    {
        Medium& m = videoConverter;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);
        setStatusInPipe(m, READY);        
        
        try
        {
            videoConverter.convert(grabNextFrame());            
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        
        return videoConverter;
    }

    template <typename Container>
    HTTPVideoStreamer<Container, CodecOrFormat, width, height>&
    operator >>
    (HTTPVideoStreamer<Container, CodecOrFormat, width, height>& httpVideoStreamer)
    {
        Medium& m = httpVideoStreamer;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);         
        setStatusInPipe(m, READY); 
        
        try
        {
            httpVideoStreamer.takeStreamableFrame(grabNextFrame());
            httpVideoStreamer.streamMuxedData();         
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return httpVideoStreamer;
    }

    template <typename Container, typename AudioCodec,
              typename audioSampleRate, typename audioChannels>
    HTTPAudioVideoStreamer<Container,
                           CodecOrFormat, width, height,
                           AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (HTTPAudioVideoStreamer<Container,
                            CodecOrFormat, width, height,
                            AudioCodec, audioSampleRate, audioChannels>& httpAudioVideoStreamer)
    {
        Medium& m = httpAudioVideoStreamer;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);  
        setStatusInPipe(m, READY); 
        
        try
        {
            httpAudioVideoStreamer.takeStreamableFrame(grabNextFrame());
            httpAudioVideoStreamer.streamMuxedData();           
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return httpAudioVideoStreamer;
    }

    template <typename Container>
    UDPVideoStreamer<Container, CodecOrFormat, width, height>&
    operator >>
    (UDPVideoStreamer<Container, CodecOrFormat, width, height>& udpVideoStreamer)
    {
        Medium& m = udpVideoStreamer;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);        
        setStatusInPipe(m, READY); 
        
        try
        {
            udpVideoStreamer.takeStreamableFrame(grabNextFrame());
            udpVideoStreamer.streamMuxedData();            
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return udpVideoStreamer;
    }

    template <typename Container, typename AudioCodec,
              typename audioSampleRate, typename audioChannels>
    UDPAudioVideoStreamer<Container, CodecOrFormat, width, height,
                          AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (UDPAudioVideoStreamer<Container, CodecOrFormat, width, height,
                           AudioCodec, audioSampleRate, audioChannels>& udpAudioVideoStreamer)
    {
        Medium& m = udpAudioVideoStreamer;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);         
        setStatusInPipe(m, READY); 
        
        try
        {
            udpAudioVideoStreamer.takeStreamableFrame(grabNextFrame());
            udpAudioVideoStreamer.streamMuxedData();           
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return udpAudioVideoStreamer;
    }    
    
    template <typename Container>
    FFMPEGVideoMuxer<Container, CodecOrFormat, width, height>&
    operator >>
    (FFMPEGVideoMuxer<Container, CodecOrFormat, width, height>& videoMuxer)
    {
        Medium& m = videoMuxer;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId); 
        setStatusInPipe(m, READY); 
        
        try
        {
            videoMuxer.mux(grabNextFrame());         
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return videoMuxer;
    }

    template <typename Container,
              typename AudioCodec, typename audioSampleRate, typename audioChannels>
    FFMPEGAudioVideoMuxer<Container,
                          CodecOrFormat, width, height,
                          AudioCodec, audioSampleRate, audioChannels>&
    operator >>
    (FFMPEGAudioVideoMuxer<Container,
                           CodecOrFormat, width, height,
                           AudioCodec, audioSampleRate, audioChannels>& audioVideoMuxer)
    {
        Medium& m = audioVideoMuxer;
        setDistanceFromInputVideoFrameFactory(m, 1);        
        setInputVideoFrameFactoryId(m, mId);
        setStatusInPipe(m, READY);
        
        try
        {
            audioVideoMuxer.mux(grabNextFrame());            
        }
        catch (const MediumException& mediumException)
        {
            setStatusInPipe(m, NOT_READY);            
        }
        return audioVideoMuxer;
    }

    /*!
     *  \exception MediumException
     */
    VideoFrame<CodecOrFormat, width, height>& grabNextFrame()
    {        
        Medium::mOutputStatus  = NOT_READY;    
        mGrabbedVideoFrame.mFactoryId = Medium::mId;            
        Medium::mDistanceFromInputVideoFrameFactory = 1;        
        Medium::mInputVideoFrameFactoryId = "V4LDEV";
        
        if (mUnrecoverableState)
            throw MediumException(INPUT_NOT_READY);

        if (Medium::mInputStatus == PAUSED)
            throw MediumException(MEDIUM_PAUSED);
        
        if (mFd == -1)
        {
            try
            {
                mInputStatus  = NOT_READY;          
                openDevice();
                configureDevice();
                if (!mUnrecoverableState)
                {
                    configureImageToCapture();
                    if (mFd == -1)
                    {
                        throw MediumException(INPUT_NOT_READY);
                    }
                }
                if (!mUnrecoverableState)
                    initMmap();
                if (!mUnrecoverableState)
                {
                    startCapturing();
                    mInputStatus  = READY;
                }
            }
            catch (V4LDeviceError err)
            {
                throw MediumException(INPUT_NOT_READY);
            }
        }

        if (!mNewVideoFrameAvailable)
        {
            if (!thereAreEventsPendingOn(mFd))
            {
                observeEventsOn(mFd);
            }
            mOutputStatus = NOT_READY;
            throw MediumException(OUTPUT_NOT_READY);
        }
        else
        {
            try
            {
                fillVideoFrameAndAskDriverToBufferData(mGrabbedVideoFrame);
            }            
            catch (V4LDeviceError err)
            {
                throw MediumException(INPUT_NOT_READY);
            }
            setPts(mGrabbedVideoFrame);
            mNewVideoFrameAvailable = false;
            mInputStatus  = READY;
            mOutputStatus = READY;
            if (std::is_same<CodecOrFormat, MJPEG>())
                mGrabbedVideoFrame.mLibAVFlags = AV_PKT_FLAG_KEY;
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

        if (mV4LError == V4L_NO_ERROR)
            ret = "V4L_NO_ERROR";
        if (mV4LError == VIDIOC_DQBUF_ERROR)
            ret = "VIDIOC_DQBUF_ERROR";
        if (mV4LError == VIDIOC_QBUF_ERROR)
            ret = "VIDIOC_QBUF_ERROR";
        if (mV4LError == VIDIOC_STREAMON_ERROR)
            ret = "VIDIOC_STREAMON_ERROR";
        if (mV4LError == VIDIOC_REQBUFS_ERROR)
            ret = "VIDIOC_REQBUFS_ERROR";
        if (mV4LError == INSUFFICIENT_BUFFER_MEMORY_ERROR)
            ret = "INSUFFICIENT_BUFFER_MEMORY_ERROR";
        if (mV4LError == VIDIOC_QUERYBUF_ERROR)
            ret = "VIDIOC_QUERYBUF_ERROR";
        if (mV4LError == MMAP_ERROR)
            ret = "MMAP_ERROR";
        if (mV4LError == V4L_MMAP_UNSUPPORTED_ERROR)
            ret = "V4L_MMAP_UNSUPPORTED_ERROR";
        if (mV4LError == VIDIOC_S_FMT_ERROR)
            ret = "VIDIOC_S_FMT_ERROR";
        if (mV4LError == VIDIOC_S_PARM_ERROR)
            ret = "VIDIOC_S_PARM_ERROR";
        if (mV4LError == NO_V4L_DEVICE_ERROR)
            ret = "NO_V4L_DEVICE_ERROR";
        if (mV4LError == VIDIOC_QUERYCAP_ERROR)
            ret = "VIDIOC_QUERYCAP_ERROR";
        if (mV4LError == NO_V4L_CAPTURE_DEVICE_ERROR)
            ret = "NO_V4L_CAPTURE_DEVICE_ERROR";
        if (mV4LError == NO_DEVICE_ERROR)
            ret = "NO_DEVICE_ERROR";
        if (mV4LError == NO_RESOURCE_ERROR)
            ret = "NO_RESOURCE_ERROR";
        if (mV4LError == CONFIG_IMG_TO_CAPTURE_ERROR)
            ret = "CONFIG_IMG_TO_CAPTURE_ERROR";
        if (mV4LError == VIDIOC_STREAMOFF_ERROR)
            ret = "VIDIOC_STREAMOFF_ERROR";
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
     * \exception V4LDeviceError
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
                    // Should not happen (because already caught
                    // by the events manager)... anyway...
                    mV4LError = V4L_EAGAIN;
                    mErrno = errno;                    
                    throw mV4LError;
                case EIO:
                // Could ignore EIO, see spec
                // fall through
                default:
                    mDeviceStatus = DEV_DISCONNECTED;
                    mV4LError = VIDIOC_DQBUF_ERROR;
                    mErrno = errno;
                    stopCapture();
                    closeDeviceAndReleaseMmap();
                    throw mV4LError;
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
        mDeviceStatus = DEV_CAN_GRAB;

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
            throw mV4LError;
        }
    }

    /*!
     * \exception V4LDeviceError
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
                mV4LError = V4L_EAGAIN;
                mErrno = errno;
                throw mV4LError;
            case EIO:
            // Could ignore EIO, see spec
            // fall through
            default:
                mV4LError = VIDIOC_DQBUF_ERROR;
                mErrno = errno;
                mDeviceStatus = DEV_DISCONNECTED;
                stopCapture();
                closeDeviceAndReleaseMmap();
                throw mV4LError;
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
        mDeviceStatus = DEV_CAN_GRAB;
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
            throw mV4LError;
        }
    }

    /*!
     * \exception V4LDeviceError
     */    
    void startCapturing()
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
                throw mV4LError;
            }
        }
        if (-1 == V4LUtils::xioctl(mFd, VIDIOC_STREAMON, &type))
        {
            mV4LError = VIDIOC_STREAMON_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            throw mV4LError;
        }
        mStreamOn = true;
        mV4LError = V4L_NO_ERROR;
        mErrno = 0;
        mDeviceStatus = DEV_CAN_GRAB;
    }

    /*!
     * \exception V4LDeviceError
     */    
    void initMmap()
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
                throw mV4LError;
            }
            else
            {
                mV4LError = VIDIOC_REQBUFS_ERROR;
                mErrno = errno;
                mUnrecoverableState = true;
                throw mV4LError;
            }
        }

        if (req.count < 2)
        {
            mV4LError = INSUFFICIENT_BUFFER_MEMORY_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            throw mV4LError;
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
                throw mV4LError;
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
                throw mV4LError;
            }

            unsigned char* videoFrameDataPtr = (unsigned char* )mBuffers[mNumOfBuffers].start;
            int mmapLength = buf.length;
            auto releaseMmap = [mmapLength] (unsigned char* videoFrameDataPtr_)
            {
                munmap((void* )videoFrameDataPtr_, mmapLength);
            };
            mBuffersFormVideoFrame.push_back(ShareableVideoFrameData(videoFrameDataPtr, releaseMmap));
        }
    }

    /*!
     * \exception V4LDeviceError
     */    
    void configureImageToCapture()
    {
        struct v4l2_format fmt;
        CLEAR(fmt);
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width       = width::value;
        fmt.fmt.pix.height      = height::value;
        fmt.fmt.pix.pixelformat = V4LUtils::translatePixelFormat<CodecOrFormat>();
        fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

        if (-1 == V4LUtils::xioctl(mFd, VIDIOC_S_FMT, &fmt))
        {
            mV4LError = VIDIOC_S_FMT_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            throw mV4LError;
        }

        CLEAR(fmt);
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == V4LUtils::xioctl(mFd, VIDIOC_G_FMT, &fmt))
        {
            mV4LError = VIDIOC_S_FMT_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            throw mV4LError;
        }

        if (fmt.fmt.pix.width != width::value || fmt.fmt.pix.height != height::value)
        {
            mV4LError = CONFIG_IMG_TO_CAPTURE_ERROR;
            mErrno = 0;
            closeDeviceAndReleaseMmap();
            throw mV4LError;
        }

        if (mFps > 0)
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
                throw mV4LError;
            }
        }
    }

    /*!
     * \exception V4LDeviceError
     */    
    void configureDevice()
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
                throw mV4LError;
            }
            else
            {
                mV4LError = VIDIOC_QUERYCAP_ERROR;
                mErrno = errno;
                mUnrecoverableState = true;
                throw mV4LError;
            }
        }

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        {
            mV4LError = NO_V4L_CAPTURE_DEVICE_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            throw mV4LError;
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
        mDeviceStatus = DEV_CONFIGURED;
    }

    /*!
     * \exception V4LDeviceError
     */    
    void openDevice()
    {
        struct stat st;

        if (-1 == stat(mDevName.c_str(), &st))
        {
            mV4LError = NO_RESOURCE_ERROR;
            mErrno = errno;
            throw mV4LError;
        }

        if (!S_ISCHR(st.st_mode))
        {
            mV4LError = NO_DEVICE_ERROR;
            mErrno = errno;
            mUnrecoverableState = true;
            throw mV4LError;
        }

        mFd = open(mDevName.c_str(), O_RDWR | O_NONBLOCK, 0);
        makePollable(mFd);

        if (-1 == mFd)
        {
            mDeviceStatus = OPEN_DEV_ERROR;
            mErrno = errno;
            throw mV4LError;
        }
        mV4LError = V4L_NO_ERROR;
        mErrno = 0;
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
            mDeviceStatus = CLOSE_DEV_ERROR;
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
    enum DeviceStatus mDeviceStatus;
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

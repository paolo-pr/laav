# Live Asynchronous Audio Video Library

## ABOUT

A header-only C++ library for capturing audio and video from multiple live sources (cameras and microphones) and

* encoding (video: H264, audio: AAC, MP2)
* transcoding (video MJPEG -> H264)
* recording to file (MPEGTS and/or MATROSKA audio and/or video)
* streaming (HTTP - MPEGTS/MATROSKA)
* image processing

The library currently runs on Linux, but a Windows port is planned (any contribution is welcome!)

## FEATURES

* All the audio/video tasks are made with strictly asynchronous multiple pipes, inside one main loop. This allows to create simple, short, easy to read and intuitive code in which all the pipes can be realized through overloaded operators, in the following form:

```
while (1)
{
    // Pipe
    videoGrabber >> videoConverter >> videoEncoder >> videoStreamer;
    
    // Audio-video events catcher
    eventsCatcher->catchNextEvent();
}
```

* Threads are not used at all and they are intentionally discouraged in order to avoid that they can be improperly used for decoupling tasks, instead of taking advantage from multi-core systems.
* All the audio/video modules are strongly templated and all their possible concatenations are checked at compile-time, so to avoid inconsistent pipes.
* All the pipes are safe at runtime. I.E: when a source is disconnected or temporarily unavailable, the main loop can continue without necessarily having to check errors (they can be checked, anyway, by polling the status of each node)
* The library is all RAII-designed (basically it safely wraps Libav, V4L and ALSA) and the user doesn't have to bother with pointers and memory.
* The public API is intended to be intuitive, with few self-explanatory functions (see the Doxy pages). In order to learn how to use the library, just read the provided examples.

## HOW TO USE IT

See the provided [EXAMPLES](https://github.com/paolo-pr/laav/tree/master/examples)
        
## COMPILING / RUNNING

Dependencies: FFMPEG >= 3.2.4 (tested with 3.2.4 version), libevent and pkg-config (optional; see the compile command below).
FFMPEG must be compiled with x264 support.

* Just include headers as shown in the examples, in YourProgram.cpp and execute:
```
g++ -Wall -std=c++11 -DLINUX -o YourProgram YourProgram.cpp `pkg-config --libs libavformat libavcodec libavutil libswresample libswscale libevent alsa`
```
* Execute ./CompileExamples for compiling the provided examples.
        

## TODO

* Error/exception handling is currently generic and will be developed/improved ASAP (according to my spare time, which is never enough...). 
* Add a RTSP/RTP streaming server.
* Windows port (basically, it will consist in creating Windows based classes corresponding to ALSAGrabber and V4L2Grabber, with the same API, and few other things: any contribution is welcome!).
* MPEGTS-MJPEG is currently NOT supported.
* Add a demuxer/player (Any contribution is welcome!).

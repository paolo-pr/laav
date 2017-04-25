# Live Asynchronous Audio Video Library

## ABOUT

A header-only C++ library for capturing audio and video from multiple live sources (cameras and microphones) and

* encoding (video: **H264**, audio: **AAC**, **MP2**)
* transcoding (video **MJPEG** -> **H264**)
* recording to file (**MPEGTS** and **MATROSKA** audio and/or video)
* streaming (**HTTP - MPEGTS / MATROSKA**)
* image processing

The library currently runs on **Linux** (ALSA and V4L2 devices), but a Windows port is planned (any contribution is welcome!);
it has been tested with **[VLC](http://www.videolan.org/)** and **[FFPLAY](https://ffmpeg.org/)** media players.

The library is useful for building **video surveillance** systems as well, consisting in media servers which stream and record at the same time and which can be controlled through HTTP commands (see **[THIS](https://github.com/paolo-pr/laav/blob/master/examples/VideoExample_2.cpp)** example)


## FEATURES

* All the audio/video tasks are made with **strictly asynchronous** multiple **pipes**, inside one main loop. This allows to create simple, short, easy to read and intuitive code in which all the pipes can be realized through overloaded operators, in the following form:

```
while (1)
{
    // Pipe
    videoGrabber >> videoConverter >> videoEncoder >> videoStreamer;
    
    // Audio-video events catcher
    eventsCatcher->catchNextEvent();
}
```

* **Threads are not used at all** and they are intentionally discouraged in order to avoid that they can be improperly used for decoupling tasks, without taking advantage from multi-core systems.
* All the audio/video modules (-> classes) make **extensive use of templates** and all their possible concatenations are checked at **compile-time**, so to avoid inconsistent pipes.
* All the pipes are **safe at runtime**. I.E: when a source is disconnected or temporarily unavailable, the main loop can continue without necessarily having to check errors (they can be checked, anyway, by polling the status of each node)
* The library is all RAII-designed (basically it safely wraps Libav, V4L and ALSA) and **the user doesn't have to bother with pointers and memory management**.
* The public API is intended to be intuitive, with few self-explanatory functions.

## COMPILING / RUNNING

Dependencies: **[FFMPEG](https://ffmpeg.org/)** >= 3.2.4 (tested with 3.2.4 version), **[libevent](http://libevent.org/)** and pkg-config (optional: see the compile command below).
**[FFMPEG](https://ffmpeg.org/)** must be compiled with **x264** support.

* Just include the library headers, as shown in the examples, in YourProgram.cpp and execute:
```
g++ -Wall -std=c++11 -DLINUX -o YourProgram YourProgram.cpp `pkg-config --libs libavformat libavcodec libavutil libswresample libswscale libevent alsa`
```
* Execute ./CompileExamples for compiling the provided examples.
* API documentation in html format can be created by executing, inside the doxy directory:
```
doxygen Doxyfile
```

## HOW TO USE IT

See the provided **[EXAMPLES](https://github.com/paolo-pr/laav/tree/master/examples)**

## TODO

* Error/exception handling must be less generic and will be improved ASAP (according to my spare time, which is never enough...). A doc/example about how to manage errors will be added as well.
* Add a RTSP/RTP streaming server.
* Windows port (basically, it will consist in creating Windows based classes corresponding to the ALSAGrabber and V4L2Grabber classes, with the same API, and few other things: any contribution is welcome!).
* MPEGTS-MJPEG is currently NOT supported.
* Add a demuxer/player (Any contribution is welcome!).

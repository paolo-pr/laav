# Live Asynchronous Audio Video Library

## ABOUT

A header-only **C++** library for capturing audio and video from multiple live sources (cameras and microphones) and

* Encoding (video: **H264**, audio: **AAC**, **MP2** and **OPUS**)
* Decoding (video: **MJPEG**) / transcoding (video: **MJPEG** -> **H264**)
* Recording to file (**MPEGTS** and **MATROSKA** containers, audio and/or video)
* Streaming (**HTTP** and **UDP** protocols for **MPEGTS** and **MATROSKA** containers)
* Basic image processing

The project is useful for building **video surveillance** systems as well, consisting in media servers which stream and record at the same time and which can be controlled through **MQTT** or **HTTP** commands (see **[THIS](https://github.com/paolo-pr/laav/blob/master/mqtt-avsystem/README.md)** and **[THIS](https://github.com/paolo-pr/laav/blob/master/examples/VideoExample_2.cpp)** examples).

It also provides an **[UDP audio (OPUS) + video (H264) streaming system](https://github.com/paolo-pr/laav/blob/master/optimum-latency/README.md)** with the **lowest possible latency**.

The project runs on **Linux** (**ALSA** and **V4L2** devices), but a Windows port is planned (any contribution is welcome!).

## FEATURES

* All the audio/video tasks are made with **strictly asynchronous** multiple **pipes**, inside one main loop. This allows to create **simple**, **short** (a complete live H264 grabber/streamer can be made with less than 40 lines of code, see **[THIS](https://github.com/paolo-pr/laav/blob/master/examples/VideoExample_1.cpp)** example), **easy to read** and **intuitive** code in which all the pipes can be realized through overloaded operators, in the following form:

```
while (1)
{
    // Pipe
    grabber >> converter >> encoder >> streamer;
    
    // Audio-video events catcher
    eventsCatcher->catchNextEvent();
}
```

* **Threads are not used at all** and they are intentionally discouraged in order to avoid that they can be improperly used for decoupling tasks, without taking advantage from multi-core systems.
* All the audio/video modules (-> classes) make **extensive use of templates** and all their possible concatenations are checked at **compile-time**, so to avoid inconsistent pipes.
* All the pipes are **safe at runtime**. I.E: when a source is disconnected or temporarily unavailable, the main loop can continue without necessarily having to check errors (they can be checked, anyway, by polling the status of each node: see **[THIS](https://github.com/paolo-pr/laav/blob/master/examples/AudioVideoExample_2.cpp)** example)
* The library is all RAII-designed (basically it safely wraps Libavcodec/format, V4L and ALSA) and **the user doesn't have to bother with pointers and memory management**.
* The public API is intended to be intuitive, with a few self-explanatory functions.

## COMPILING / RUNNING

Dependencies: **[FFmpeg](https://ffmpeg.org/)** >= 4.1 (tested with 4.1 version), **[libevent](http://libevent.org/)** and pkg-config (optional: see the compile command below).
**[FFmpeg](https://ffmpeg.org/)** must be compiled with **[x264](http://www.videolan.org/developers/x264.html)** and **[libopus](http://opus-codec.org/)** support.

* Include the library headers, as shown in the [examples](https://github.com/paolo-pr/laav/tree/master/examples), in YourProgram.cpp and execute:
```
g++ -Wall -std=c++11 -DLINUX -o YourProgram YourProgram.cpp `pkg-config --libs libavformat libavcodec libavutil libswresample libswscale libevent alsa`
```
* Execute ./CompileExamples for compiling the provided examples.
* API documentation in HTML format can be created by executing, inside the doxy directory:
```
doxygen Doxyfile
```
The library has been tested with the **[VLC](http://www.videolan.org/)** and **[FFPLAY](https://ffmpeg.org/)** media players, but other players should work as well.
In order to reduce the network streams' latency, use the following flags on the client side:

VLC (tested with v2.2.4: set a low value for --network-caching flag):
```
vlc --network-caching 200 http://stream_url
```
FFPLAY:
```
ffplay -fflags nobuffer http://stream_url
```

## HOW TO USE IT

See the provided **[EXAMPLES](https://github.com/paolo-pr/laav/tree/master/examples)**

## TODO

* Improve the compile-time error messages, for inconsistent pipes, with more static_assert(...) calls.
* Add a RTSP/RTP streaming server.
* Windows port (basically, it will consist in creating Windows based classes corresponding to the ALSAGrabber and V4L2Grabber classes, with the same API, and few other things: any contribution is welcome!).
* MPEGTS-MJPEG is currently NOT supported.

# Optimum Latency Streaming System

## ABOUT

An experimental **UDP** audio (**OPUS**)+ video (**H264**) streaming system (streaming server + client player) with an optimized latency, based on LAAV and Gstreamer.
The system is composed as follows:

## Streaming server


![Image](olss-server.png)

* All the modules, except (4), do not introduce significant latency
* Module (4) introduces 1 frame latency, due to FFmpeg MPEGTS muxer.

## Client player

![Image](olss-player.png)

* Modules (3A) and (4A) do not introduce significant latency.
* Module's (1) latency depends on the network latency/jitter. Late UDP packets could be dropped by setting blocksize and buffer-size properties on the udpsrc element in OptimumLatencyPlayer. It is suggested to do preliminary tests with both player and streamer on localhost, so to avoid this latency. Note that the player ensures that (1) starts to push frames to (2A) and (2B) only when a first PCR is obtained.
* Module (4B) introduces latency, here eliminated by setting the "max-threads = 1" property on the gstreamer decoder.
* Each of the modules (2A), (2B), (3B) introduces the latency of 1 frame (due to a missing feature of gstreamer). Note that a PAT/PMT is sent for each muxed frame, on the streamer's muxer (4), in order to avoid further latency introduced by modules (2A) and (2B) when starting to parse the MPEGTS stream (not sure if it's necessary. Anyway: see the macro PAT_PMT_AT_FRAMES in FFMPEGMuxer.hpp).
* Latencies introduced by (5A) and (5B) are minimized through the properties: buffer-time + latency-time (5A) and max-lateness (5B) on the gstreamer corresponding elements.

In order to avoid 1-frame latency introduced by (2A), (2B), (3A), (3B), and (4), the patches included in patches/gst-plugins-bad-1.15.2 and patches/ffmpeg-4.4.2/libavformat can be applied. They are independent of the Live Asynchronous Audio Video Library project and can optimize ANY MPEGTS-OPUS-H264 live streamer and player made with FFmpeg (streamer) and gstreamer (player).

A player with optimized latency can be also obtained by simply launching "ffplay -fflags nobuffer udp://127.0.0.1:8084": the player proposed in OptimumLatencyPlayer helps the user to print timestamps for each module of the pipe.

The system is based on the idea that audio/video synchronization is performed on the server side and avoids using the synchronizer provided by gstreamer, which introduces a latency that is hard to eliminate. In order to avoid the above mentioned synchronizer, two different MPEGTS demuxers are used, one for the audio pipe and one for the video pipe, corresponding to two independent threads on the player.

The system is strictly experimental, not fully tested and, for the moment, requires complex installation.

Tests must be performed with a video source with a framerate of at least 25fps and an audio source with a sampling rate of 48000. Write me an-email (see the email address in github) if you need additional help.

As said before, it is suggested to perform tests by launching both player and streamer on localhost, comparing the encoding date of each audio and video frame, printed as output, with that of the same frame, demuxed and decoded, provided by the player.

You can also modify the system by creating a HTTP (instead of UDP) streamer, with low latency as well, by compiling the LAAV library with the TCP_NODELAY macro (see where it is defined in EventsManager.hpp).

Due to the complexity of the project, any help/feedback is GREATLY appreciated!

## COMPILING 

* Required: gstreamer with bad/good plugins and gst-libav (tested on 1.12.5), FFmpeg (tested on 4.1)
Be sure that ALL the gstreamer modules required by the player are properly installed (you can check that with gst-inspect-1.0 module-name): udpsrc, appsink, appsrc, tsdemux, audioconvert, avdec_opus, pulsesink, h264parse, avdec_h264, xvimagesink.
* Execute: ./Compile

## RUNNING

Execute:

* (Streaming server)<br>
    ./OptimumLatencyStreamer.bin
* (Client player)<br>
    ./OptimumLatencyPlayer.bin alsa-device-identifier[i.e: plughw:U0x46d0x819] /path/to/v4l/device receiver_address

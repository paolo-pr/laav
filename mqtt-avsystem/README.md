# MQTTLAAVSystem

## ABOUT

Recording/streaming system template MQTT full-compliant. Particularly useful for home-automation applications which use the MQTT protocol (for example: **[Home Assistant](https://www.home-assistant.io)**). It uses the excellent **[MQTT-C library](https://github.com/LiamBindle/MQTT-C)**.

## HOW IT WORKS

* It creates a video grabber with "**Cam1**" id.

* It connects to a MQTT broker and subscribe to two different topics: **topicPrefix_IN** and **topicPrefix_OUT**, where **"topicPrefix"** is passed as argument when launching the application. 

* It creates two H264 encoders, with two different ids: "**Cam1-Encoder-HQ**" (640x480 resolution) and "**Cam1-Encoder-LQ**" (320x240 resolution).

* It creates two HTTP streamers, fed by the two previous encoders.

* It creates two video recorders, fed by the two previous encoders, with "**Cam1-Recorder-HQ**" and "**Cam1-Recorder-LQ**" ids.

* It listens for MQTT incoming messages on "**topicPrefix_IN**" topic, executes commands described in the messages and sends their result on the "**topicPrefix_OUT**" topic. MQTT commands must be in the form:

    **ID---REQUEST---VALUE**

    (where "---" is the separator token between the three fields).<br>
    Possible commands are:

    1. **Start/Stop recording**<br>
       ID = Recorder id (in this case:  "Cam1-Recorder-HQ" or "Cam1-Recorder-LQ"<br>
       REQUEST = Rec<br>
       VALUE = 1 (start recording) or 0 (stop recording)<br>   
       
    2. **Set Bitrate** (works on H264 encoders)<br>
       ID = Encoder id (in this case:  "Cam1-Encoder-HQ" or "Cam1-Encoder-LQ"<br>
       REQUEST = Bitrate<br>
       VALUE = new birate

* At each (re)connection to the MQTT broker, it will inform about the state of the audio/video device by sending a message on topicPrefix_OUT topic, in the form previously described, with:

    ID = Grabber id (in this case:  "Cam1")<br>
    REQUEST = AVDevGrabbing<br>
    VALUE = 0 (not grabbing frames) or 1 (grabbing frames)

* At each (re)connection to the MQTT broker, it will inform about the state of each recorder/encoder by sending 1) and 2) messages on the topicPrefix_OUT topic.

* Each incoming command will be backed up on file (.laavbackup) so to restore the state of the system before any interruption (i.e: power failure)

* Each recording will have a name containing the recorder's id and the current date, and it will be fragmented if the file size will exceed MAX_REC_FILESIZE (here set to 100 MB)

* It checks continuously if the streamers and the recorders fed by each encoder are all inactive (no clients connected and no recordings): in this case, it pauses the encoder, so to not stress the CPU, and will re-activate it as soon as at least one of the linked media gets active again.

## CUSTOMIZE
   
The code of MQTTLAAVSystem can be very easily customized so to work with different pipes/encoders/recorders. Just edit the blocks marked with "CUSTOM" tag by adding custom elements and pipes in the same way shown in the other examples, and DON'T touch the other parts of the code. It's even simpler than the other examples because you only have to write the elements' and the pipes' definitions, but NOT the logic behind the recordings, disconnections etc.

## COMPILING / RUNNING

* Just copy the **[MQTT-C library](https://github.com/LiamBindle/MQTT-C)** directory inside this directory (you must have the library's directory tree starting with "MQTT-C" inside this directory) and compile it (see the library's homepage for details).

*  Compile MQTTLaavSystem by executing "Compile".

* Execute:

    ./MQTTLaavSystem.bin /path/to/v4l/device brokerAddress brokerPort topicPrefix

** Enjoy! **

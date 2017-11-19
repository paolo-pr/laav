/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 * This example shows how to grab video from a V4L camera,
 * encode (H264) and stream it through HTTP with a MPEGTS container.
 * The stream's address is:
 * 
 *   http://127.0.0.1:8080/stream.ts
 * 
 */

#include "V4L2Grabber.hpp"

#define WIDTH 640
#define HEIGHT 480

using namespace laav;

int main(int argc, char** argv)
{

    if (argc < 2) 
    {
        std::cout << "Usage: " << argv[0] << " /path/to/v4l/device" << std::endl;
        return 1;
    }

    SharedEventsCatcher eventsCatcher = EventsManager::createSharedEventsCatcher();

    V4L2Grabber <YUYV422_PACKED, WIDTH, HEIGHT>
    vGrab(eventsCatcher, argv[1]);

    // Needed for the H264 encoder (it accepts only PLANAR formats)
    FFMPEGVideoConverter <YUYV422_PACKED, WIDTH, HEIGHT, YUV420_PLANAR, WIDTH, HEIGHT>
    vConv;

    FFMPEGH264Encoder <YUV420_PLANAR, WIDTH, HEIGHT>
    vEnc(DEFAULT_BITRATE, 5, H264_ULTRAFAST, H264_DEFAULT_PROFILE, H264_DEFAULT_TUNE);

    HTTPVideoStreamer <MPEGTS, H264, WIDTH, HEIGHT>
    vStream(eventsCatcher, "127.0.0.1", 8080);

    while (1)
    {
        vGrab >> vConv >> vEnc >> vStream;
        
        eventsCatcher->catchNextEvent();
    }
    
    return 0;

}

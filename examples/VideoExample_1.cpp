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
 * --------------------------------------------
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

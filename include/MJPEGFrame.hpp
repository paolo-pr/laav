/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef MJPEGFRAME_HPP_INCLUDED
#define MJPEGFRAME_HPP_INCLUDED

namespace laav
{

class MJPEG {};

template <unsigned int width_, unsigned int height_>
class VideoFrame<MJPEG, width_, height_> :
public VideoFrameBase<width_, height_>,
public EncodedVideoFrame
{

public:

    VideoFrame<MJPEG, width_, height_> ():
        EncodedVideoFrame(width_, height_)
    {
    }

};

}

#endif // MJPEGFRAME_HPP_INCLUDED

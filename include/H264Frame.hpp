/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef H264FRAME_HPP_INCLUDED
#define H264FRAME_HPP_INCLUDED

namespace laav
{

class H264 {};

template <unsigned int width_, unsigned int height_>
class VideoFrame<H264, width_, height_> :
public VideoFrameBase<width_, height_>,
public EncodedVideoFrame
{

public:

    VideoFrame<H264, width_, height_>():
        EncodedVideoFrame(width_, height_)
    {
    }

};

}

#endif // H264FRAME_HPP_INCLUDED

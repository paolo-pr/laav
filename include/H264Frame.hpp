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

#ifndef H264FRAME_HPP_INCLUDED
#define H264FRAME_HPP_INCLUDED

namespace laav
{

class H264 {};

template <typename width_, typename height_>
class VideoFrame<H264, width_, height_> :
public VideoFrameBase<width_, height_>,
public EncodedVideoFrame
{

public:

    VideoFrame<H264, width_, height_>():
        EncodedVideoFrame(width_::value, height_::value)
    {
    }

};

}

#endif // H264FRAME_HPP_INCLUDED

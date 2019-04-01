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

#ifndef NV12PLANARFRAME_HPP_INCLUDED
#define NV12PLANARFRAME_HPP_INCLUDED

namespace laav
{

class NV12_PLANAR {};

template <typename width_, typename height_>
class VideoFrame<NV12_PLANAR, width_, height_> :
public VideoFrameBase<width_, height_>,
public Planar3RawVideoFrame,
public FormattedRawVideoFrame<unsigned char, unsigned char, unsigned char>
{

public:

    VideoFrame<NV12_PLANAR, width_, height_>() :
        Planar3RawVideoFrame(width_::value, height_::value)
    {
    }

    /*!
     *  \exception OutOfBounds
     */    
    const Pixel<unsigned char, unsigned char, unsigned char>& pixelAt(uint16_t x, uint16_t y) const
    {
        printAndThrowUnrecoverableError("NOT IMPLEMENTED YET! TODO!");
        return mCurrPixel;
    }

    /*!
     *  \exception OutOfBounds
     */
    void setPixelAt(const Pixel<unsigned char, unsigned char, unsigned char>& pixel,
                    uint16_t x, uint16_t y)
    {
        printAndThrowUnrecoverableError("NOT IMPLEMENTED YET! TODO!");
    }

};

}

#endif // NV12_PLANARFRAME_HPP_INCLUDED

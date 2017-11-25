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

#ifndef YUV420PLANARFRAME_HPP_INCLUDED
#define YUV420PLANARFRAME_HPP_INCLUDED

#include "Frame.hpp"

namespace laav
{

class YUV420_PLANAR {};

template <unsigned int width_, unsigned int height_>
class VideoFrame<YUV420_PLANAR, width_, height_> :
public VideoFrameBase<width_, height_>,
public Planar3RawVideoFrame,
public FormattedRawVideoFrame<unsigned char, unsigned char, unsigned char>
{

public:

    VideoFrame<YUV420_PLANAR, width_, height_>() :
        Planar3RawVideoFrame(width_, height_)
    {
    }

    /*!
     *  \exception OutOfBounds
     */    
    const Pixel<unsigned char, unsigned char, unsigned char>&
    pixelAt(uint16_t x, uint16_t y) const
    {
        if (x > width_ - 1 || y > height_ - 1)
            throw OutOfBounds();

        uint16_t x1 = x / 2;
        mCurrPixel.set(this->plane<0>()[y * width_ + x],
                       this->plane<1>()[y * width_ / 4 + x1],
                       this->plane<1>()[y * width_ / 4 + x1]);

        return mCurrPixel;
    }

    /*!
     *  \exception OutOfBounds
     */    
    void setPixelAt(const Pixel<unsigned char, unsigned char, unsigned char>& pixel,
                    uint16_t x, uint16_t y)
    {
        if (x > width_ - 1 || y > height_ - 1)
            throw OutOfBounds();
        
        this->plane<0>()[y * width_ + x] = pixel.component<0>();
        uint16_t x1 = x / 2;
        this->plane<1>()[y * width_ / 4 + x1] = pixel.component<1>();
        this->plane<2>()[y * width_ / 4 + x1] = pixel.component<2>();
    }

};

}

#endif // YUV420PLANARFRAME_HPP_INCLUDED

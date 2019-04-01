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

#ifndef YUYV422PACKEDFRAME_HPP_INCLUDED
#define YUYV422PACKEDFRAME_HPP_INCLUDED

namespace laav
{

class YUYV422_PACKED {};

template <typename width_, typename height_>
class VideoFrame<YUYV422_PACKED, width_, height_> :
public VideoFrameBase<width_, height_>,
public PackedRawVideoFrame,
public FormattedRawVideoFrame<unsigned char, unsigned char, unsigned char>
{

public:

    VideoFrame<YUYV422_PACKED, width_, height_>() :
        PackedRawVideoFrame(width_::value, height_::value)
    {
    }

    /*!
     *  \exception OutOfBounds
     */    
    const Pixel<unsigned char, unsigned char, unsigned char>& pixelAt(uint16_t x, uint16_t y) const
    {
        if (x > width_::value - 1 || y > height_::value - 1)
            throw OutOfBounds();
        
        uint16_t x1 = x / 2;
        mCurrPixel.set(this->data()[y * width_::value * 2 + 4 * x1],
                       this->data()[y * width_::value * 2 + 4 * x1 + 1],
                       this->data()[y * width_::value * 2 + 4 * x1 + 3]);
        return mCurrPixel;
    }

    /*!
     *  \exception OutOfBounds
     */    
    void setPixelAt(const Pixel<unsigned char, unsigned char, unsigned char>& pixel,
                    uint16_t x, uint16_t y)
    {
        if (x > width_::value - 1 || y > height_::value - 1)
            throw OutOfBounds();
        
        uint16_t x1 = x / 2;
        this->data()[y * width_::value * 2 + 4 * x1] = pixel.component<0>();
        this->data()[y * width_::value * 2 + 4 * x1 + 1] = pixel.component<1>();
        this->data()[y * width_::value * 2 + 4 * x1 + 2] = pixel.component<0>();
        this->data()[y * width_::value * 2 + 4 * x1 + 3] = pixel.component<2>();
    }

};

}

#endif // YUYV422PACKEDFRAME_HPP_INCLUDED

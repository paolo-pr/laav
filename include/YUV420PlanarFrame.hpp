/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
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

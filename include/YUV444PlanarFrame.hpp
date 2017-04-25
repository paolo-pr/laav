/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef YUV444PLANARFRAME_HPP_INCLUDED
#define YUV444PLANARFRAME_HPP_INCLUDED

namespace laav
{

class YUV444_PLANAR {};

template <unsigned int width_, unsigned int height_>
class VideoFrame<YUV444_PLANAR, width_, height_> :
public VideoFrameBase<width_, height_>,
public Planar3RawVideoFrame,
public FormattedRawVideoFrame<unsigned char, unsigned char, unsigned char>
{

public:

    VideoFrame<YUV444_PLANAR, width_, height_>() :
        Planar3RawVideoFrame(width_, height_)
    {
    }

    /*!
     *  \exception OutOfBounds
     */    
    const Pixel<unsigned char, unsigned char, unsigned char>& pixelAt(uint16_t x, uint16_t y) const
    {
        if (x > width_ - 1 || y > height_ - 1)
            throw OutOfBounds();
        
        mCurrPixel.set(this->plane<0>()[y * width_ + x],
                       this->plane<1>()[y * width_ + x],
                       this->plane<1>()[y * width_ + x]);
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
        this->plane<1>()[y * width_ + x] = pixel.component<1>();
        this->plane<2>()[y * width_ + x] = pixel.component<2>();
    }

};

}

#endif // YUV444PLANARFRAME_HPP_INCLUDED

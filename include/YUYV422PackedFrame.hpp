/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef YUYV422PACKEDFRAME_HPP_INCLUDED
#define YUYV422PACKEDFRAME_HPP_INCLUDED

namespace laav
{

class YUYV422_PACKED {};

template <unsigned int width_, unsigned int height_>
class VideoFrame<YUYV422_PACKED, width_, height_> :
public VideoFrameBase<width_, height_>,
public PackedRawVideoFrame,
public FormattedRawVideoFrame<unsigned char, unsigned char, unsigned char>
{

public:

    VideoFrame<YUYV422_PACKED, width_, height_>() :
        PackedRawVideoFrame(width_, height_)
    {
    }

    /*!
     *  \exception OutOfBounds
     */    
    const Pixel<unsigned char, unsigned char, unsigned char>& pixelAt(uint16_t x, uint16_t y) const
    {
        if (x > width_ - 1 || y > height_ - 1)
            throw OutOfBounds();
        
        uint16_t x1 = x / 2;
        mCurrPixel.set(this->data()[y * width_ * 2 + 4 * x1],
                       this->data()[y * width_ * 2 + 4 * x1 + 1],
                       this->data()[y * width_ * 2 + 4 * x1 + 3]);
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
        
        uint16_t x1 = x / 2;
        this->data()[y * width_ * 2 + 4 * x1] = pixel.component<0>();
        this->data()[y * width_ * 2 + 4 * x1 + 1] = pixel.component<1>();
        this->data()[y * width_ * 2 + 4 * x1 + 2] = pixel.component<0>();
        this->data()[y * width_ * 2 + 4 * x1 + 3] = pixel.component<2>();
    }

};

}

#endif // YUYV422PACKEDFRAME_HPP_INCLUDED

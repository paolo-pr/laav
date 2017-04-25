/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef PIXEL_HPP_INCLUDED
#define PIXEL_HPP_INCLUDED

#include <tuple>

namespace laav
{

template <typename... Components>
class Pixel
{

public:

    // taken from the standard library for tuples (std::get<..>())
    template <std::size_t I>
    const typename std::tuple_element<I, std::tuple<Components...> >::type&
    component() const
    {
        return std::get<I, Components...>(mComponents);
    }

    void set(Components... components)
    {
        mComponents = std::make_tuple(components...);
    }

private:

    std::tuple<Components...> mComponents;

};

typedef Pixel<unsigned char, unsigned char, unsigned char> YUVPixel;

}

#endif // PIXEL_HPP_INCLUDED

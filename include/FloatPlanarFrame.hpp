/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef FLOATPLANARFRAME_HPP_INCLUDED
#define FLOATPLANARFRAME_HPP_INCLUDED

#include "Frame.hpp"

namespace laav
{

class FLOAT_PLANAR {};

template <unsigned int audioSampleRate, enum AudioChannels audioChannels>
class AudioFrame<FLOAT_PLANAR, audioSampleRate, audioChannels> :
public Frame, public Planar2RawAudioFrame
{
};

}

#endif // FLOATPLANARFRAME_HPP_INCLUDED

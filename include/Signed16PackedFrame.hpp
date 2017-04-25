/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef SIGNED16PACKEDFRAME_HPP_INCLUDED
#define SIGNED16PACKEDFRAME_HPP_INCLUDED

#include "Frame.hpp"

namespace laav
{

class S16_LE {};

template <unsigned int audioSampleRate, enum AudioChannels audioChannels>
class AudioFrame<S16_LE, audioSampleRate, audioChannels> : public Frame, public PackedRawAudioFrame
{
};

}

#endif // SIGNED16PACKEDFRAME_HPP_INCLUDED

/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef MP2FRAME_HPP_INCLUDED
#define MP2FRAME_HPP_INCLUDED

#include "Frame.hpp"

namespace laav
{

class MP2 {};

template <unsigned int audioSampleRate, enum AudioChannels audioChannels>
class AudioFrame<MP2, audioSampleRate, audioChannels> :
public EncodedAudioFrame<MP2>
{
};

}

#endif // MP2FRAME_HPP_INCLUDED

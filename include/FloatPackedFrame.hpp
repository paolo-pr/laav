/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef FLOATPACKEDFRAME_HPP_INCLUDED
#define FLOATPACKEDFRAME_HPP_INCLUDED

namespace laav
{

class FLOAT_PACKED {};

template <unsigned int audioSampleRate, enum AudioChannels audioChannels>
class AudioFrame<FLOAT_PACKED, audioSampleRate, audioChannels > :
public Frame, public PackedRawAudioFrame
{
};

}

#endif // FLOATPACKEDFRAME_HPP_INCLUDED

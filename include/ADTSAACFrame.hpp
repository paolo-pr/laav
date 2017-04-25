/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef ADTSAACFRAME_HPP_INCLUDED
#define ADTSAACFRAME_HPP_INCLUDED

#include "Frame.hpp"

namespace laav
{

class ADTS_AAC {};

template <unsigned int audioSampleRate, enum AudioChannels audioChannels>
class AudioFrame<ADTS_AAC, audioSampleRate, audioChannels> : public EncodedAudioFrame<ADTS_AAC> {};

}

#endif // ADTSAACFRAME_HPP_INCLUDED

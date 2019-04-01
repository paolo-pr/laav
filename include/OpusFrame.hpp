/* 
 * Created (15/03/2019) by Paolo-Pr.
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

#ifndef OPUSFRAME_HPP_INCLUDED
#define OPUSFRAME_HPP_INCLUDED

#include "Frame.hpp"

namespace laav
{

class OPUS {};

template <typename audioSampleRate, typename audioChannels>
class AudioFrame<OPUS, audioSampleRate, audioChannels> :
public EncodedAudioFrame<OPUS>
{
};

}

#endif // OPUSFRAME_HPP_INCLUDED

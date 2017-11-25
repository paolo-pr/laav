/* 
 * Created (25/04/2017) by Paolo-Pr.
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

#ifndef UDPAUDIOSTREAMER_HPP_INCLUDED
#define UDPAUDIOSTREAMER_HPP_INCLUDED

#include "UDPStreamer.hpp"

namespace laav
{

template <typename Container,
          typename AudioCodecOrFormat, unsigned int audioSampleRate, enum AudioChannels audioChannels>
class UDPAudioStreamer : public UDPStreamer<Container>
{

public:

    UDPAudioStreamer(std::string address, unsigned int port) :
        UDPStreamer<Container>(address, port),
        mAudioMuxer(true)
    {
    }

    void takeStreamableFrame(const AudioFrame<AudioCodecOrFormat, audioSampleRate,
                                              audioChannels>& audioFrameToStream)
    {
        mAudioMuxer.takeMuxableFrame(audioFrameToStream);
    }

    void streamMuxedData()
    {   
        const std::vector<MuxedAudioData<Container,
                                         AudioCodecOrFormat, audioSampleRate,
                                         audioChannels> >&                                              
        chunksToStream = mAudioMuxer.muxedAudioChunks();
        unsigned int n;
        std::vector<unsigned char> dataToStream;
        
        for (n = 0; n < mAudioMuxer.muxedAudioChunksOffset(); n++)
        {
            dataToStream.insert(dataToStream.end(), chunksToStream[n].data(), 
                                chunksToStream[n].data()+chunksToStream[n].size());
        }
        if (sendto(this->mSock, dataToStream.data(), dataToStream.size(), 0, 
            (const sockaddr* )&this->mSockAddr, this->mSlen)==-1)
            printAndThrowUnrecoverableError("UDPAudioStreamer::streamMuxedData(); sendto() error");
    }

private:

    FFMPEGAudioMuxer<Container,
                     AudioCodecOrFormat, audioSampleRate, audioChannels> mAudioMuxer;

};

}

#endif // UDPAUDIOSTREAMER_HPP_INCLUDED

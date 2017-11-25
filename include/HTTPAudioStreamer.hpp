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

#ifndef HTTPAUDIOSTREAMER_HPP_INCLUDED
#define HTTPAUDIOSTREAMER_HPP_INCLUDED

#include "HTTPStreamer.hpp"

namespace laav
{

template <typename Container, typename AudioCodecOrFormat,
          unsigned int audioSampleRate, enum AudioChannels audioChannels>
class HTTPAudioStreamer : public HTTPStreamer<Container>
{

public:

    HTTPAudioStreamer(SharedEventsCatcher eventsCatcher, std::string address, unsigned int port) :
        HTTPStreamer<Container>(eventsCatcher, address, port),
        mAudioMuxer(false)
    {
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    void takeStreamableFrame(const AudioFrame<AudioCodecOrFormat,
                                              audioSampleRate, audioChannels>& audioFrameToStream)
    {
        if (this->mStatus == MEDIA_READY)
            mAudioMuxer.takeMuxableFrame(audioFrameToStream);
    }

    // Todo extract common code and move it to
    // base class (do the same for video and audiovideo streamers)
    void streamMuxedData()
    {
        if (this->mClientConnectionsAndRequests.size() != 0 && this->mStatus == MEDIA_READY)
        {
            try
            {
                using Iter3 = std::map<struct evhttp_connection*, struct evhttp_request* >::iterator;
                for (Iter3 it = this->mClientConnectionsAndRequests.begin();
                     it != this->mClientConnectionsAndRequests.end(); ++it)
                {

                    const std::vector<MuxedAudioData<Container, AudioCodecOrFormat,
                                      audioSampleRate, audioChannels> >& chunksToStream =
                    mAudioMuxer.muxedAudioChunks();

                    unsigned int n;
                    struct evbuffer* buf = evbuffer_new();
                    if (this->mWrittenHeaderFlagAndRequests[it->second] == false)
                    {
                        evbuffer_add(buf, mAudioMuxer.header().c_str(), mAudioMuxer.header().size());
                        this->mWrittenHeaderFlagAndRequests[it->second] = true;
                    }
                    for (n = 0; n < mAudioMuxer.muxedAudioChunksOffset(); n++)
                    {
                        if (buf == NULL)
                            printAndThrowUnrecoverableError("buf == NULL");
                        evbuffer_add(buf, chunksToStream[n].data(), chunksToStream[n].size());

                    }
                    evhttp_send_reply_chunk(it->second, buf);
                    evbuffer_free(buf);
                }
            }
            catch (const MediaException& mediaException)
            {
                // DO next step
            }
        }
    }

private:

    void hTTPDisconnectionCallBack(struct evhttp_connection* clientConnection)
    {
        this->mWrittenHeaderFlagAndRequests.erase(this->mClientConnectionsAndRequests[clientConnection]);
        evhttp_request_free(this->mClientConnectionsAndRequests[clientConnection]);
        this->mClientConnectionsAndRequests.erase(clientConnection);
        if (this->mClientConnectionsAndRequests.size() == 0)
        {
            mAudioMuxer.stopMuxing();
            this->mIsStreaming = false;           
        }
    }

    void hTTPConnectionCallBack(struct evhttp_request* clientRequest,
                                struct evhttp_connection* clientConnection)
    {
        this->mClientConnectionsAndRequests[clientConnection] = clientRequest;

        if (mAudioMuxer.isMuxing())
            this->mWrittenHeaderFlagAndRequests[clientRequest] = false;
        else
            this->mWrittenHeaderFlagAndRequests[clientRequest] = true;

        evhttp_send_reply_start(clientRequest, HTTP_OK, "OK");
        this->mIsStreaming = true;        
        mAudioMuxer.startMuxing();
    }

    FFMPEGAudioMuxer<Container, AudioCodecOrFormat, audioSampleRate, audioChannels> mAudioMuxer;

};

}

#endif // HTTPAUDIOSTREAMER_HPP_INCLUDED

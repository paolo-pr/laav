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

#ifndef HTTPAUDIOVIDEOSTREAMER_HPP_INCLUDED
#define HTTPAUDIOVIDEOSTREAMER_HPP_INCLUDED

#include "HTTPStreamer.hpp"

namespace laav
{

template <typename Container,
          typename VideoCodecOrFormat, unsigned int width, unsigned int height,
          typename AudioCodecOrFormat, unsigned int audioSampleRate, enum AudioChannels audioChannels>
class HTTPAudioVideoStreamer : public HTTPStreamer<Container>
{

public:

    HTTPAudioVideoStreamer(SharedEventsCatcher eventsCatcher, std::string address, unsigned int port) :
        HTTPStreamer<Container>(eventsCatcher, address, port),
        mAudioVideoMuxer(false)
    {
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    void takeStreamableFrame(const VideoFrame<VideoCodecOrFormat, width, height>& videoFrameToStream)
    {
        if (this->mStatus == MEDIA_READY)
            mAudioVideoMuxer.takeMuxableFrame(videoFrameToStream);
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    void takeStreamableFrame(const AudioFrame<AudioCodecOrFormat, audioSampleRate,
                                              audioChannels>& audioFrameToStream)
    {
        mAudioVideoMuxer.takeMuxableFrame(audioFrameToStream);
    }

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

                    const std::vector<MuxedAudioVideoData<Container, VideoCodecOrFormat,
                                                          width, height,
                                                          AudioCodecOrFormat, audioSampleRate,
                                                          audioChannels> >&
                    chunksToStream = mAudioVideoMuxer.muxedAudioVideoChunks();

                    unsigned int n;
                    struct evbuffer* buf = evbuffer_new();
                    if (this->mWrittenHeaderFlagAndRequests[it->second] == false)
                    {
                        evbuffer_add(buf, mAudioVideoMuxer.header().c_str(),
                                     mAudioVideoMuxer.header().size());

                        this->mWrittenHeaderFlagAndRequests[it->second] = true;
                    }
                    for (n = 0; n < mAudioVideoMuxer.muxedAudioVideoChunksOffset(); n++)
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
            mAudioVideoMuxer.stopMuxing();
            this->mIsStreaming = false;
        }
    }

    void hTTPConnectionCallBack(struct evhttp_request* clientRequest,
                                struct evhttp_connection* clientConnection)
    {
        this->mClientConnectionsAndRequests[clientConnection] = clientRequest;
        if (mAudioVideoMuxer.isMuxing())
            this->mWrittenHeaderFlagAndRequests[clientRequest] = false;
        else
            this->mWrittenHeaderFlagAndRequests[clientRequest] = true;
        evhttp_send_reply_start(clientRequest, HTTP_OK, "OK");
        mAudioVideoMuxer.startMuxing();
        this->mIsStreaming = true;
    }

    FFMPEGAudioVideoMuxer<Container,
                          VideoCodecOrFormat, width, height,
                          AudioCodecOrFormat, audioSampleRate, audioChannels> mAudioVideoMuxer;

};

}

#endif // HTTPAUDIOVIDEOSTREAMER_HPP_INCLUDED

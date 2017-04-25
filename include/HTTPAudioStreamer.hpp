/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
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
        mAudioMuxer.takeMuxableFrame(audioFrameToStream);
    }

    // Todo extract common code and move it to
    // base class (do the same for video and audiovideo streamers)
    void sendMuxedData()
    {
        if (this->mClientConnectionsAndRequests.size() != 0)
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
        mAudioMuxer.startMuxing();
    }

    FFMPEGAudioMuxer<Container, AudioCodecOrFormat, audioSampleRate, audioChannels> mAudioMuxer;

};

}

#endif // HTTPAUDIOSTREAMER_HPP_INCLUDED

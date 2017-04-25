/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef HTTPVIDEOSTREAMER_HPP_INCLUDED
#define HTTPVIDEOSTREAMER_HPP_INCLUDED

#include "HTTPStreamer.hpp"

namespace laav
{

template <typename Container,
          typename VideoCodecOrFormat,
          unsigned int width,
          unsigned int height>
class HTTPVideoStreamer : public HTTPStreamer<Container>
{

public:

    HTTPVideoStreamer(SharedEventsCatcher eventsCatcher, std::string address, unsigned int port) :
        HTTPStreamer<Container>(eventsCatcher, address, port),
        mLatency(0),
        mVideoStreamingStartTime(0),
        mVideoMuxer(false)
    {
    }

    /*!
     *  \exception MediaException(MEDIA_NO_DATA)
     */
    void takeStreamableFrame(const VideoFrame<VideoCodecOrFormat,
                             width, height>& videoFrameToStream)
    {
        mVideoMuxer.takeMuxableFrame(videoFrameToStream);
    }

    void streamMuxedData()
    {

        if (this->mClientConnectionsAndRequests.size() != 0)
        {
            try
            {
                using Iter3 = std::map<struct evhttp_connection*, struct evhttp_request* >::iterator;
                for (Iter3 it = this->mClientConnectionsAndRequests.begin();
                     it != this->mClientConnectionsAndRequests.end(); ++it)
                {
                    const std::vector<MuxedVideoData<Container, VideoCodecOrFormat, width, height> >&
                    chunksToStream = mVideoMuxer.muxedVideoChunks();

                    unsigned int n;
                    struct evbuffer* buf = evbuffer_new();
                    if (this->mWrittenHeaderFlagAndRequests[it->second] == false)
                    {
                        evbuffer_add(buf, mVideoMuxer.header().c_str(), mVideoMuxer.header().size());
                        this->mWrittenHeaderFlagAndRequests[it->second] = true;
                    }
                    for (n = 0; n < mVideoMuxer.muxedVideoChunksOffset(); n++)
                    {
                        if (buf == NULL)
                            printAndThrowUnrecoverableError("buf == NULL");

                        evbuffer_add(buf, chunksToStream[n].data(), chunksToStream[n].size());
                    }
                    if (mLatency == 0)
                    {
                        mLatency = av_gettime_relative() - mVideoStreamingStartTime;
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
            mVideoMuxer.stopMuxing();
        }

    }

    void hTTPConnectionCallBack(struct evhttp_request* clientRequest,
                                struct evhttp_connection* clientConnection)
    {
        this->mClientConnectionsAndRequests[clientConnection] = clientRequest;

        if (mVideoMuxer.isMuxing())
            this->mWrittenHeaderFlagAndRequests[clientRequest] = false;
        else
            this->mWrittenHeaderFlagAndRequests[clientRequest] = true;
        evhttp_send_reply_start(clientRequest, HTTP_OK, "OK");
        mVideoMuxer.startMuxing();
    }

    int64_t mLatency;
    int64_t mVideoStreamingStartTime;
    FFMPEGVideoMuxer<Container, VideoCodecOrFormat, width, height> mVideoMuxer;

};

}

#endif // HTTPVIDEOSTREAMER_HPP_INCLUDED

/*
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef HTTPSTREAMER_HPP_INCLUDED
#define HTTPSTREAMER_HPP_INCLUDED

#include "EventsManager.hpp"
#include "Streamer.hpp"

namespace laav
{

template <typename Container>
class HTTPStreamer  : public Streamer<Container>, public EventsProducer
{

public:

    bool isStreaming() const
    {
        return mIsStreaming;
    }
    
protected:

    HTTPStreamer(SharedEventsCatcher eventsCatcher, std::string address, unsigned int port):
        Streamer<Container>(address, port),
        EventsProducer::EventsProducer(eventsCatcher),
        mIsStreaming(false)
    {
        std::string location = "/stream." + containerExtension<Container>();
        if (!makeHTTPServerPollable(this->mAddress, location, this->mPort))
        {
            this->mErrno = errno;
            return;
        }
        observeHTTPEventsOn(this->mAddress, this->mPort);
        this->mStatus = MEDIA_READY;
    }

    ~HTTPStreamer()
    {
        dontObserveHTTPEventsOn(this->mAddress, this->mPort);
    }

    virtual void hTTPConnectionCallBack(struct evhttp_request* clientRequest,
                                        struct evhttp_connection* clientConnection) = 0;

    virtual void hTTPDisconnectionCallBack(struct evhttp_connection* clientConnection) = 0;

    std::map<struct evhttp_connection*, struct evhttp_request*> mClientConnectionsAndRequests;
    std::map<struct evhttp_request*, bool> mWrittenHeaderFlagAndRequests;
    bool mIsStreaming;

private:

    template <typename Container_>
    std::string containerExtension();

};

template <>
template <>
std::string HTTPStreamer<MPEGTS>::containerExtension<MPEGTS>()
{
    return "ts";
}

template <>
template <>
std::string HTTPStreamer<MATROSKA>::containerExtension<MATROSKA>()
{
    return "mkv";
}

}

#endif // HTTPSTREAMER_HPP_INCLUDED

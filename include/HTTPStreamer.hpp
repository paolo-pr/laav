/*
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef HTTPSTREAMER_HPP_INCLUDED
#define HTTPSTREAMER_HPP_INCLUDED

#include "EventsManager.hpp"
#include "FFMPEGAudioVideoMuxer.hpp"

namespace laav
{

template <typename Container>
class HTTPStreamer  : public EventsProducer
{

public:

    enum MediaStatus status() const
    {
        return mStatus;
    }

    int getErrno() const
    {
        return mErrno;
    }

protected:

    HTTPStreamer(SharedEventsCatcher eventsCatcher, std::string address, unsigned int port):
        EventsProducer::EventsProducer(eventsCatcher),
        mAddress(address),
        mPort(port),
        mStatus(MEDIA_NOT_READY),
        mErrno(0)
    {
        std::string location = "/stream." + containerExtension<Container>();
        if (!makeHTTPServerPollable(mAddress, location, mPort))
        {
            mErrno = errno;
            return;
        }
        observeHTTPEventsOn(mAddress, mPort);
        mStatus = MEDIA_READY;
    }

    ~HTTPStreamer()
    {
        dontObserveHTTPEventsOn(mAddress, mPort);
    }

    virtual void hTTPConnectionCallBack(struct evhttp_request* clientRequest,
                                        struct evhttp_connection* clientConnection) = 0;

    virtual void hTTPDisconnectionCallBack(struct evhttp_connection* clientConnection) = 0;

    std::map<struct evhttp_connection*, struct evhttp_request*> mClientConnectionsAndRequests;
    std::map<struct evhttp_request*, bool> mWrittenHeaderFlagAndRequests;
    std::string mAddress;
    unsigned int mPort;
    enum MediaStatus mStatus;

private:

    int mErrno;
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

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
    
    size_t numOfConnectedClients() const
    {
        return mClientConnectionsAndRequests.size();
    }
    
protected:

    HTTPStreamer(SharedEventsCatcher eventsCatcher, std::string address, 
                 unsigned int port, const std::string& id = ""):
        Streamer<Container>(address, port, id),
        EventsProducer::EventsProducer(eventsCatcher),
        mIsStreaming(false)
    {        
        Medium::mInputStatus = NOT_READY;
        Medium::mOutputStatus = NOT_READY;
        
        this->mLocation = "/stream." + containerExtension<Container>();
        if (!makeHTTPServerPollable(this->mAddress, this->mLocation, this->mPort))
        {
            this->mErrno = errno;
            return;
        }
        
        Medium::mInputStatus = READY;
        //Medium::mOutputStatus = READY;        
        
        observeHTTPEventsOn(this->mAddress, this->mPort);
    }

    ~HTTPStreamer()
    {
        dontObserveHTTPEventsOn(this->mAddress, this->mPort);
    }

    void hTTPConnectionCallBack(struct evhttp_request* clientRequest,
                                struct evhttp_connection* clientConnection)
    {
        this->mClientConnectionsAndRequests[clientConnection] = clientRequest;

        this->mWrittenHeaderFlagAndRequests[clientRequest] = false;
        this->mGotKeyFrameForRequest[clientRequest] = false;
        this->mSkipFramesForRequest[clientRequest] = -1;
        evhttp_send_reply_start(clientRequest, HTTP_OK, "OK");
        char* address; 
        u_short port;
        evhttp_connection_get_peer (clientConnection, &address, &port);
        std::string clientAddr =  address;
        loggerLevel1 << "HTTP Streamer with id='" << Medium::id() << "' : got new connection from client '" <<
        clientAddr << ":" << port << "'" << std::endl;
        //mVideoMuxer.startMuxing();
        this->mIsStreaming = true;
    }


    void hTTPDisconnectionCallBack(struct evhttp_connection* clientConnection)
    {
        char* address; 
        u_short port;
        evhttp_connection_get_peer (clientConnection, &address, &port);
        std::string clientAddr =  address;
        loggerLevel1 << "HTTP Streamer with id='" << Medium::id() << "' : got disconnection from client '" <<
        clientAddr << ":" << port << "'" << std::endl;        
        this->mWrittenHeaderFlagAndRequests.erase(this->mClientConnectionsAndRequests[clientConnection]);
        this->mGotKeyFrameForRequest.erase(this->mClientConnectionsAndRequests[clientConnection]);
        this->mSkipFramesForRequest.erase(this->mClientConnectionsAndRequests[clientConnection]);
        evhttp_request_free(this->mClientConnectionsAndRequests[clientConnection]);
        this->mClientConnectionsAndRequests.erase(clientConnection);
        if (this->mClientConnectionsAndRequests.size() == 0)
        {
            this->mIsStreaming = false;
        }
    }

    void logStreamAddr(const std::string& containerAndCodecsOrFormats)
    {
        std::string log = "HTTP streaming server (" + containerAndCodecsOrFormats + ") with address: http://"
                          + this->mAddress + ":" + std::to_string(this->mPort) + this->mLocation;
        if (Medium::mInputStatus == READY)        
            loggerLevel1 << "Created " << log << std::endl;
        else
        {
            std::string err = strerror(this->getErrno());
            loggerLevel1 << "Error while creating " << log 
            << " (" << err << ")" << std::endl;
        }
    }      
            
    std::map<struct evhttp_connection*, struct evhttp_request*> mClientConnectionsAndRequests;
    std::map<struct evhttp_request*, bool> mWrittenHeaderFlagAndRequests;
    std::map<struct evhttp_request*, bool> mGotKeyFrameForRequest;
    std::map<struct evhttp_request*, int> mSkipFramesForRequest;
    
    bool mIsStreaming;
    std::string mLocation;
    
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

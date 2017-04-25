/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef HTTPCOMMANDSRECEIVER_HPP_INCLUDED
#define HTTPCOMMANDSRECEIVER_HPP_INCLUDED

#include "EventsManager.hpp"

namespace laav
{

class HTTPCommandsReceiver : public EventsProducer
{

public:

    HTTPCommandsReceiver(SharedEventsCatcher eventsCatcher, std::string address, unsigned int port):
        EventsProducer::EventsProducer(eventsCatcher),
        mAddress(address),
        mPort(port)
    {
        std::string location = "/commands";
        makeHTTPServerPollable(mAddress, location, mPort);
        observeHTTPEventsOn(mAddress, mPort);
    }

    std::map<std::string, std::string>& receivedCommands()
    {
        return mCommands;
    }

    void clearCommands()
    {
        mCommands.clear();
    }

private:

    void hTTPDisconnectionCallBack(struct evhttp_connection* clientConnection)
    {
    }

    void hTTPConnectionCallBack(struct evhttp_request* clientRequest,
                                struct evhttp_connection* clientConnection)
    {
        struct evbuffer* inputBuf = evhttp_request_get_input_buffer(clientRequest);
        std::string data(evbuffer_get_length(inputBuf), ' ');
        evbuffer_copyout(inputBuf, &data[0], data.size());
        std::vector<std::string> tokens = split(data, '&');
        mCommands.clear();
        unsigned int q;
        for (q = 0; q < tokens.size(); q++)
        {
            std::vector<std::string> paramAndVal = split(tokens[q], '=');
            if (paramAndVal.size() == 2)
            {
                mCommands[paramAndVal[0]] = paramAndVal[1];
            }
        };
        struct evbuffer* buf = evbuffer_new();
        if (buf == NULL)
            printAndThrowUnrecoverableError("buf == NULL");
        evbuffer_add_printf(buf, "OK\n");
        evhttp_send_reply(clientRequest, HTTP_OK, "OK", buf);
        evbuffer_free(buf);
    }

    std::map<std::string, std::string> mCommands;
    std::map<struct evhttp_connection*, struct evhttp_request*> mClientConnectionsAndRequests;
    std::string mAddress;
    unsigned int mPort;

};

}

#endif // HTTPCOMMANDSRECEIVER_HPP_INCLUDED

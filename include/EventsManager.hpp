/* 
 * Created (25/04/2017) by Paolo-Pr.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 *
 */

#ifndef EVENTSMANAGER_HPP_INCLUDED
#define EVENTSMANAGER_HPP_INCLUDED

#include <map>
#include <vector>
#include <memory>

extern "C"
{
#include <event.h>
#include <evhttp.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <fcntl.h>
#include <assert.h>
#ifdef LINUX
#include "unistd.h"
#endif
}

namespace laav
{

enum EventType {EVENT_READ, EVENT_WRITE, EVENT_READWRITE};

class EventsCatcher
{
    friend class EventsProducer;
    friend class EventsManager;

public:

    void catchNextEvent()
    {
        if (mObservedEvents > 0)
            event_base_loop(mEventBase, EVLOOP_ONCE);
        else
            // PREVENTS CPU OVERLOAD WHEN THERE'S A LOOP WITHOUT FDS
            // TODO make portable
            sleep(1);
    }

    ~EventsCatcher()
    {
        event_base_free(mEventBase);
        event_config_free(mEventBaseConfig);
    }

private:

    EventsCatcher():
        mObservedEvents(0)
    {
        ignoreSigpipe();
        event_init();
        mEventBaseConfig = event_config_new();
        if (event_config_require_features(mEventBaseConfig, EV_FEATURE_FDS) < 0)
            printAndThrowUnrecoverableError
            ("event_config_require_features(mEventBaseConfig, EV_FEATURE_FDS) < 0");
        mEventBase = event_base_new_with_config(mEventBaseConfig);
    }

    struct event_base* libeventEventBase()
    {
        return mEventBase;
    }

    void incrementObservedEvents()
    {
        mObservedEvents++;
    }

    void decrementObservedEvents()
    {
        mObservedEvents--;
    }

    unsigned int mObservedEvents;
    struct event_base* mEventBase;
    struct event_config* mEventBaseConfig;

};

class EventsProducer
{

protected:

    EventsProducer(std::shared_ptr<EventsCatcher> eventsCatcher):
        mEventsCatcher(eventsCatcher)
    {
    }

    ~EventsProducer()
    {
        using Iter = std::vector<struct event* >::iterator;
        for (Iter it = mObservedEventContainers.begin();
             it != mObservedEventContainers.end(); ++it)
        {
            event_free(*it);
        }

        using Iter2 = std::map<int, struct event* >::iterator;
        for (Iter2 it = mObservedEventContainers2.begin();
             it != mObservedEventContainers2.end(); ++it)
        {
            event_free(it->second);
        }

        using Iter3 = std::map<int, struct evhttp* >::iterator;
        for (Iter3 it = mObservedHTTPEventContainers.begin();
             it != mObservedHTTPEventContainers.end(); ++it)
        {
            evhttp_free(it->second);
        }
    }

    void eventProduced(int fd, enum EventType eventType)
    {
        eventCallBack(fd, eventType);
    }

    void makePollable(int fd)
    {
        struct event* eventContainer;
        eventContainer = event_new(mEventsCatcher.get()->libeventEventBase(), fd,
                                   EV_READ | EV_WRITE | EV_PERSIST,
                                   EventsProducer::libeventCallback, this);
        mObservedEventContainers2[fd] = eventContainer;
    }

    void makeHTTPServerPollable(const std::string& address, const std::string& location, int port)
    {
        struct evhttp* httpEventContainer = evhttp_new(mEventsCatcher.get()->libeventEventBase());
        if (evhttp_bind_socket(httpEventContainer, address.c_str(), port) != 0)
            printAndThrowUnrecoverableError
            ("evhttp_bind_socket(httpEventContainer, address.c_str(), port)");
        if (evhttp_set_cb(httpEventContainer, location.c_str(),
            libEventHTTPConnectionCallBack, this) != 0)
            printAndThrowUnrecoverableError
            ("evhttp_set_cb(...)");
        mObservedHTTPEventContainers[port] = httpEventContainer;
    }

    void observeEventsOn(int fd)
    {
        event_add(mObservedEventContainers2[fd], NULL);
        mEventsCatcher->incrementObservedEvents();
    }

    void dontObserveEventsOn(int fd)
    {
        event_del(mObservedEventContainers2[fd]);
        mEventsCatcher->decrementObservedEvents();
    }

    // TODO: implement or remove??
    void observeHTTPEventsOn(std::string address, int port)
    {
    }

    // TODO: implement or remove??
    void dontObserveHTTPEventsOn(std::string address, int port)
    {
    }

    bool thereAreEventsPendingOn(int fd)
    {
        bool ret;
        event_pending
        (mObservedEventContainers2[fd], EV_TIMEOUT | EV_READ | EV_WRITE | EV_SIGNAL, NULL) == 0 ?
            ret = false : ret = true;
        return ret;
    }

    virtual void eventCallBack(int fd, enum EventType eventType)
    {
    }

    virtual void hTTPConnectionCallBack(struct evhttp_request* clientRequest,
                                        struct evhttp_connection* connection)
    {
    }

    virtual void hTTPDisconnectionCallBack(struct evhttp_connection* connection)
    {
    }

private:

    static void libeventCallback(evutil_socket_t fd, short what, void *arg)
    {
        EventsProducer* producer = static_cast<EventsProducer* >(arg);
        if (what == EV_READ)
        {
            producer->eventCallBack(fd, EVENT_READ);
        }
        else if (what == EV_WRITE)
        {
            producer->eventCallBack(fd, EVENT_WRITE);
        }
        else if ( what == (EV_READ | EV_WRITE) )
        {
            producer->eventCallBack(fd, EVENT_READWRITE);
        }
    }

    static void libEventHTTPConnectionCallBack(struct evhttp_request* clientRequest, void *arg)
    {
        EventsProducer* producer = static_cast<EventsProducer* >(arg);
        evhttp_connection_set_closecb
        (clientRequest->evcon, producer->libEventHTTPDisconnectionCallBack, producer);
        producer->hTTPConnectionCallBack(clientRequest, clientRequest->evcon);
    }

    static void libEventHTTPDisconnectionCallBack(struct evhttp_connection* evcon, void *arg)
    {
        EventsProducer* producer = static_cast<EventsProducer* >(arg);
        producer->hTTPDisconnectionCallBack(evcon);
    }

    std::map<int, struct evhttp*> mObservedHTTPEventContainers;
    std::map<int, struct event*> mObservedEventContainers2;
    std::vector<struct event*> mObservedEventContainers;
    std::shared_ptr<EventsCatcher> mEventsCatcher;

};

typedef std::shared_ptr<EventsCatcher> SharedEventsCatcher;

class EventsManager
{

public:

    static SharedEventsCatcher createSharedEventsCatcher()
    {
        return SharedEventsCatcher(new EventsCatcher());
    }

};

}

#endif // EVENTSMANAGER_HPP_INCLUDED

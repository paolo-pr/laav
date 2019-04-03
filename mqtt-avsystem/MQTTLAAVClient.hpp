/* 
 * Created (25/02/2019) by Paolo-Pr.
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

#ifndef MQTTLAAVCLIENT_HPP_INCLUDED
#define MQTTLAAVCLIENT_HPP_INCLUDED

//#define MQTT_DEBUG

#define MQTTLAAV_REC 	            "Rec"
#define MQTTLAAV_AVDEV_GRABBING     "AVDevGrabbing"
#define MQTTLAAV_BITRATE            "Bitrate"
#define SEPARATOR                   "---"

extern "C"{
    #include <stdlib.h>
    #include <stdio.h>
    #include "MQTT-C/include/mqtt.h"
#ifdef LINUX
    #include <unistd.h>    
    #include <netinet/tcp.h>
#endif    
}

namespace laav
{

class MQTTLAAVMsgException
{

public:

    MQTTLAAVMsgException(const std::string& cause):
    mCause(cause)
    { }

    const std::string& cause() const
    {
        return mCause;
    }

private:

    const std::string mCause;

};

enum MQTTConnectionStatus
{
    MQTT_CONNECTED,
    MQTT_DISCONNECTED
};

class MQTTLAAVMsg
{

public:

    MQTTLAAVMsg():
        valInt(-1)
    {}
    
    //TODO: add getters funcs and proper exception handling
    std::string what;    
    std::string resourceId;
    int valInt;
    std::string valStr;  
    std::string rawStr;
};

class MQTTLAAVClient;

struct reconnect_state_t 
{
    const char* hostname;
    const char* port;
    const char* topicIN;
    uint8_t* sendbuf;
    size_t sendbufsz;
    uint8_t* recvbuf;
    size_t recvbufsz;
    MQTTLAAVClient* mqttLaavClient;
};

class MQTTLAAVClient
{

public:

    MQTTLAAVClient(const std::string& brokerAddress, 
                   int port, const std::string& topicPrefix):
        mBrokerAddress(brokerAddress),
        mPort(port),
        mTopicIN(topicPrefix+"_IN"),
        mTopicOUT(topicPrefix+"_OUT"),        
        mLastPublishedINMsg(""),
        mLastPublishedOUTMsg(""),
        mBufferedConnectionStatus(MQTT_DISCONNECTED)
    {

        //only for preventing a "uninitialised value" bug
        mClient.pid_lfsr = 0;
        mReconnectState.hostname = mBrokerAddress.c_str();
        static std::string portString = std::to_string(port);
        mReconnectState.port = portString.c_str();
        mReconnectState.topicIN = mTopicIN.c_str();
        static uint8_t sendbuf[2048];
        static uint8_t recvbuf[1024];
        mReconnectState.sendbuf = sendbuf;
        mReconnectState.sendbufsz = sizeof(sendbuf);
        mReconnectState.recvbuf = recvbuf;
        mReconnectState.recvbufsz = sizeof(recvbuf);
        mReconnectState.mqttLaavClient = this;
        mClient.publish_response_callback_state = this;
        mqtt_init_reconnect(&mClient, reconnectAndSubscribeClient, 
                            &mReconnectState, publishCallback);
    }

    enum MQTTConnectionStatus status() const
    {
        return mBufferedConnectionStatus;
    }
    
    /*!
    *  \exception MQTTLAAVMsgException
    */ 
    MQTTLAAVMsg bufferedPublishedINMsg() const
    {
        std::string tokenToCut;
        std::string resourceId;
        MQTTLAAVMsg returnMsg;
        std::string bufferedMsg = mLastPublishedINMsg;
        returnMsg.rawStr = mLastPublishedINMsg;
        std::vector <std::string> msgTokens = split(bufferedMsg, SEPARATOR);
        
        if (bufferedMsg.empty())
            throw MQTTLAAVMsgException("[MQTTLaavClient::lastPublishedINMsg()] " \
            "Can't get msgs on subscribed topic");
        if (msgTokens.size() < 3)
            throw MQTTLAAVMsgException("[MQTTLaavClient::lastPublishedINMsg()] " \
            "Bad msg: "+bufferedMsg);
        
        returnMsg.resourceId = msgTokens[0];
        returnMsg.what = msgTokens[1];
        
        bool ok = false;        
        
        if (bufferedMsg.find(MQTTLAAV_BITRATE)!= std::string::npos)
        {
            try
            {
                returnMsg.valInt = std::stoi(msgTokens[2]);
                returnMsg.valStr = std::to_string(returnMsg.valInt);
                ok = true;
            }
            catch (const std::exception& e) {}
        } 
        
        if (bufferedMsg.find(MQTTLAAV_REC)          != std::string::npos ||
            bufferedMsg.find(MQTTLAAV_AVDEV_GRABBING)!= std::string::npos)
        {
            try
            {
                int val = std::stoi(msgTokens[2]);
                if (val == 0 || val == 1)
                {
                    returnMsg.valInt = val;
                    returnMsg.valStr = std::to_string(returnMsg.valInt);
                    ok = true;
                }
            }
            catch (const std::exception& e) {}
        }
        
        if(!ok)
            throw MQTTLAAVMsgException("[MQTTLaavClient::lastPublishedINMsg()] " \
            "Bad msg: "+bufferedMsg);        
        
        return returnMsg;

    }

    ~MQTTLAAVClient()
    {
        if (mClient.socketfd != -1)
            close(mClient.socketfd);
    }
    
    
    bool hasBufferedPublishedINMsg() const
    {
        return !mLastPublishedINMsg.empty();
    }

    void clearPublishedINMsgBuffer()
    {   
        mLastPublishedINMsg = "";
    }

    void publishOUTMsg(const MQTTLAAVMsg& outMsg)
    {
        if (mBufferedConnectionStatus != MQTT_CONNECTED)
            return;
        
        char outMsgCharArr[256];
        std::string outMsgStr = outMsg.resourceId + SEPARATOR + outMsg.what + SEPARATOR + std::to_string(outMsg.valInt);
        strncpy(outMsgCharArr, outMsgStr.c_str(), sizeof(outMsgCharArr));
        outMsgCharArr[sizeof(outMsgCharArr) - 1] = 0;            
        mqtt_publish(&mClient, mTopicOUT.c_str(), 
                     outMsgCharArr, strlen(outMsgCharArr) + 1, 
                     MQTT_PUBLISH_QOS_0);
    }

    void refresh()
    {
        mqtt_sync(&mClient);
    }
    
private:

    static void publishCallback(void** opaque, struct mqtt_response_publish *published)  
    {
        MQTTLAAVClient* laavClient = (MQTTLAAVClient* )(*opaque);
    	char* topicName = (char*) malloc(published->topic_name_size + 1);
    	memcpy(topicName, published->topic_name, published->topic_name_size);
    	topicName[published->topic_name_size] = '\0';
    	std::string msg = (const char*) published->application_message;
    	msg.resize(published->application_message_size);
    	laavClient->mLastPublishedINMsg = msg;
#ifdef MQTT_DEBUG
        std::cerr << "[MQTTLAAVClient:publishCallback(...)]: Got \"" << 
        msg << "\" on topic \"" <<
        topicName << "\"" << std::endl;
#endif
    	free(topicName);
    }

    static void reconnectAndSubscribeClient(struct mqtt_client* client, void **reconnectStateVptr)
    {
        struct reconnect_state_t *reconnectState = *((struct reconnect_state_t**) reconnectStateVptr);
        // Close the clients socket if this isn't the initial reconnect call
        if (client->error != MQTT_ERROR_INITIAL_RECONNECT) 
        {
            close(client->socketfd);
#ifdef MQTT_DEBUG
            std::cerr << "[MQTTLAAVClient::reconnectClient(...)] " << 
            "Attempting to reconnect while client was in error state " << 
            mqtt_error_str(client->error) << std::endl;
#endif
        }

        // Open a new socket. 
    	int sockfd = reconnectState->mqttLaavClient->openNonBlockingSocket
                     (reconnectState->hostname, reconnectState->port);
        
        if (sockfd == -1) 
                printAndThrowUnrecoverableError("openNonBlockingSocket(...);");

        // Reinitialize the client.
        mqtt_reinit(client, sockfd, 
                    reconnectState->sendbuf, reconnectState->sendbufsz,
                    reconnectState->recvbuf, reconnectState->recvbufsz);	  

        // Send connection request to the broker. 
        mqtt_connect(client, "subscribing_client", NULL, NULL, 0, NULL, NULL, 0, 60);

        // Subscribe to the topic.
        mqtt_subscribe(client, reconnectState->topicIN, 0);

        mqtt_sync(client);
            
        if (client->error == MQTT_OK)
            reconnectState->mqttLaavClient->mBufferedConnectionStatus = MQTT_CONNECTED;
        else
            reconnectState->mqttLaavClient->mBufferedConnectionStatus = MQTT_DISCONNECTED;

#ifdef MQTTEBUG
        std::cerr << "[MQTTLAAVClient::reconnectClient(...)] " << 
        "Executed mqtt_connect(...): " << mqtt_error_str(client->error) << std::endl;
#endif
    }
         
    int openNonBlockingSocket(const char* addr, const char* port) 
    {
        struct addrinfo addInfo = {0};

        addInfo.ai_family = AF_UNSPEC; 
        addInfo.ai_socktype = SOCK_STREAM;
        int sockfd = -1;
        int retVal;
        struct addrinfo *p, *servInfo;
        retVal = getaddrinfo(addr, port, &addInfo, &servInfo);
        
        if (retVal != 0) 
        {
            fprintf(stderr, "Failed to open socket (getaddrinfo): %s\n", gai_strerror(retVal));
            return -1;
        }
        for (p = servInfo; p != NULL; p = p->ai_next) 
        {
            sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sockfd == -1) continue;

#ifdef LINUX            
            int keepalive = 1;	// 1=KeepAlive On, 0=KeepAlive Off. */
            int reuseAddr = 1;
            int reusePort = 1;
            int idle = 10;	// Number of idle seconds before sending a KeepAlive probe. 
            int interval = 5;	// How often in seconds to resend an unacked KeepAlive probe. 
            int count = 3;	// How many times to resend a KA probe if previous probe was unacked.  
            
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr)) < 0)
            { printAndThrowUnrecoverableError("setsockopt(... SO_REUSEADDR ..."); }
            
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &reusePort, sizeof(reusePort)) < 0)
            { printAndThrowUnrecoverableError("setsockopt(... SO_REUSEPORT ..."); }
            
            if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0)
            { printAndThrowUnrecoverableError("setsockopt(... SO_KEEPALIVE ..."); }
            
            if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) < 0)
            { printAndThrowUnrecoverableError("setsockopt(... TCP_KEEPIDLE ..."); }

            // Set how often in seconds to resend an unacked KA probe.
            if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) < 0)
            { printAndThrowUnrecoverableError("setsockopt(... TCP_KEEPINTVL ..."); }

            // Set how many times to resend a KA probe if previous probe was unacked. 
            if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) < 0)
            { printAndThrowUnrecoverableError("setsockopt(... TCP_KEEPCNT ..."); }
#endif                    
            // connect to server 
            retVal = connect(sockfd, servInfo->ai_addr, servInfo->ai_addrlen);
            if (retVal == -1) 
                continue;
            break;
        }  

        freeaddrinfo(servInfo);

        if (sockfd != -1) 
            fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);

        return sockfd;  
    }
    
    const std::string mBrokerAddress; 
    const int mPort;
    const std::string mTopicIN;
    const std::string mTopicOUT;     
    const std::string mUser; 
    const std::string mPassword;
    struct reconnect_state_t mReconnectState;
    struct mqtt_client mClient;
    std::string mLastPublishedINMsg;
    std::string mLastPublishedOUTMsg;   
    MQTTConnectionStatus mBufferedConnectionStatus;

};

}

#endif

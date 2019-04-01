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
 */

#ifndef COMMON_HPP_INCLUDED
#define COMMON_HPP_INCLUDED

#include "Medium.hpp"
#include <tuple>
#include <type_traits>
#include <vector>
#include <typeinfo>
#include <string.h>
#include <iostream>

#ifdef LINUX
    #include <signal.h>
#endif
#ifdef __GNUG__
    #include <cstdlib>
    #include <memory>
    #include <cxxabi.h>
#endif

namespace laav
{

#ifdef __GNUG__

#define printAndThrowUnrecoverableError(x) printAndThrowUnrecoverableError_(x, std::runtime_error{__PRETTY_FUNCTION__})

void printAndThrowUnrecoverableError_(const std::string& error, const std::runtime_error& func)
{
    std::cerr << error << "\n";
    throw func;
}

#endif

unsigned int encodedVideoFrameBufferSize = 100;
unsigned int encodedAudioFrameBufferSize = 100;

unsigned int DEFAULT_BITRATE = -1;
unsigned int DEFAULT_GOPSIZE = -1;
unsigned int DEFAULT_SAMPLERATE = 0;
unsigned int DEFAULT_FRAMERATE = -1;

enum DeviceStatus
{
    OPEN_DEV_ERROR,
    CONFIGURE_DEV_ERROR,
    CLOSE_DEV_ERROR,
    DEV_INITIALIZING,
    DEV_CONFIGURED,
    DEV_CAN_GRAB,
    DEV_DISCONNECTED
};

class OutOfBounds {};

class Void {};

#ifdef LINUX
void ignoreSigpipe()
{
    // THIS IS NEEDED FOR GDB and VALGRIND.
    // ignore SIGPIPE (or else it will bring our program down if the client
    // closes its socket).
    // NB: if running under gdb, you might need to issue this gdb command:
    //          handle SIGPIPE nostop noprint pass
    //     because, by default, gdb will stop our program execution (which we
    //     might not want).
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;

    if (sigemptyset(&sa.sa_mask) < 0 || sigaction(SIGPIPE, &sa, 0) < 0)
    {
        printAndThrowUnrecoverableError
        ("sigemptyset(&sa.sa_mask) < 0 || sigaction(SIGPIPE, &sa, 0) (could not handle SIGPIPE)");
    }
}
#endif

void printCurrDate()
{
#ifdef LINUX    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    char day[4], mon[4];
    int wday, hh, mm, ss, year;
    sscanf(ctime((time_t*) & (ts.tv_sec)), "%s %s %d %d:%d:%d %d",
           day, mon, &wday, &hh, &mm, &ss, &year);
    fprintf(stderr, "[ %s %s %d %d:%d:%d %ld ]", day, mon, wday, hh, mm, ss, ts.tv_nsec);
#endif
}

static unsigned LAAVLogLevel = 0;

class LAAVLogger 
{

class LAAVLoggerPrv
{
    public:
        
    LAAVLoggerPrv(unsigned instanceLogLevel) : mInstanceLogLevel(instanceLogLevel) {}  
    
    template <typename T>
    LAAVLoggerPrv& operator<<(T& t)
    {
        if (mInstanceLogLevel <= LAAVLogLevel)
        {
            std::cerr << t;
        }
        return *this;
    }    

    LAAVLoggerPrv& operator<<(std::ostream& (*func)(std::ostream&))
    {
        if (mInstanceLogLevel <= LAAVLogLevel)
        {
            std::cerr << func;
        }        
        return *this;
    }  
    private:
    
    unsigned mInstanceLogLevel;    
};    
    
public: 

LAAVLogger(unsigned instanceLogLevel) : 
    mLoggerPrv(instanceLogLevel),
    mInstanceLogLevel(instanceLogLevel) 
    {}    
    
template <typename T>
LAAVLoggerPrv& operator<<(T& t)
{
    if (mInstanceLogLevel <= LAAVLogLevel)
    {
        printCurrDate();
        std::cerr << " [LAAVLogger] " << t;
    }
    return mLoggerPrv;
}    

LAAVLoggerPrv& operator<<(std::ostream& (*func)(std::ostream&))
{
    if (mInstanceLogLevel <= LAAVLogLevel)
    {
        printCurrDate();
        std::cerr << " [LAAVLogger] " << func;
    }    
    return mLoggerPrv;
}  



private:
    
    LAAVLoggerPrv mLoggerPrv;
    unsigned mInstanceLogLevel;
    
};

LAAVLogger loggerLevel1(1);
LAAVLogger loggerLevel2(2);
LAAVLogger loggerLevel3(3);
LAAVLogger loggerLevel4(4);

std::vector<std::string> split(const std::string &text, char sep)
{
    std::vector<std::string> tokens;
    std::size_t start = 0, end = 0;
    while ((end = text.find(sep, start)) != std::string::npos)
    {
        tokens.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    tokens.push_back(text.substr(start));
    return tokens;
}

std::vector<std::string> split (const std::string& s, const std::string& delimiter) {
    size_t posStart = 0, posEnd, delimLen = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((posEnd = s.find (delimiter, posStart)) != std::string::npos) {
        token = s.substr (posStart, posEnd - posStart);
        posStart = posEnd + delimLen;
        res.push_back (token);
    }

    res.push_back (s.substr (posStart));
    return res;
}

typedef std::shared_ptr<unsigned char> ShareableVideoFrameData;
typedef std::shared_ptr<unsigned char> ShareableAudioFrameData;
typedef std::shared_ptr<unsigned char> ShareableMuxedData;

class MPEGTS {};
class MATROSKA {};

typedef std::integral_constant<unsigned int,0> MONO;
typedef std::integral_constant<unsigned int,1> STEREO;
enum class AudioChannels {MONO, STEREO};

template <int Val>
struct UnsignedConstant : public std::integral_constant<unsigned int,Val> {};

template <typename T>
std::string constantToString();

class OPUS;
template <>
std::string constantToString<OPUS>()
{
    return "OPUS";
}

class MP2;
template <>
std::string constantToString<MP2>()
{
    return "MP2";
}

class ADTS_AAC;
template <>
std::string constantToString<ADTS_AAC>()
{
    return "ADTS_AAC";
}

class H264;
template <>
std::string constantToString<H264>()
{
    return "H264";
}

class MJPEG;
template <>
std::string constantToString<MJPEG>()
{
    return "MJPEG";
}

class MPEGTS;
template <>
std::string constantToString<MPEGTS>()
{
    return "MPEGTS";
}

class MATROSKA;
template <>
std::string constantToString<MATROSKA>()
{
    return "MATROSKA";
}

}

#endif // COMMON_HPP_INCLUDED


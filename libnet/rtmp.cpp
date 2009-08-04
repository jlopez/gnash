// rtmp.cpp:  Adobe/Macromedia Real Time Message Protocol handler, for Gnash.
// 
//   Copyright (C) 2005, 2006, 2007, 2008, 2009 Free Software Foundation, Inc.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//

#ifdef HAVE_CONFIG_H
#include "gnashconfig.h"
#endif

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <cerrno>
#include <boost/detail/endian.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/format.hpp>

#if ! (defined(_WIN32) || defined(WIN32))
#	include <netinet/in.h>
#endif

#include "log.h"
#include "amf.h"
#include "rtmp.h"
#include "cque.h"
#include "network.h"
#include "element.h"
// #include "handler.h"
#include "utility.h"
#include "buffer.h"
#include "GnashSleep.h"

using std::cerr;
using std::endl;
using namespace amf;

namespace gnash
{

namespace {

    /// Function object for matching C strings alphabetically.
    class MatchFirst
    {
    public:
        MatchFirst(const char* match) : _match(match) {}

        bool operator()(const RTMP::AMFProperties::value_type& a) {
            return std::strcmp(a.first, _match) == 0;
        }
    private:
        const char* _match;
    };

}


CQue incoming;

// extern std::map<int, Handler *> handlers;

const char *content_str[] = {
    "None",
    "Chunk Size",
    "Unknown",
    "Bytes Read",
    "Ping",
    "Server",
    "Client",
    "Unknown2",
    "Audio Data",
    "Video Data",
    "Unknown3",
    "Blank 0xb",
    "Blank 0xc",
    "Blank 0xd",
    "Blank 0xe",
    "Blank 0xf",
    "Blank 0x10",
    "Blank 0x11",
    "Notify",
    "Shared object",
    "Invoke",
    "FLV Data"
};

const char *ping_str[] = {
    "PING_CLEAR",
    "PING_PLAY",
    "Unknown Ping 2",
    "PING_TIME",
    "PING_RESET",
    "Unknown Ping 2",
    "PING_CLIENT",
    "PONG_CLIENT"
};

const char *status_str[] = {
    "APP_GC",
    "APP_RESOURCE_LOWMEMORY",
    "APP_SCRIPT_ERROR",
    "APP_SCRIPT_WARNING",
    "APP_SHUTDOWN",
    "NC_CALL_BADVERSION",
    "NC_CALL_FAILED",
    "NC_CONNECT_APPSHUTDOWN",
    "NC_CONNECT_CLOSED",
    "NC_CONNECT_FAILED",
    "NC_CONNECT_INVALID_APPLICATION",
    "NC_CONNECT_REJECTED",
    "NC_CONNECT_SUCCESS",
    "NS_CLEAR_FAILED",
    "NS_CLEAR_SUCCESS",
    "NS_DATA_START",
    "NS_FAILED",
    "NS_INVALID_ARGUMENT",
    "NS_PAUSE_NOTIFY",
    "NS_PLAY_COMPLETE",
    "NS_PLAY_FAILED",
    "NS_PLAY_FILE_STRUCTURE_INVALID",
    "NS_PLAY_INSUFFICIENT_BW",
    "NS_PLAY_NO_SUPPORTED_TRACK_FOUND",
    "NS_PLAY_PUBLISHNOTIFY",
    "NS_PLAY_RESET",
    "NS_PLAY_START",
    "NS_PLAY_STOP",
    "NS_PLAY_STREAMNOTFOUND",
    "NS_PLAY_SWITCH",
    "NS_PLAY_UNPUBLISHNOTIFY",
    "NS_PUBLISH_BADNAME",
    "NS_PUBLISH_START",
    "NS_RECORD_FAILED",
    "NS_RECORD_NOACCESS",
    "NS_RECORD_START",
    "NS_RECORD_STOP",
    "NS_SEEK_FAILED",
    "NS_SEEK_NOTIFY",
    "NS_UNPAUSE_NOTIFY",
    "NS_UNPUBLISHED_SUCCESS",
    "SO_CREATION_FAILED",
    "SO_NO_READ_ACCESS",
    "SO_NO_WRITE_ACCESS",
    "SO_PERSISTENCE_MISMATCH"
};


// These are the textual responses
const char *response_str[] = {
    "/onStatus",
    "/onResult",
    "/onDebugEvents"
};

int
RTMP::headerSize(boost::uint8_t header)
{
//    GNASH_REPORT_FUNCTION;
    
    int headersize = -1;

//    cerr << "Header size value: " << (void *)header << std::endl;
    
    switch (header & RTMP_HEADSIZE_MASK) {
      case HEADER_12:
          headersize = 12;
          break;
      case HEADER_8:
          headersize = 8;
          break;
      case HEADER_4:
          headersize = 4;
          break;
      case HEADER_1:
          headersize = 1;
          break;
      default:
          log_error(_("AMF Header size bits (0x%X) out of range"),
          		header & RTMP_HEADSIZE_MASK);
          headersize = 1;
          break;
    };

    return headersize;
}

RTMP::RTMP() 
    : _handshake(0),
//       _handler(0),
      _packet_size(0),
      _mystery_word(0),
      _timeout(1)
{
//    GNASH_REPORT_FUNCTION;

    // Initialize all of the queues
    for (int i=0; i<MAX_AMF_INDEXES; i++)
    {
        // Name is only used for debugging
        boost::format fmt("channel #%s");
        std::string name = (fmt % i).str();
	    _queues[i].setName(name.c_str());

        // each channel can have a different chunksize
	    _chunksize[i] = RTMP_VIDEO_PACKET_SIZE;
	    _lastsize[i] = 0;
    }
}

RTMP::~RTMP()
{
//    GNASH_REPORT_FUNCTION;
    _properties.clear();
    delete _handshake;
//     delete _handler;

//    delete _body;
}

void
RTMP::addProperty(amf::Element &el)
{
//    GNASH_REPORT_FUNCTION;
    _properties[el.getName()] = el;
}

void
RTMP::addProperty(char *name, amf::Element &el)
{ 
//    GNASH_REPORT_FUNCTION;
    _properties[name] = el;
}


amf::Element &
RTMP::getProperty(const std::string &name)
{
    // Find without inserting.
    AMFProperties::iterator it = std::find_if(_properties.begin(),
            _properties.end(), MatchFirst(name.c_str()));

    // If this fails and we return, it will corrupt memory, so either
    // the assertion never fails or we'll have to return a pointer.
    assert(it != _properties.end()); 
    return it->second;
}

boost::shared_ptr<RTMP::rtmp_head_t>
RTMP::decodeHeader(amf::Buffer &buf)
{
//    GNASH_REPORT_FUNCTION;
    return decodeHeader(buf.reference());
}

boost::shared_ptr<RTMP::rtmp_head_t>
RTMP::decodeHeader(boost::uint8_t *in)
{
//    GNASH_REPORT_FUNCTION;

    boost::shared_ptr<RTMP::rtmp_head_t> head(new RTMP::rtmp_head_t);
    boost::uint8_t *tmpptr = in;

    head->channel = *tmpptr & RTMP_INDEX_MASK;
//     log_debug (_("The AMF channel index is %d"), head->channel);
    
    head->head_size = headerSize(*tmpptr++);
//     log_debug (_("The header size is %d"), head->head_size);

    if (head->head_size > RTMP_MAX_HEADER_SIZE) {
	head.reset();
	return head;
    }
    
//     cerr << "FIXME3: " << hexify(in, head->head_size, false) << endl;    
    
    if (head->head_size >= 4) {
        _mystery_word = *tmpptr++;
        _mystery_word = (_mystery_word << 8) + *tmpptr++;
        _mystery_word = (_mystery_word << 8) + *tmpptr++;

//         log_debug(_("The mystery word is: %d"), _mystery_word);
    }

    if (head->head_size >= 8) {
        head->bodysize = *tmpptr++;
        head->bodysize = (head->bodysize << 8) + *tmpptr++;
        head->bodysize = (head->bodysize << 8) + *tmpptr++;
        head->bodysize = head->bodysize & 0xffffff;
//         log_debug(_("The body size is: %d"), head->bodysize);
    }

    if (head->head_size >= 8) {
	boost::uint8_t byte = *tmpptr;
        head->type = (content_types_e)byte;
        tmpptr++;
// 	if (head->type <= RTMP::INVOKE ) {
// 	    log_debug(_("The type is: %s"), content_str[head->type]);
// 	} else {
// 	    log_debug(_("The type is: 0x%x"), head->type);
// 	}
    }

    if (head->head_size == 1) {
	if (head->channel == RTMP_SYSTEM_CHANNEL) {
	    head->bodysize = sizeof(boost::uint16_t) * 3;
// 	    log_debug("Got a one byte header system message: %s", hexify(in, head->bodysize, false));
	} else {
// 	    log_debug("Got a continuation packet for channel #%d", head->channel);
	    head->bodysize = _lastsize[head->channel];
	}
    }

    log_debug("RTMP %s: channel: %d, header_size %d, bodysize: %d",
  	      ((head->head_size == 1) ? "same" : content_str[head->type]),
	      head->channel, head->head_size, head->bodysize);

    if (head->head_size == 12) {
        head->src_dest = *(reinterpret_cast<RTMPMsg::rtmp_source_e *>(tmpptr));
        tmpptr += sizeof(unsigned int);
//         log_debug(_("The source/destination is: %x"), head->src_dest);
    }

    return head;

}

/// \brief \ Each RTMP header consists of the following:
///
/// * Index & header size - The header size and amf channel index.
/// * Total size - The total size of the message
/// * Type - The type of the message
/// * Routing - The source/destination of the message
//

boost::shared_ptr<amf::Buffer> 
RTMP::encodeHeader(int amf_index, rtmp_headersize_e head_size)
{
//    GNASH_REPORT_FUNCTION;
    boost::shared_ptr<amf::Buffer> buf(new Buffer(1));
    boost::uint8_t *ptr = buf->reference();
    
    // Make the channel index & header size byte
    *ptr = head_size & RTMP_HEADSIZE_MASK;  
    *ptr += amf_index  & RTMP_INDEX_MASK;

    return buf;
}

// There are 3 size of RTMP headers, 1, 4, 8, and 12.
boost::shared_ptr<amf::Buffer> 
RTMP::encodeHeader(int amf_index, rtmp_headersize_e head_size,
		       size_t total_size, content_types_e type,
		       RTMPMsg::rtmp_source_e routing)
{
//    GNASH_REPORT_FUNCTION;

    boost::shared_ptr<amf::Buffer> buf;
    switch(head_size) {
      case HEADER_1:
	  buf.reset(new Buffer(1));
	  break;
      case HEADER_4:
	  buf.reset(new Buffer(4));
	  break;
      case HEADER_8:
	  buf.reset(new Buffer(8));
	  break;
      case HEADER_12:
	  buf.reset(new Buffer(12));
	  break;
    }
    boost::uint8_t *ptr = buf->reference();
    
    // Make the channel index & header size byte
//    *ptr = head_size & RTMP_HEADSIZE_MASK;
    *ptr = head_size; // & RTMP_INDEX_MASK;
    *ptr += amf_index  & RTMP_INDEX_MASK;
    ptr++;

    // Add the unknown bytes. These seem to be used by video and
    // audio, and only when the header size is 4 or more.
    if ((head_size == HEADER_4) || (head_size == HEADER_8) || (head_size == HEADER_12)) {
	memset(ptr, 0, 3);
	ptr += 3;
    }
    
    // Add the size of the message if the header size is 8 or more.
    // and add the type of the object if the header size is 8 or more.
    // length is a 3 byte field
    if ((head_size == HEADER_8) || (head_size == HEADER_12)) {
	*ptr++ = (total_size >> 16) & 0xff;
	*ptr++ = (total_size >> 8) & 0xff;
	*ptr++ = total_size & 0xff;
	// The type is a one byte field
	*ptr = type;
	ptr++;
    }
    
    // Add the routing of the message if the header size is 12, the maximum.
    if (head_size == HEADER_12) {
        boost::uint32_t swapped = htonl(routing);
        memcpy(ptr, &swapped, 4);
        ptr += 4;
    }
    
    // Manually adjust the seek pointer since we added the data by
    // walking ou own temporary pointer, so none of the regular ways
    // of setting the seek pointer are appropriate.
    buf->setSeekPointer(buf->reference() + buf->size());
    
    return buf;
}

#if 0
bool
RTMP::packetRead(amf::Buffer &buf)
{
    GNASH_REPORT_FUNCTION;

//    int packetsize = 0;
    size_t amf_index, headersize;
    boost::uint8_t *ptr = buf.reference();
    boost::uint8_t *tooFar = ptr+buf.size();
    AMF amf;

    ptr += 1;			// skip past the RTMP header byte
    
    amf_index = *ptr & RTMP_INDEX_MASK;
    headersize = headerSize(*buf.reference());
    log_debug (_("The Header size is: %d"), headersize);
    log_debug (_("The AMF index is: 0x%x"), amf_index);

     if (headersize > 1) {
	 RTMP::rtmp_head_t *rthead = decodeHeader(ptr);
//         if (packetsize) {
//             log_debug (_("Read first RTMP packet header of size %d"), packetsize);
//         } else {
//             log_error (_("Couldn't read first RTMP packet header"));
//             return false;
//         }
     }

#if 1
    boost::uint8_t *end = buf.remove(0xc3);
#else
    boost::uint8_t *end = buf.find(0xc3);
    log_debug("END is %x", (void *)end);
    *end = '*';
#endif
    
//    ptr = decodeHeader(ptr);
//    ptr += headersize;
    
    boost::shared_ptr<amf::Element> el = amf.extractAMF(ptr, tooFar);
//    el->dump();
    el = amf.extractAMF(ptr, tooFar); // @@strk@@ : what's the +1 for ?
//    el->dump();
    log_debug (_("Reading AMF packets till we're done..."));
//    buf->dump();
    while (ptr < end) {
	boost::shared_ptr<amf::Element> el = amf.extractProperty(ptr, tooFar);
	addProperty(el);
//	el->dump();
    }
    ptr += 1;
    size_t actual_size = static_cast<size_t>(_header.bodysize - AMF_HEADER_SIZE);
    log_debug("Total size in header is %d, buffer size is: %d", _header.bodysize, buf.size());
//    buf->dump();
    if (buf.size() < actual_size) {
	log_debug("FIXME: MERGING");
//	buf = _handler->merge(buf); FIXME needs to use shared_ptr
    }
    while ((ptr - buf.begin()) < static_cast<int>(actual_size)) {
	boost::shared_ptr<amf::Element> el = amf.extractProperty(ptr, tooFar);
	addProperty(el);
//	el->dump();		// FIXME: dump the AMF objects as they are read in
    }

//    dump();
    
    boost::shared_ptr<amf::Element> url = getProperty("tcUrl");
    boost::shared_ptr<amf::Element> file = getProperty("swfUrl");
    boost::shared_ptr<amf::Element> app = getProperty("app");
    
    if (file) {
	log_debug("SWF file %s", file->to_string());
    }
    if (url) {
	log_debug("is Loading video %s", url->to_string());
    }
    if (app) {
	log_debug("is file name is %s", app->to_string());
    }
    
    return true;
}
#endif

void
RTMP::dump()
{
    cerr << "RTMP packet contains " << _properties.size() << " variables." << endl;
    AMFProperties::iterator it;
    for (it = _properties.begin(); it != _properties.end(); it++) {
//	const char *name = it->first;
	amf::Element el = it->second;
	el.dump();
    }
}

// A Ping packet has two parameters that are always specified, and 2 that are optional.
// The first two bytes are the ping type, as in rtmp_ping_e, the second is the ping
// target, which is always zero as far as we can tell.
//
// More notes from: http://jira.red5.org/confluence/display/docs/Ping
// type 0: Clear the stream. No third and fourth parameters. The second parameter could be 0.
// After the connection is established, a Ping 0,0 will be sent from server to client. The
// message will also be sent to client on the start of Play and in response of a Seek or
// Pause/Resume request. This Ping tells client to re-calibrate the clock with the timestamp
// of the next packet server sends.
// type 1: Tell the stream to clear the playing buffer.
// type 3: Buffer time of the client. The third parameter is the buffer time in millisecond.
// type 4: Reset a stream. Used together with type 0 in the case of VOD. Often sent before type 0.
// type 6: Ping the client from server. The second parameter is the current time.
// type 7: Pong reply from client. The second parameter is the time the server sent with his
//         ping request.

// A RTMP Ping packet looks like this: "02 00 00 00 00 00 06 04 00 00 00 00 00 00 00 00 00 0",
// which is the Ping type byte, followed by two shorts that are the parameters. Only the first
// two paramters are required.
// This seems to be a ping message, 12 byte header, system channel 2
// 02 00 00 00 00 00 06 04 00 00 00 00 00 00 00 00 00 00
boost::shared_ptr<RTMP::rtmp_ping_t>
RTMP::decodePing(boost::uint8_t *data)
{
//    GNASH_REPORT_FUNCTION;
    
    boost::uint8_t *ptr = reinterpret_cast<boost::uint8_t *>(data);
    boost::shared_ptr<rtmp_ping_t> ping(new rtmp_ping_t);

    // All the data fields in a ping message are 2 bytes long.
    boost::uint16_t type = ntohs(*reinterpret_cast<boost::uint16_t *>(ptr));
    ping->type = static_cast<rtmp_ping_e>(type);
    ptr += sizeof(boost::uint16_t);

    ping->target = ntohs(*reinterpret_cast<boost::uint16_t *>(ptr));
    ptr += sizeof(boost::uint16_t);
    
    ping->param1 = ntohs(*reinterpret_cast<boost::uint16_t *>(ptr));
    ptr += sizeof(boost::uint16_t);
    
//     ping->param2 = ntohs(*reinterpret_cast<boost::uint16_t *>(ptr));
//     ptr += sizeof(boost::uint16_t);

//    ping->param3 = ntohs(*reinterpret_cast<boost::uint16_t *>(ptr));
    ping->param3 = 0;

    return ping;    
}
boost::shared_ptr<RTMP::rtmp_ping_t>
RTMP::decodePing(amf::Buffer &buf)
{
//    GNASH_REPORT_FUNCTION;
    return decodePing(buf.reference());
}

// Decode the result we get from the server after we've made a request.
//
// 03 00 00 00 00 00 81 14 00 00 00 00 02 00 07 5f  ..............._
// 72 65 73 75 6c 74 00 3f f0 00 00 00 00 00 00 05  result.?........
// 03 00 0b 61 70 70 6c 69 63 61 74 69 6f 6e 05 00  ...application..
// 05 6c 65 76 65 6c 02 00 06 73 74 61 74 75 73 00  .level...status.
// 0b 64 65 73 63 72 69 70 74 69 6f 6e 02 00 15 43  .description...C
// 6f 6e 6e 65 63 74 69 6f 6e 20 73 75 63 63 65 65  onnection succee
// 64 65 64 2e 00 04 63 6f 64 65 02 00 1d 4e 65 74  ded...code...Net
// 43 6f 6e 6e 65 63 74 69 6f 6e 2e 43 6f 6e 6e 65  Connection.Conne
// 63 74 2e 53 75 63 63 65 73 73 00 00 c3 09        ct.Success....
//
// 43 00 00 00 00 00 48 14 02 00 06 5f 65 72 72 6f  C.....H...._erro
// 72 00 40 00 00 00 00 00 00 00 05 03 00 04 63 6f  r.@...........co
// 64 65 02 00 19 4e 65 74 43 6f 6e 6e 65 63 74 69  de...NetConnecti
// 6f 6e 2e 43 61 6c 6c 2e 46 61 69 6c 65 64 00 05  on.Call.Failed..
// 6c 65 76 65 6c 02 00 05 65 72 72 6f 72 00 00 09  level...error...
//
// T 127.0.0.1:1935 -> 127.0.0.1:38167 [AP]
// 44 00 00 00 00 00 b2 14 02 00 08 6f 6e 53 74 61  D..........onSta
// 74 75 73 00 3f f0 00 00 00 00 00 00 05 03 00 08  tus.?...........
// 63 6c 69 65 6e 74 69 64 00 3f f0 00 00 00 00 00  clientid.?......
// 00 00 05 6c 65 76 65 6c 02 00 06 73 74 61 74 75  ...level...statu
// 73 00 07 64 65 74 61 69 6c 73 02 00 16 6f 6e 32  s..details...on2
// 5f 66 6c 61 73 68 38 5f 77 5f 61 75 64 69 6f 2e  _flash8_w_audio.
// 66 6c 76 00 0b 64 65 73 63 72 69 70 74 69 6f 6e  flv..description
// 02 00 27 53 74 61 72 74 65 64 20 70 6c 61 79 69  ..'Started playi
// 6e 67 20 6f 6e 32 5f 66 c4 6c 61 73 68 38 5f 77  ng on2_f.lash8_w
// 5f 61 75 64 69 6f 2e 66 6c 76 2e 00 04 63 6f 64  _audio.flv...cod
// 65 02 00 14 4e 65 74 53 74 72 65 61 6d 2e 50 6c  e...NetStream.Pl
// 61 79 2e 53 74 61 72 74 00 00 09                 ay.Start...
//
// ^^^_result^?^^^^^^^^^^^application^^^level^^^status^^description^^^Connection succeeded.^^code^^^NetConnection.Connect.Success^^^^
// 02 00 07 5f 72 65 73 75 6c 74 00 3f f0 00 00 00 00 00 00 05 03 00 0b 61 70 70 6c 69 63 61 74 69 6f 6e 05 00 05 6c 65 76 65 6c 02 00 06 73 74 61 74 75 73 00 0b 64 65 73 63 72 69 70 74 69 6f 6e 02 00 15 43 6f 6e 6e 65 63 74 69 6f 6e 20 73 75 63 63 65 65 64 65 64 2e 00 04 63 6f 64 65 02 00 1d 4e 65 74 43 6f 6e 6e 65 63 74 69 6f 6e 2e 43 6f 6e 6e 65 63 74 2e 53 75 63 63 65 73 73 00 00 c3 09 
// 10629:3086592224] 20:01:20 DEBUG: read 29 bytes from fd 3 from port 0
// C^^^^^^^^^^onBWDone^@^^^^^^^^
// 43 00 00 00 00 00 15 14 02 00 08 6f 6e 42 57 44 6f 6e 65 00 40 00 00 00 00 00 00 00 05
boost::shared_ptr<RTMPMsg>
RTMP::decodeMsgBody(boost::uint8_t *data, size_t size)
{
//     GNASH_REPORT_FUNCTION;
    AMF amf_obj;
    boost::uint8_t *ptr = data;
    boost::uint8_t* tooFar = ptr + size;
    bool status = false;
    boost::shared_ptr<RTMPMsg> msg(new RTMPMsg);

    // The first data object is the method name of this object.
    boost::shared_ptr<amf::Element> name = amf_obj.extractAMF(ptr, tooFar);
    if (name) {
	ptr += name->getDataSize() + AMF_HEADER_SIZE; // skip the length bytes too
    } else {
	log_error("Name field of RTMP Message corrupted!");
	msg.reset();
	return msg;
    }

    // The stream ID is the second data object. All messages have these two objects
    // at the minimum.
    boost::shared_ptr<amf::Element> streamid = amf_obj.extractAMF(ptr, tooFar);
    if (streamid) {
	// Most onStatus messages have the stream ID, but the Data Start onStatus
	// message is basically just a marker that an FLV file is coming next.
	if (streamid->getType() == Element::NUMBER_AMF0) {
	    ptr += AMF0_NUMBER_SIZE + 1;
	}
    } else {
	log_error("Stream ID field of RTMP Message corrupted!");
	msg.reset();
	return msg;
    }

    if (name->to_string() != 0) {
	msg->setMethodName(name->to_string());
    }
    
    double swapped = streamid->to_number();
//     swapBytes(&swapped, amf::AMF0_NUMBER_SIZE);
    msg->setStreamID(swapped);

    if ((msg->getMethodName() == "_result") || (msg->getMethodName() == "_error") || (msg->getMethodName() == "onStatus")) {
 	status = true;
    }
    
    // Then there are a series of AMF objects, often a higher level ActionScript object with
    // properties attached.
    while (ptr < tooFar) {
	// These pointers get deleted automatically when the msg object is deleted
        boost::shared_ptr<amf::Element> el = amf_obj.extractAMF(ptr, tooFar);
	ptr += amf_obj.totalsize();
        if (el == 0) {
	    break;
	}
	msg->addObject(el);
 	if (status) {
	    msg->checkStatus(el);
	}
    };
    
    return msg;
}

boost::shared_ptr<RTMPMsg> 
RTMP::decodeMsgBody(amf::Buffer &buf)
{
//    GNASH_REPORT_FUNCTION;
    return decodeMsgBody(buf.reference(), buf.allocated());
}

boost::shared_ptr<amf::Buffer> 
RTMP::encodeChunkSize()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
    return boost::shared_ptr<amf::Buffer>((amf::Buffer*)0);
}

void
RTMP::decodeChunkSize()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
}
    
boost::shared_ptr<amf::Buffer> 
RTMP::encodeBytesRead()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
    return boost::shared_ptr<amf::Buffer>((amf::Buffer*)0);
}

void
RTMP::decodeBytesRead()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
}

boost::shared_ptr<amf::Buffer> 
RTMP::encodeServer()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
    return boost::shared_ptr<amf::Buffer>((amf::Buffer*)0);
}

void 
RTMP::decodeServer()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
}
    
boost::shared_ptr<amf::Buffer> 
RTMP::encodeClient()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
    return boost::shared_ptr<amf::Buffer>((amf::Buffer*)0);
}

void 
RTMP::decodeClient()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
}
    
boost::shared_ptr<amf::Buffer> 
RTMP::encodeAudioData()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
    return boost::shared_ptr<amf::Buffer>((amf::Buffer*)0);
}

void 
RTMP::decodeAudioData()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
}
    
boost::shared_ptr<amf::Buffer> 
RTMP::encodeVideoData()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
    return boost::shared_ptr<amf::Buffer>((amf::Buffer*)0);
}

void 
RTMP::decodeVideoData()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
}
    
boost::shared_ptr<amf::Buffer> 
RTMP::encodeNotify()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
    return boost::shared_ptr<amf::Buffer>((amf::Buffer*)0);
}

void 
RTMP::decodeNotify()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
}
    
boost::shared_ptr<amf::Buffer> 
RTMP::encodeSharedObj()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
    return boost::shared_ptr<amf::Buffer>((amf::Buffer*)0);
}

void 
RTMP::decodeSharedObj()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
}
    
boost::shared_ptr<amf::Buffer> 
RTMP::encodeInvoke()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
    return boost::shared_ptr<amf::Buffer>((amf::Buffer*)0);
}
void 
RTMP::decodeInvoke()
{
    GNASH_REPORT_FUNCTION;
    log_unimpl(__PRETTY_FUNCTION__);
}

// Send a message, usually a single ActionScript object. This message
// may be broken down into a series of packets on a regular byte
// interval. The byte boundary defaults to 128 bytes (video data), but can
// be changed by the ChunkSize() command.
bool
RTMP::sendMsg(int channel, rtmp_headersize_e head_size,
	      size_t total_size, content_types_e type,
	      RTMPMsg::rtmp_source_e routing, amf::Buffer &data)
{
//    GNASH_REPORT_FUNCTION;
    return sendMsg(getFileFd(), channel, head_size, total_size, type, routing, data.reference(), data.allocated());
}

bool
RTMP::sendMsg(int fd, int channel, rtmp_headersize_e head_size,
	      size_t total_size, content_types_e type,
	      RTMPMsg::rtmp_source_e routing, amf::Buffer &data)
{
//    GNASH_REPORT_FUNCTION;
    return sendMsg(fd, channel, head_size, total_size, type, routing, data.reference(), data.allocated());
}


bool
RTMP::sendMsg(int channel, rtmp_headersize_e head_size,
	      size_t total_size, content_types_e type,
	      RTMPMsg::rtmp_source_e routing, boost::uint8_t *data, size_t size)
{
//  GNASH_REPORT_FUNCTION;
    return sendMsg(getFileFd(), channel, head_size, total_size, type, routing, data, size);
}

bool
RTMP::sendMsg(int fd, int channel, rtmp_headersize_e head_size,
	      size_t total_size, content_types_e type,
	      RTMPMsg::rtmp_source_e routing, boost::uint8_t *data, size_t size)
{
//  GNASH_REPORT_FUNCTION;
    int ret = 0;
    
    // We got some bogus parameters
    if (total_size == 0) {
	log_error("Bogus size parameter in %s!", __PRETTY_FUNCTION__);
	return false;
    }
    
    // This builds the full header,which is required as the first part
    // of the packet.
    boost::shared_ptr<amf::Buffer> head = encodeHeader(channel, head_size, total_size,
						       type, routing);
    // When more data is sent than fits in the chunksize for this
    // channel, it gets broken into chunksize pieces, and each piece
    // after the first packet is sent gets a one byte header instead.
    boost::shared_ptr<amf::Buffer> cont_head = encodeHeader(channel, RTMP::HEADER_1);
    size_t partial = _chunksize[channel];
    size_t nbytes = 0;

    // First send the full header, afterwards we only use continuation
    // headers, which are only one byte.
    gnashSleep(100000);
    ret = writeNet(fd, head->reference(), head->size());
    if (ret == -1) {
	log_error("Couldn't write the RTMP header!");
	return false;
    }    

    // now send the data
    while (nbytes <= size) {
	// The last bit of data is usually less than the packet size,
	// so we write less data of course.
	if ((size - nbytes) < static_cast<signed int>(_chunksize[channel])) {
	    partial = size - nbytes;
	}
	// After the first packet, only send the single byte
	// continuation packet.
	if (nbytes > 0) {
	    ret = writeNet(fd, *cont_head);
	}
	// write the data to the client
	ret = writeNet(fd, data + nbytes, partial);
	if (ret == -1) {
	    log_error("Couldn't write the RTMP body!");
	    return false;
	}
	// adjust the accumulator.
	nbytes += _chunksize[channel];
    };
    return true;
}

#if 0
// Send a Msg, and expect a response back of some kind.
RTMPMsg *
RTMP::sendRecvMsg(amf::Buffer &bufin)
{
    GNASH_REPORT_FUNCTION;
//    size_t total_size = buf2->size() - 6; // FIXME: why drop 6 bytes ?
    boost::shared_ptr<amf::Buffer> head = encodeHeader(amf_index, head_size, total_size,
				type, routing);
//    int ret = 0;
    int ret = writeNet(head->reference(), head->size()); // send the header first
//     if (netDebug()) {
// 	head->dump();
// 	bufin->dump();
//     }
    ret = sendMsg(bufin);

    RTMP::rtmp_head_t *rthead = 0;
    RTMPMsg *msg = 0;
    boost::shared_ptr<amf::Buffer> buf;
    boost::uint8_t *ptr = 0;


    buf = recvMsg(1);	// use a 1 second timeout
    if (buf == 0) {
	return 0;
    }
    RTMP::queues_t *que = split(buf);
//    CQue *que = split(buf);
    while (que->size()) {
//	ptr = que->pop();
	cerr << "QUE SIZE: " << que->size() << endl;
	ptr = que->front()->pop()->reference();
	que->pop_front();
//	ptr = buf->reference();
	rthead = decodeHeader(ptr);
	
	if (rthead) {
	    if (rthead->head_size == 1) {
		log_debug("Response header: %s", hexify(ptr, 7, false));
	    } else {
		log_debug("Response header: %s", hexify(ptr, rthead->head_size, false));
	    }
	    if (rthead->type <= RTMP::FLV_DATA) {
		log_error("Processing message of type %s!", content_str[rthead->type]);
	    }
	    
	    switch (rthead->type) {
	      case CHUNK_SIZE:
		  log_debug("Got CHUNK_SIZE packet!!!");
		  _chunksize[rthead->channel] = ntohl(*reinterpret_cast<boost::uint32_t *>(ptr + rthead->head_size));
		  log_debug("Setting packet chunk size to %d.", _chunksize);
//		  decodeChunkSize();
		  break;
	      case BYTES_READ:
		  log_debug("Got Bytes Read packet!!!");
//		  decodeBytesRead();
		  break;
	      case PING:
	      {
 		  RTMP::rtmp_ping_t *ping = decodePing(ptr);
 		  log_debug("FIXME: Ping type is: %d, ignored for now", ping->type);
		  switch (ping->type) {
		    case PING_CLEAR:
			break;
		    case PING_PLAY:
			break;
		    case PING_TIME:
			break;
		    case PING_RESET:
			break;
		    case PING_CLIENT:
			break;
		    case PONG_CLIENT:
			break;
		    default:
			break;
		  };
		  break;
	      }
	      case SERVER:
	      {
		  log_debug("Got SERVER packet!!!");
		  Buffer server_data(rthead->bodysize);
		  server_data.copy(ptr + rthead->head_size, rthead->bodysize);
//		  decodeServer();
		  break;
	      }
	      case CLIENT:
	      {
		  log_debug("Got CLIENT packet!!!");
		  Buffer client_data(rthead->bodysize);
		  client_data.copy(ptr + rthead->head_size, rthead->bodysize);
//		  decodeClient();
		  break;
	      }
	      case VIDEO_DATA:
	      {
		  log_debug("Got VIDEO packets!!!");
		  boost::shared_ptr<amf::Buffer> frame;
		  do {
		      frame = recvMsg(1);	// use a 1 second timeout
		      if (frame) {
			  _queues[rthead->channel].push(frame);
		      }
//		      decodeVideoData();
		  } while (frame);
		  _queues->dump();
		  break;
	      }
	      case NOTIFY:
//		  decodeNotify();
		  break;
	      case SHARED_OBJ:
		  log_debug("Got Shared Object packet!!!");
//		  decodeSharedObj();
		  break;
	      case INVOKE:
		  msg = decodeMsgBody(ptr + rthead->head_size, rthead->bodysize);
//		  msg->dump();
		  if (msg) {
		      log_debug("%s: Msg status is: %d: %s, name is %s, size is %d", __FUNCTION__,
				msg->getStatus(), status_str[msg->getStatus()],
				msg->getMethodName(), msg->size());
		      if (msg->getMethodName() == "onBWDone") {	
			  log_debug("Got onBWDone packet!!!");
			  continue;
		      }
		      return msg;
		  } else {
		      log_error("Couldn't decode message body for type %s!",
				content_str[rthead->type]);
		  }
//		  decodeInvoke();
		  break;
	      case AUDIO_DATA:	  
//		  decodeAudioData();
		  break;
	      default:
		  break;
	    } // end of switch
	}
//   		if (_queues[rthead->channel] != 0) {
//   		    _queues[rthead->channel].push(chunk);
//   		}
//	ptr += rthead->head_size + rthead->bodysize;
    };
    
    return msg;
}
#endif

// Receive a message, which is a series of AMF elements, seperated
// by a one byte header at regular byte intervals. (128 bytes for
// video data by default). Each message main contain multiple packets.
boost::shared_ptr<amf::Buffer> 
RTMP::recvMsg()
{
//    GNASH_REPORT_FUNCTION;
    return recvMsg(getFileFd());
}

// Read big chunks of NETBUFSIZE, which is the default for a Buffer as it's
// more efficient. As these reads may cross packet boundaries, and they may
// also include the RTMP header every _chunksize bytes, this raw data will
// need to be processed later on.
boost::shared_ptr<amf::Buffer> 
RTMP::recvMsg(int fd)
{
//     GNASH_REPORT_FUNCTION;

    int ret = 0;
    //bool nopacket = true;

    // Read really big packets, they get split into the smaller ones when 'split'
    boost::shared_ptr<amf::Buffer> buf(new Buffer(7096));
    do {
	ret = readNet(fd, buf->reference()+ret, buf->size()-ret, _timeout);
//	cerr << __PRETTY_FUNCTION__ << ": " << ret << endl;
	// We got data. Resize the buffer if necessary.
	if (ret > 0) {
	    buf->setSeekPointer(buf->reference() + ret);
	}
	// the read timed out as there was no data, but the socket is still open.
 	if (ret == 0) {
	    log_debug("no data for fd #%d, done reading this packet...", fd);
 	    break;
 	}
	if ((ret == 1) && (*(buf->reference()) == 0xff)) {
	    log_debug("Got an empty packet from the server at line %d", __LINE__);
	    ret = 0;
	    buf->clear();
 	    continue;
	}
	// ret is "no position" when the socket is closed from the other end of the connection,
	// so we're done.
	if ((ret == static_cast<int>(std::string::npos)) || (ret == -1)) {
	    log_debug("socket for fd #%d was closed...", fd);
	    buf.reset();
	    break;
	}
    } while (ret <= 0);
    
//     if (netDebug()) {
// 	buf->dump();
//     }

    return buf;
}

// Split a large buffer into multiple smaller ones of the default chunksize
// of 128 bytes. We read network data in big chunks because it's more efficient,
// but RTMP uses a weird scheme of a standard header, and then every chunksize
// bytes another 1 byte RTMP header. The header itself is not part of the byte
// count.
boost::shared_ptr<RTMP::queues_t>
RTMP::split(amf::Buffer &buf)
{
//     GNASH_REPORT_FUNCTION;
    return split(buf.reference(), buf.allocated());
}

boost::shared_ptr<RTMP::queues_t>
RTMP::split(boost::uint8_t *data, size_t size)
{
//     GNASH_REPORT_FUNCTION;

    if (data == 0) {
	log_error("Buffer pointer is invalid.");
    }

    boost::shared_ptr<RTMP::queues_t> channels(new RTMP::queues_t);
	
    // split the buffer at the chunksize boundary
    boost::uint8_t *ptr = 0;
    boost::shared_ptr<rtmp_head_t> rthead(new rtmp_head_t);
    size_t pktsize = 0;
    //size_t nbytes = 0;
    
    ptr = data;
    boost::shared_ptr<amf::Buffer> chunk;
    // There may be multiple messages in this Buffer, so we walk a
    // temp pointer through the contents of the Buffer.
    while ((ptr - data) < static_cast<int>(size)) {
	// Decode the header of the packet to get the header size, the
	// body size, and the channel, all of which we need.
	rthead = decodeHeader(ptr);
	// System channel messages are always on channel 2, and get
	// processed differently.
	// FIXME: skip system messages for now!
	if (rthead->channel == RTMP_SYSTEM_CHANNEL) {
// 	    log_debug("FIXME: %s: Got a message on the system channel!", __FUNCTION__);
	}
	// Make sure the header size we just got is in range. We can
	// proceed as long as it is in range, but if it is out of
	// range, we can't really continue.
	if (rthead->head_size <= RTMP_MAX_HEADER_SIZE) {
	    // Any packet with a header size greater than 1 is a
	    // always a new RTMP message, so create a new Buffer to
	    // hold all the data.
	    if (rthead->head_size <= 4) {
		rthead->bodysize = _lastsize[rthead->channel];
	    }
	    if ((rthead->head_size > 1) || (ptr == data)) {
//  		cerr << "New packet for channel #" << rthead->channel << " of size "
//  		     << (rthead->head_size + rthead->bodysize) << endl;
		// give it some memory to store data in. We store
		chunk.reset(new Buffer(rthead->bodysize + rthead->head_size));
		// Each RTMP connection has 64 channels, so we store
		// the header with the data so that info is accessible
		// via the Buffer for processing later. All the data
		// goes in a queue for each channel.
		_queues[rthead->channel].push(chunk);
//		rthead->head_size == 4
	    } else {
		// Use the existing Buffer for this packet, as it's a
		// continuation messages for an existing packet. Leave
		// the message in the queue, we just want access to
		// the Buffer.
		chunk = _queues[rthead->channel].peek();
	    }
#if 1
	    // Red5 version 5 sends out PING messages with a 1 byte header. I think this
	    // may be a bug in Red5, but we should handle it anyway.
	    if (chunk == 0) {
// 		cerr << "Chunk wasn't allocated! " << (rthead->bodysize + rthead->head_size) << endl;
		chunk.reset(new Buffer(rthead->bodysize + rthead->head_size));
		chunk->clear();	// FIXME: temporary debug only, should be unnecessary
		_queues[rthead->channel].push(chunk);
	    }
#endif    
	    // Many RTMP messages are smaller than the chunksize, so
	    // they're easy. Each channel may have a different
	    // chunksize, just to keep things interesting. The
	    // chunksize for a channel is changed by the Chunksize
	    // RTMP command.
	    if (chunk->size() <= _chunksize[rthead->channel]) {
		// Since the total RTMP message size is less than the
		// chunksize for this channel, the packet size is the
		// total message size.
		pktsize = chunk->size();
	    } else {
		// This RTMP message is larger than the chunksize for
		// this channel, so the packet size is smaller than
		// the total message size. The header bytes aren't
		// counted as part of the message size, so we read the
		// header plus all the data up to the channel chunksize.
		if (rthead->head_size > 1) {
		    pktsize = rthead->head_size + _chunksize[rthead->channel];
		} else {
		    // One byte headers are continuation messages for
		    // existing data. There may be multiple
		    // continuation messages to complete the RTMP
		    // messagem, so all packets are read up to the
		    // chunksize but the last packet of the sequence.
		    if (chunk->spaceLeft() < _chunksize[rthead->channel]) {
			// don't store the continutation header,jusdt
			// append the data,
			pktsize = chunk->spaceLeft();
		    } else {
			pktsize = rthead->head_size + (chunk->size() - _chunksize[rthead->channel]);
		    }
		}
	    }
	    
	    // Now that we calculated the packet size, range check it
	    // for sanity.
	    if (pktsize <= (_chunksize[rthead->channel] + RTMP_MAX_HEADER_SIZE)) {
		// Skip the header for all but the first packet. The rest are just to
		// complete all the data up to the body size from the header.
// 		cerr << _queues[rthead->channel].size() << " messages in queue for channel "
// 		     << rthead->channel << endl;
		if ((rthead->head_size == 1) && (ptr != data)) {
//  		    cerr << "FOLLOWING PACKET!" << " for channel " << rthead->channel << endl;
//  		    cerr << "Space Left in buffer for channel " << rthead->channel << " is: "
//  			 << chunk->spaceLeft() << endl;
		    ptr += rthead->head_size;
//  		} else {
//  		    cerr << "FIRST PACKET!" << " for channel " << rthead->channel << endl;
		}
		// This is a queue of channels with active messages. This is a
		// much smaller list to traverse when processing data than all 64 channels.
		if (pktsize < 0xffffff) {
//		    cerr << "FIXME5: " << hexify(ptr, pktsize, true) << endl;
		    // If the packet size is in range, then append the
		    // data to the existing data to complete the message.
		    chunk->append(ptr, pktsize);
		    _lastsize[rthead->channel] = rthead->bodysize;
// 		    cerr << "Adding data to existing packet for channel #" << rthead->channel
// 			 << ", read " << pktsize << " bytes." << endl;
		    ptr += pktsize;
		} else {
		    log_error("Packet size out of range! %d, %d", rthead->bodysize, pktsize);
		}
		// add this packet to the list of active channels if we've gotten
		// all the data.
// 		if (chunk->allocated() - 5 >= rthead->bodysize) {
// 		    cerr << "Bodysize: " << rthead->bodysize 
// 			 << " Allocated Chunksize: " << chunk->allocated() << endl;
		channels->push_back(&_queues[rthead->channel]);
//		}
	    } else {
		log_error("RTMP packet size is out of range! %d, %d", rthead->bodysize, pktsize);
		break;
	    }
	} else {
	    log_error("RTMP header size is out of range! %d", rthead->head_size);
	    break;
	}
    }

    return channels;
}


} // end of gnash namespace

// local Variables:
// mode: C++
// indent-tabs-mode: t
// End:

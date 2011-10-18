//
//   Copyright (C) 2007, 2008, 2009, 2010, 2011 Free Software Foundation, Inc.
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

#include "MediaHandlerIOS.h"

#include <sstream>

#include "log.h"
#include "FLVParser.h"
#include "MediaParser.h"

#include "IOChannel.h" // for visibility of destructor



namespace gnash {
namespace media {
namespace ios {

std::string
MediaHandlerIOS::description() const
{
    return "iOS";
}

std::auto_ptr<MediaParser>
MediaHandlerIOS::createMediaParser(std::auto_ptr<IOChannel> stream)
{
    std::auto_ptr<MediaParser> parser;

    try
    {
        if (isFLV(*stream))
        {
            parser.reset(new FLVParser(stream));
        }
        else
        {
            // TODO
            assert(false);
        }
    }
    catch (GnashException& ex)
    {
        log_error("Could not create iOS media parser for "
                "input stream: %s", ex.what());
        assert(!parser.get());
    }

    return parser;
}

std::auto_ptr<VideoDecoder>
MediaHandlerIOS::createVideoDecoder(const VideoInfo& info)
{
    if (info.type != CODEC_TYPE_FLASH) {
      // TODO
      assert(false);
    }
//    videoCodecType format = static_cast<videoCodecType>(info.codec);
//    int width = info.width;
//    int height = info.height;

    boost::uint8_t* extradata = 0;
    size_t datasize = 0;

    ExtraVideoInfoFlv* extrainfo = dynamic_cast<ExtraVideoInfoFlv*>(info.extra.get());
    if (extrainfo) {
        extradata = extrainfo->data.get();
                datasize = extrainfo->size;
    }

    // TODO
    assert(false);
    std::auto_ptr<VideoDecoder> ret;
    return ret;
}

std::auto_ptr<AudioDecoder>
MediaHandlerIOS::createAudioDecoder(const AudioInfo& info)
{
    std::auto_ptr<AudioDecoder> ret;

#ifdef DECODING_SPEEX
    if (info.codec == AUDIO_CODEC_SPEEX) {
        assert(info.type == CODEC_TYPE_FLASH);
        ret.reset(new AudioDecoderSpeex);
    } else
#endif
    {
        try {
            // TODO
            assert(false);
        }
        catch (const MediaException& ex) {

            if (info.type != CODEC_TYPE_FLASH) throw;

            try {
                ret = createFlashAudioDecoder(info);
            }
            catch (const MediaException& ex2) {
                boost::format err = boost::format(
                    _("MediaHandlerIOS::createAudioDecoder: %s "
                      "-- %s")) % ex.what() % ex2.what();
                throw MediaException(err.str());
            }
        }

    }

    return ret;
}

std::auto_ptr<VideoConverter>
MediaHandlerIOS::createVideoConverter(ImgBuf::Type4CC srcFormat, ImgBuf::Type4CC dstFormat)
{
    std::auto_ptr<VideoConverter> converter;

    try
    {
        // TODO
        assert(false);
    }
    catch (GnashException& ex)
    {
        log_error("Could not create iOS video converter parser for "
                "input format: %s", ex.what());
    }

    return converter;
}

AudioInput*
MediaHandlerIOS::getAudioInput(size_t /*index*/)
{
    // FIXME: these should be stored in the media handler, not newly
    // created each time. The documentation is correct, implementation wrong.
    // TODO
    assert(false);
    return 0;
}

VideoInput*
MediaHandlerIOS::getVideoInput(size_t /*index*/)
{
    // FIXME: these should be stored in the media handler, not newly
    // created each time. The documentation is correct, implementation wrong.
    assert(false);
    return 0;
}

void
MediaHandlerIOS::cameraNames(std::vector<std::string>& names) const
{
    assert(false);
}

#ifdef REGISTER_MEDIA_HANDLERS
namespace {
    MediaFactory::RegisterHandler<MediaHandlerIOS> reg("ios");
}
#endif

} // gnash.media.ios namespace
} // gnash.media namespace
} // gnash namespace

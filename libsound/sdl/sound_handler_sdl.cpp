// sound_handler_sdl.cpp: Sound handling using standard SDL
//
//   Copyright (C) 2005, 2006, 2007, 2008, 2009 Free Software Foundation, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//

// Based on sound_handler_sdl.cpp by Thatcher Ulrich http://tulrich.com 2003
// which has been donated to the Public Domain.


#ifdef HAVE_CONFIG_H
#include "gnashconfig.h"
#endif

#include "sound_handler_sdl.h"
#include "SoundInfo.h"
#include "EmbedSound.h"
#include "AuxStream.h" // for use..

#include "log.h" // will import boost::format too
#include "GnashException.h" // for SoundException

//#include <cmath>
#include <vector>
#include <boost/scoped_array.hpp>
#include <SDL.h>

// Define this to get debugging call about pausing/unpausing audio
//#define GNASH_DEBUG_SDL_AUDIO_PAUSING

// Mixing and decoding debugging
//#define GNASH_DEBUG_MIXING

namespace { // anonymous

// Header of a wave file
// http://ftp.iptel.org/pub/sems/doc/full/current/wav__hdr_8c-source.html
typedef struct{
     char rID[4];            // 'RIFF'
     long int rLen;        
     char wID[4];            // 'WAVE'
     char fId[4];            // 'fmt '
     long int pcm_header_len;   // varies...
     short int wFormatTag;
     short int nChannels;      // 1,2 for stereo data is (l,r) pairs
     long int nSamplesPerSec;
     long int nAvgBytesPerSec;
     short int nBlockAlign;      
     short int nBitsPerSample;
} WAV_HDR;

// Chunk of wave file
// http://ftp.iptel.org/pub/sems/doc/full/current/wav__hdr_8c-source.html
typedef struct{
    char dId[4];            // 'data' or 'fact'
    long int dLen;
} CHUNK_HDR;

} // end of anonymous namespace

namespace gnash {
namespace sound {


void
SDL_sound_handler::initAudio()
{
    // This is our sound settings
    audioSpec.freq = 44100;

    // Each sample is a signed 16-bit audio in system-endian format
    audioSpec.format = AUDIO_S16SYS; 

    // We want to be pulling samples for 2 channels:
    // {left,right},{left,right},...
    audioSpec.channels = 2;

    audioSpec.callback = SDL_sound_handler::sdl_audio_callback;

    audioSpec.userdata = this;

    //512 - not enough for  videostream
    audioSpec.samples = 2048;   

    openAudio();

}

void
SDL_sound_handler::openAudio()
{
    if ( _audioOpened ) return; // nothing to do

    if (SDL_OpenAudio(&audioSpec, NULL) < 0 ) {
            boost::format fmt = boost::format(
            _("Unable to open SDL audio: %s"))
            % SDL_GetError();
        throw SoundException(fmt.str());
    }

    _audioOpened = true;
}

void
SDL_sound_handler::closeAudio()
{
    SDL_CloseAudio();
    _audioOpened = false;
}


SDL_sound_handler::SDL_sound_handler(const std::string& wavefile)
    :
    _audioOpened(false)
{

    initAudio();

    if (! wavefile.empty() ) {
        file_stream.open(wavefile.c_str());
        if (file_stream.fail()) {
            std::cerr << "Unable to write file '" << wavefile << std::endl;
            exit(1);
        } else {
                write_wave_header(file_stream);
                std::cout << "# Created 44100 16Mhz stereo wave file:" << std::endl <<
                    "AUDIOFILE=" << wavefile << std::endl;
        }
    }

}

SDL_sound_handler::SDL_sound_handler()
    :
    _audioOpened(false)
{
    initAudio();
}

void
SDL_sound_handler::reset()
{
    boost::mutex::scoped_lock lock(_mutex);
    sound_handler::delete_all_sounds();
    sound_handler::stop_all_sounds();
}

SDL_sound_handler::~SDL_sound_handler()
{
    boost::mutex::scoped_lock lock(_mutex);
#ifdef GNASH_DEBUG_SDL_AUDIO_PAUSING
    log_debug("Pausing SDL Audio on destruction");
#endif
    SDL_PauseAudio(1);

    lock.unlock();

    // we already locked, so we call 
    // the base class (non-locking) deleter
    delete_all_sounds();

    unplugAllInputStreams();

    SDL_CloseAudio();

    if (file_stream) file_stream.close();
}


int
SDL_sound_handler::create_sound(std::auto_ptr<SimpleBuffer> data,
                                std::auto_ptr<media::SoundInfo> sinfo)
{
    boost::mutex::scoped_lock lock(_mutex);
    return sound_handler::create_sound(data, sinfo);
}

// This gets called when an SWF embedded sound stream gets more data
long
SDL_sound_handler::fill_stream_data(unsigned char* data,
        unsigned int dataBytes, unsigned int nSamples,
        int handleId)
{

    boost::mutex::scoped_lock lock(_mutex);
    return sound_handler::fill_stream_data(data, dataBytes, nSamples, handleId);
}


void
SDL_sound_handler::playSound(int id, int loops, int offSecs,
        long startPos, const SoundEnvelopes* env, bool allowMulti)
{
    boost::mutex::scoped_lock lock(_mutex);
    // WARNING: playSound might trigger another lock of _mutex here (check me)
    sound_handler::playSound(id, loops, offSecs, startPos, env, allowMulti);
}


void
SDL_sound_handler::stop_sound(int soundHandle)
{
    boost::mutex::scoped_lock lock(_mutex);
    sound_handler::stop_sound(soundHandle);
}


void
SDL_sound_handler::delete_sound(int soundHandle)
{
    boost::mutex::scoped_lock lock(_mutex);
    sound_handler::delete_sound(soundHandle);
}

void   
SDL_sound_handler::stop_all_sounds()
{
    boost::mutex::scoped_lock lock(_mutex);
    sound_handler::stop_all_sounds();
}


int
SDL_sound_handler::get_volume(int soundHandle)
{
    boost::mutex::scoped_lock lock(_mutex);
    return sound_handler::get_volume(soundHandle);
}


void   
SDL_sound_handler::set_volume(int soundHandle, int volume)
{
    boost::mutex::scoped_lock lock(_mutex);
    sound_handler::set_volume(soundHandle, volume);
}
    
media::SoundInfo*
SDL_sound_handler::get_sound_info(int soundHandle)
{
    boost::mutex::scoped_lock lock(_mutex);
    return sound_handler::get_sound_info(soundHandle);
}

InputStream*
SDL_sound_handler::attach_aux_streamer(aux_streamer_ptr ptr, void* owner)
{
    // TODO: move to base class !!

    boost::mutex::scoped_lock lock(_mutex);
    assert(owner);
    assert(ptr);

    std::auto_ptr<InputStream> newStreamer ( new AuxStream(ptr, owner) );

    InputStream* ret = newStreamer.get();

    plugInputStream(newStreamer);

    return ret;
}

unsigned int
SDL_sound_handler::get_duration(int soundHandle)
{
    boost::mutex::scoped_lock lock(_mutex);
    return sound_handler::get_duration(soundHandle);
}

unsigned int
SDL_sound_handler::tell(int soundHandle)
{
    boost::mutex::scoped_lock lock(_mutex);
    return sound_handler::tell(soundHandle);
}

sound_handler*
create_sound_handler_sdl()
// Factory.
{
    return new SDL_sound_handler;
}

sound_handler*
create_sound_handler_sdl(const std::string& wave_file)
// Factory.
{
    return new SDL_sound_handler(wave_file);
}

// write a wave header, using the current audioSpec settings
void
SDL_sound_handler::write_wave_header(std::ofstream& outfile)
{
 
  // allocate wav header
  WAV_HDR wav;
  CHUNK_HDR chk;
 
  // setup wav header
  std::strncpy(wav.rID, "RIFF", 4);
  std::strncpy(wav.wID, "WAVE", 4);
  std::strncpy(wav.fId, "fmt ", 4);
 
  wav.nBitsPerSample = ((audioSpec.format == AUDIO_S16SYS) ? 16 : 0);
  wav.nSamplesPerSec = audioSpec.freq;
  wav.nAvgBytesPerSec = audioSpec.freq;
  wav.nAvgBytesPerSec *= wav.nBitsPerSample / 8;
  wav.nAvgBytesPerSec *= audioSpec.channels;
  wav.nChannels = audioSpec.channels;
    
  wav.pcm_header_len = 16;
  wav.wFormatTag = 1;
  wav.rLen = sizeof(WAV_HDR) + sizeof(CHUNK_HDR);
  wav.nBlockAlign = audioSpec.channels * wav.nBitsPerSample / 8;

  // setup chunk header
  std::strncpy(chk.dId, "data", 4);
  chk.dLen = 0;
 
  /* write riff/wav header */
  outfile.write((char *)&wav, sizeof(WAV_HDR));
 
  /* write chunk header */
  outfile.write((char *)&chk, sizeof(CHUNK_HDR));
 
}

void
SDL_sound_handler::fetchSamples(boost::int16_t* to, unsigned int nSamples)
{
    boost::mutex::scoped_lock lock(_mutex);
    sound_handler::fetchSamples(to, nSamples);

    // TODO: move this to base class !
    if (file_stream)
    {
        // NOTE: if muted, the samples will be silent already
        boost::uint8_t* stream = reinterpret_cast<boost::uint8_t*>(to);
        unsigned int len = nSamples*2;
        file_stream.write((char*) stream, len);

        // now, mute all audio
        std::fill(to, to+nSamples, 0);
    }

    // If nothing is left to play there is no reason to keep polling.
    if ( ! hasInputStreams() )
    {
#ifdef GNASH_DEBUG_SDL_AUDIO_PAUSING
        log_debug("Pausing SDL Audio...");
#endif
        SDL_PauseAudio(1);
    }
}

// Callback invoked by the SDL audio thread.
void
SDL_sound_handler::sdl_audio_callback (void *udata, Uint8 *buf, int bufLenIn)
{
    if ( bufLenIn < 0 )
    {
        log_error(_("Negative buffer length in sdl_audio_callback (%d)"), bufLenIn);
        return;
    }

    if ( bufLenIn == 0 )
    {
        log_error(_("Zero buffer length in sdl_audio_callback"));
        return;
    }

    unsigned int bufLen = static_cast<unsigned int>(bufLenIn);
    boost::int16_t* samples = reinterpret_cast<boost::int16_t*>(buf);
    assert(!(bufLen%2));
    unsigned int nSamples = bufLen/2;

    // Get the soundhandler
    SDL_sound_handler* handler = static_cast<SDL_sound_handler*>(udata);
    handler->fetchSamples(samples, nSamples);
}

void
SDL_sound_handler::mix(boost::int16_t* outSamples, boost::int16_t* inSamples,
            unsigned int nSamples, float volume)
{
    Uint8* out = reinterpret_cast<Uint8*>(outSamples);
    Uint8* in = reinterpret_cast<Uint8*>(inSamples);
    unsigned int nBytes = nSamples*2;
    
    SDL_MixAudio(out, in, nBytes, SDL_MIX_MAXVOLUME*volume);
}

void
SDL_sound_handler::plugInputStream(std::auto_ptr<InputStream> newStreamer)
{
    // TODO: lock the mutex once attach_aux_streamer is in the base class

    sound_handler::plugInputStream(newStreamer);

#ifdef GNASH_DEBUG_SDL_AUDIO_PAUSING
    log_debug("Unpausing SDL Audio...");
#endif
    SDL_PauseAudio(0);
}

void
SDL_sound_handler::mute()
{
    boost::mutex::scoped_lock lock(_mutedMutex);
    sound_handler::mute();
}

void
SDL_sound_handler::unmute()
{
    boost::mutex::scoped_lock lock(_mutedMutex);
    sound_handler::unmute();
}

bool
SDL_sound_handler::is_muted() const
{
    boost::mutex::scoped_lock lock(_mutedMutex);
    return sound_handler::is_muted();
}

void
SDL_sound_handler::pause() 
{
    closeAudio();

    sound_handler::pause();
}

void
SDL_sound_handler::unpause() 
{
    openAudio();

    if ( hasInputStreams() ) SDL_PauseAudio(0);

    sound_handler::unpause();
}

} // gnash.sound namespace 
} // namespace gnash

// Local Variables:
// mode: C++
// End:


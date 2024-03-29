//
//   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Free Software
//   Foundation, Inc
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

#ifdef HAVE_CONFIG_H
#include "gnashconfig.h"
#endif

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>
#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>
#include <fcntl.h>

#include "log.h"
#include "Renderer.h"
#include "Renderer_agg.h"
#include "fbsup.h"
#include "RunResources.h"
#include "fb_glue_agg.h"

#ifdef BUILD_RAWFB_DEVICE
#include "rawfb/RawFBDevice.h"
#endif

namespace gnash {

namespace gui {

// FBGlue::FBGlue()
// {
//     GNASH_REPORT_FUNCTION;
// }

// FBGlue::~FBGlue()
// {
//     GNASH_REPORT_FUNCTION;
// }

//---------------------------------------------
FBAggGlue::FBAggGlue()
    : _fd(-1)
{
    GNASH_REPORT_FUNCTION;
    memset(&_varinfo, 0, sizeof(fb_var_screeninfo));
    memset(&_fixinfo, 0, sizeof(fb_fix_screeninfo));
}

FBAggGlue::FBAggGlue(int fd)
    : _fd(fd)
{
    GNASH_REPORT_FUNCTION;    
}

FBAggGlue::~FBAggGlue()
{
    GNASH_REPORT_FUNCTION;    

    // Close the memory
    if (_fd) {
        ::close(_fd);
    }
}

void
FBAggGlue::setInvalidatedRegion(const SWFRect &/*bounds */)
{
    // GNASH_REPORT_FUNCTION;
    
    if (!_renderer) {
        log_error("No renderer set!");
        return;
    }
}

void
FBAggGlue::setInvalidatedRegions(const InvalidatedRanges &ranges)
{
    // GNASH_REPORT_FUNCTION;

    if (!_renderer) {
        log_error("No renderer set!");
        return;
    }

    _renderer->set_invalidated_regions(ranges);
    
    _drawbounds.clear();

    for (size_t rno = 0; rno<ranges.size(); rno++) {
        geometry::Range2d<int> bounds = Intersection(
            _renderer->world_to_pixel(ranges.getRange(rno)),
            _validbounds);
        // it may happen that a particular range is out of the screen, which 
        // will lead to bounds==null. 
        if (bounds.isNull()) continue; 
        
        _drawbounds.push_back(bounds);   
    }

}

bool
FBAggGlue::init (int argc, char ***argv)
{
    GNASH_REPORT_FUNCTION;    

    // The device must be initialized before the renderer. AGG only supports
    // The Raw framebuffer, so we use that.
    _device.reset(new renderer::rawfb::RawFBDevice);
    _device->initDevice(argc, *argv);    

    // Set the renderer for the AGG glue layer
    gnash::Renderer *rend = reinterpret_cast<gnash::Renderer *>
                                                (createRenderHandler());
    if (rend) {
        _renderer.reset(rend);
    } else {
        log_error("failed to create a render handler for AGG!");
        return false;
    }

#ifdef PIXELFORMAT_LUT8
    // Set grayscale for 8 bit modes
    renderer::rawfb::RawFBDevice *rawfb = reinterpret_cast
        <renderer::rawfb::RawFBDevice *>(_device.get());    
    if (_varinfo.bits_per_pixel == 8) {
	if (!rawfb->setGrayscaleLUT8())
	    return false;
    }
#endif

    _display.initDevice(0, 0);

    // You must pass in the file descriptor to the opened
    // framebuffer when creating a window. Under X11, this is
    // actually the XID of the created window.
    return _device->attachWindow(_display.getHandle());
    
    return true;
}

#define TO_16BIT(x) (x | (x<<8))

Renderer *
FBAggGlue::createRenderHandler()
{
    GNASH_REPORT_FUNCTION;

    if (!_device) {
        log_error("No Device layer initialized yet!");
        return 0;
    }
    
    const int width     = _device->getWidth();
    const int height    = _device->getHeight();
    const int bpp       = _device->getDepth();
    
    // TODO: should recalculate!  
    boost::uint8_t       *mem;
    Renderer_agg_base   *agg_handler;

    agg_handler = NULL;

    _validbounds.setTo(0, 0, width - 1, height - 1);
    
#ifdef ENABLE_DOUBLE_BUFFERING
    log_debug(_("Double buffering enabled"));
    mem = _buffer;
#else
    log_debug(_("Double buffering disabled"));
    mem = _device->getFBMemory();
#endif
    
    agg_handler = NULL;
  
    // choose apropriate pixel format

    renderer::rawfb::RawFBDevice *rawfb = reinterpret_cast
        <renderer::rawfb::RawFBDevice *>(_device.get());
    log_debug(_("red channel: %d / %d"), rawfb->getRedOffset(), 
	      rawfb->getRedSize());
    log_debug(_("green channel: %d / %d"), rawfb->getGreenOffset(), 
	      rawfb->getGreenSize());
    log_debug(_("blue channel: %d / %d"), rawfb->getBlueOffset(), 
              rawfb->getBlueSize());
    log_debug(_("Total bits per pixel: %d"), bpp);
    
    const char* pixelformat = agg_detect_pixel_format(
        rawfb->getRedOffset(),   rawfb->getRedSize(),
        rawfb->getGreenOffset(), rawfb->getGreenSize(),
        rawfb->getBlueOffset(),  rawfb->getBlueSize(),
        bpp);

    if (pixelformat) {
	agg_handler = create_Renderer_agg(pixelformat);
    } else {
	log_error("The pixel format of your framebuffer could not be detected.");
	return false;
    }
    
    assert(agg_handler != NULL);

    // This attaches the memory from the framebuffer to the AGG
    // renderer.
    size_t rowsize = width*((bpp+7)/8);
    agg_handler->init_buffer((unsigned char *)mem, _device->getFBMemSize(),
                             width, height, rowsize);
    
    return (Renderer *)agg_handler;
}    

void
FBAggGlue::prepDrawingArea(FbWidget */* drawing_area */)
{
    GNASH_REPORT_FUNCTION;
    // nothing to do here, the memory was attached when
    // creating the renderer.
}

#if 0
void
FBGlue::render(void * /* region */)
{
    GNASH_REPORT_FUNCTION;
}
#endif

void
FBAggGlue::render()
{
    GNASH_REPORT_FUNCTION;

    if ( _drawbounds.size() == 0 ) {
        return; // nothing to do..
    }
    
#ifdef ENABLE_DOUBLE_BUFFERING
    // Size of a pixel in bytes
    // NOTE: +7 to support 15 bpp
    const unsigned int pixel_size = (var_screeninfo.bits_per_pixel+7)/8;
    
    for (unsigned int bno=0; bno < _drawbounds.size(); bno++) {
	geometry::Range2d<int>& bounds = _drawbounds[bno];
        
	assert ( ! bounds.isWorld() );
	
	// Size, in bytes, of a row that has to be copied
	const unsigned int row_size = (bounds.width()+1) * pixel_size;
	
	// copy each row
	const int minx = bounds.getMinX();
	const int maxy = bounds.getMaxY();
	
	for (int y=bounds.getMinY(); y<=maxy; ++y) {    
	    const unsigned int pixel_index = y * _rowsize + minx*pixel_size;
	    memcpy(&(_fbmem[pixel_index]), &buffer[pixel_index], row_size);
	    
	}
    }  
    
#endif
    
#ifdef DEBUG_SHOW_FPS
    profile();
#endif
}    

int
FBAggGlue::width()
{
    GNASH_REPORT_FUNCTION;

    if (_device) {
        return _device->getWidth();
    }
    return 0;
}

int
FBAggGlue::height()
{
    GNASH_REPORT_FUNCTION;

    if (_device) {
        return _device->getHeight();
    }
    return 0;
}

} // end of namespace gui
} // end of namespace gnash
    
// Local Variables:
// mode: C++
// indent-tabs-mode: nil
// End:

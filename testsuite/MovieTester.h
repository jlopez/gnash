/* 
 *   Copyright (C) 2005, 2006, 2007, 2008 Free Software Foundation, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 */ 

#ifndef GNASH_MOVIETESTER_H
#define GNASH_MOVIETESTER_H

#ifdef HAVE_CONFIG_H
# include "gnashconfig.h" // For exp2 test
#endif

#include "Range2d.h"
#include "GnashKey.h"
#include "sound_handler.h" // for creating the "test" sound handlers
#include "render_handler.h" // for dtor visibility by auto_ptr
#include "movie_instance.h" 
#include "ManualClock.h" // for composition
#include "RunInfo.h" // For initialization.

#include <memory> // for auto_ptr
#include <string> 
#include <boost/shared_ptr.hpp>
#include <cmath>

#define check_pixel(x, y, radius, color, tolerance) \
	{\
		std::stringstream ss; \
		ss << "[" << __FILE__ << ":" << __LINE__ << "]"; \
		tester.checkPixel(x, y, radius, color, tolerance, ss.str(), false); \
	}

#define xcheck_pixel(x, y, radius, color, tolerance) \
	{\
		std::stringstream ss; \
		ss << "[" << __FILE__ << ":" << __LINE__ << "]"; \
		tester.checkPixel(x, y, radius, color, tolerance, ss.str(), true); \
	}

// Forward declarations
namespace gnash {
	class movie_definition;
	class movie_root;
	class MovieClip;
	class character;
	class FuzzyPixel;
	class VirtualClock;
	class rgba;
}

namespace gnash {

/// A table of built renderers
//
///
class TestingRenderer
{

public:

	TestingRenderer(std::auto_ptr<render_handler> renderer, const std::string& name)
		:
		_name(name),
		_renderer(renderer)
	{}

	const std::string& getName() const { return _name; }

	/// Return the underlying render handler
	render_handler& getRenderer() const { return *_renderer; }

private:

	std::string _name;
	std::auto_ptr<render_handler> _renderer;
};

typedef boost::shared_ptr<TestingRenderer> TestingRendererPtr;

/// An utility class for testing movie playback
//
/// This is a just born implementation and doesn't
/// have much more then simply loading a movie and
/// providing a function to find DisplayItems by name
///
/// More functions will be added when needed.
///
class MovieTester
{
public:
	/// Fully load the movie at the specified location
	/// and create an instance of it.
	/// Also, initialize any built renderer capable of in-memory
	/// rendering to allow testing of it.
	/// The renderer(s) will be initialized with a memory
	/// buffer with the size found in the SWF header
	///
	MovieTester(const std::string& filespec);

	/// Advance the movie by one frame
	//
	/// @param updateClock
	///	If true (the default), this method also
	///     advances the clock by the nominal delay expected
	///     between frame advancements before performing the
	///     actual playhead advancement.
	///
	void advance(bool updateClock=true);

	/// Advance the clock by the given amount of milliseconds
	void advanceClock(unsigned long ms);

	/// Fully redraw of current frame
	//
	/// This function forces complete redraw in all testing
	/// renderers.
	///
	void redraw();

	/// Return the invalidated ranges in PIXELS
	//
	/// This is to debug/test partial rendering
	///
	geometry::SnappingRanges2d<int> getInvalidatedRanges() const;

	/// Find a character in the display list of a sprite by name.
	//
	/// Return NULL if there's no character with that name in
	/// the sprite's display list.
	///
	const character* findDisplayItemByName(const MovieClip& mc,
			const std::string& name);

	/// Find a character in the display list of a sprite by depth.
	//
	/// Return NULL if there's no character at that depth in
	/// the sprite's display list.
	///
	const character* findDisplayItemByDepth(const MovieClip& mc,
			int depth);

	/// Get the topmost sprite instance of this movie
	gnash::MovieClip* getRootMovie() {
		return _movie;
	}

	/// Notify mouse pointer movement to the given coordinate
	//
	/// Coordinates are in pixels
	///
	void movePointerTo(int x, int y);

	/// Check color of the average pixel under the mouse pointer 
	//
	///
	/// This method will test any built renderer.
	///
	/// @param x
	///	The x coordinate of the point being the center
	///	of the circle you want to compute the average color of.
	///
	/// @param y
	///	The y coordinate of the point being the center
	///	of the circle you want to compute the average color of.
	///
	/// @param radius
	///	Radius defining the average zone used.
	///	1 means a single pixel.
	///	Behaviour of passing 0 is undefined.
	///
	/// @param color
	///	The color we expect to find under the pointer.
	///
	/// @param tolerance
	///	The tolerated difference of any r,g,b,a values.
	///	Note that the actual tolerance used for comparison might
	///	be bigger then the given one depending on the minimum tolerance
	///	supported by the renderers being tested, being a function of color
	///	depth. For example, comparisions against 16bpp renderers will use
	///	at tolerance of at least 8.
	///
	/// @param label
	///	A label to use in test results.
	///
	/// @param expectFailure
	///	Set to true if a failure is expected. Defaults to false.
	///
	void checkPixel(int x, int y, unsigned radius, const rgba& color,
			short unsigned tolerance, const std::string& label, bool expectFailure=false) const;

	/// Notify mouse button was pressed
	void pressMouseButton();

	/// Notify mouse button was depressed
	void depressMouseButton();

	/// Simulate a mouse click (press and depress mouse button)
	void click();

	/// Notify key press
	//
	/// See key codes in namespace gnash::key (gnash.h)
	///
	void pressKey(key::code k);

	/// Notify key release
	//
	/// See key codes in namespace gnash::key (gnash.h)
	///
	void releaseKey(key::code k);

	/// Return true if the currently active 
	/// character is over a character that
	/// handles mouse events
	bool isMouseOverMouseEntity();

	/// \brief
	/// Return the number of times a sound has been stopped,
	/// or 0 if sound testing is not supported. See canTestSound().
	//
	int soundsStopped();

	/// \brief
	/// Return the number of times a sound has been started,
	/// or 0 if sound testing is not supported. See canTestSound().
	//
	int soundsStarted();

	/// Return true if this build of MovieTester supports sound testing
	//
	/// Sound will be supported as long as a sound handler was compiled in.
	///
	bool canTestSound() const { return _sound_handler.get() != NULL; }

	/// Return true if this build of MovieTester supports pixel checking 
	//
	/// Pixel checking will be supported as long as a testing-capable render handler
	/// was compiled in. Testing-capable means capable of off-screen rendering, which
	/// is implementing the render_handler::initTestBuffer method.
	///
	bool canTestRendering() const { return ! _testingRenderers.empty(); }

	/// Return true if this build of gnash supports video
	bool canTestVideo() const;

	/// Restart the movie
	//
	/// NOTE: the movie returned by getRootMovie() will likely be
	///       NOT the real root movie anymore, so call getRootMovie
	///	  again after this call.
	///
	void restart();

    /// Simulate a manually resized view.
    //
    /// If scaleMode != noScale, the renderers are instructed
    /// to scale the view.
    void resizeStage(int x, int y) ; 

private:

	/// Initialize testing renderers
	void initTestingRenderers();

	/// Initialize sound handlers
	//
	/// For now this function initializes a single sound handler,
	/// the one enabled at configure time.
	/// In the future it might initialize multiple ones (maybe)
	///
	void initTestingSoundHandlers();

	/// Initialize media handlers
	//
	/// For now this function initializes a single media handler,
	/// the one enabled at configure time.
	/// In the future it might initialize multiple ones (maybe)
	///
	void initTestingMediaHandlers();

	/// Render the current movie to all testing renderers
	//
	/// This function calls movie_root::display internally
	///
	void render();

	/// Render the current movie to a specific testing renderer
	//
	/// @param renderer
	///	The renderer to draw to. It will be temporarly set as
	///	the global renderer in the gnash core lib.
	///
	/// @param invalidated
	///	The invalidated ranges as computed by the core lib.
	///
	void render(render_handler& renderer, InvalidatedRanges& invalidated);

	/// Add a testing renderer to the list, initializing it with current viewport size
	void addTestingRenderer(std::auto_ptr<render_handler> h, const std::string& name);

	gnash::movie_root* _movie_root;

	gnash::movie_definition* _movie_def;

	gnash::movie_instance* _movie;

    boost::shared_ptr<sound::sound_handler> _sound_handler;

    std::auto_ptr<RunInfo> _runInfo;
	/// Current pointer position - X ordinate
	int _x;

	/// Current pointer position - Y ordinate
	int _y;

	/// Current viewport width
	unsigned _width;

	/// Current viewport height
	unsigned _height;

	/// Invalidated bounds of the movie after last
	/// advance call. They are cached here so we
	/// can safely call ::display w/out wiping this
	/// information out.
	InvalidatedRanges _invalidatedBounds;

	/// I'd use ptr_list here, but trying not to spread
	/// boost 1.33 requirement for the moment.
	/// Still, I'd like to simplify things...
	/// is shared_ptr fine ?
	typedef std::vector< TestingRendererPtr > TRenderers;

	std::vector< TestingRendererPtr > _testingRenderers;

	// When true, pass world invalidated ranges
	// to the renderer(s) at ::render time.
	bool _forceRedraw;

	/// Virtual clock to use to let test runners
	/// control time flow
	ManualClock _clock;
	//std::auto_ptr<VirtualClock> _clock;
};

// exp2 isn't part of standard C++, so is defined here in case the compiler
// doesn't supply it (e.g. in BSD)
#ifndef HAVE_EXP2
inline double	exp2(double x) { return std::pow((double)2, double(x)); }
#endif

} // namespace gnash

#endif // _GNASH_MOVIETESTER_H

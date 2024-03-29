// 
//   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010,
//   2011 Free Software Foundation, Inc
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

#ifndef GNASH_SHAREDLIB_H
#define GNASH_SHAREDLIB_H

#ifdef HAVE_CONFIG_H
#include "gnashconfig.h"
#endif

#include <boost/thread/mutex.hpp>
#include <string>
#include "dsodefs.h" // DSOEXPORT

#ifdef _WIN32
#undef DLL_EXPORT
#define LIBLTDL_DLL_IMPORT 1
#endif

#include <ltdl.h>

// Used on Darwin for basename
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif

namespace gnash {
    class as_object;
}

namespace gnash {

/// TODO: document this class
class SharedLib
{
    typedef boost::mutex::scoped_lock scoped_lock;

public:
    // Typedefs for function pointers to keep the code readable
    typedef bool entrypoint (void *obj);
    typedef void initentry(as_object &obj);
    
    DSOEXPORT SharedLib(const std::string& filespec);

    DSOEXPORT bool openLib();
    bool openLib(const std::string &filespec);
    
    // Get a C symbol from the shared library based on the name
    entrypoint *getDllSymbol (const std::string& symbol);
    DSOEXPORT initentry *getInitEntry (const std::string& symbol);
    
private:

    bool closeLib();

    lt_dlhandle _dlhandle;
    std::string _filespec;
    boost::mutex _libMutex;    
};

} // end of gnash namespace

// GNASH_SHAREDLIB_H
#endif

// Local Variables:
// mode: C++
// indent-tabs-mode: t
// End:

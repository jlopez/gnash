// 
//   Copyright (C) 2009 Free Software Foundation, Inc.
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

#ifdef HAVE_DEJAGNU_H

#include <string>
#include <sys/stat.h>
#include <cstdio>

#include "dejagnu.h"
#include "log.h"

#include "gst/AudioInputGst.h"
#include <vector>


using namespace gnash;
using namespace media;
using namespace gst;
using namespace std;

static TestState runtest;

static string infile;

static void test_client();

LogFile& dbglogfile = LogFile::getDefaultInstance();

int
main(int argc, char *argv[])
{   
    test_client();
    return 0;
}

static void test_client()
{
	//create a test class, call constructor
	gst::AudioInputGst aud;
	aud.findAudioDevs();
	
	vector<GnashAudio*> *audioVect = aud.getAudioVect();
	
    if (audioVect == NULL) {
        runtest.fail("the audiovector wasn't created.");
    } else {
        runtest.pass("the audiovector was created.");
    }
    if (audioVect->at(0)->getElementPtr() == NULL) {
        runtest.fail("the test audio source doesn't have an element ptr");
    } else {
        runtest.pass("the test audio source has a ptr set");
    }
    if (g_strcmp0(audioVect->at(0)->getGstreamerSrc(), "audiotestsrc") != 0) {
        runtest.fail("the test audio source has the wrong GstreamerSrc value");
    } else {
        runtest.pass("the test audio source has the correct GstreamerSrc value");
    }
    if (g_strcmp0(audioVect->at(0)->getProductName(), "audiotest") != 0) {
        runtest.fail("the test audio source has the wrong product name value");
    } else {
        runtest.pass("the test audio source has the right product name value");
    }
    
    int devselect;
    devselect = aud.makeAudioDevSelection();
    
    GnashAudioPrivate *audio = NULL;
    audio = aud.transferToPrivate(devselect);
    if (audio == NULL) {
        runtest.fail("the transferToPrivate function returned nothing");
    } else {
        runtest.pass("the transferToPrivate function returned a class");
    }
    if (audio->getAudioDevice() == NULL) {
        runtest.fail("the audioDevice variable wasn't set");
    } else {
        runtest.pass("the audioDevice variable was set");
    }
    if (audio->getDeviceName() == NULL) {
        runtest.fail("the productName variable wasn't set");
    } else {
        runtest.pass("the productName variable was set");
    }
    if (aud.getGlobalAudio() == NULL) {
        runtest.fail("the _globalAudio reference wasn't set");
    } else {
        runtest.pass("the _globalAudio reference was set during transferToPrivate");
    }
    
    bool ok = false;
    ok = aud.audioCreateMainBin(audio);
    if (ok != true) {
        runtest.fail("audioCreateMainBin() function reported an error");
    } else {
        runtest.pass("audioCreateMainBin() function reported no errors");
    }
    if (audio->_pipeline == NULL) {
        runtest.fail("the audio pipeline wasn't created or a reference wasn't stored");
    } else {
        runtest.pass("the audio pipeline reference is in the private class");
    }
    if (audio->_audioMainBin == NULL) {
        runtest.fail("the audioMainBin wasn't created or a reference wasn't stored");
    } else {
        runtest.pass("the audioMainBin reference is in the private class");
    }
    if (audio->_audioSourceBin == NULL) {
        runtest.fail("the audioSourceBin wasn't created or a reference wasn't store");
    } else {
        runtest.pass("the audioSourceBin reference is in the private class");
    }
    
    ok = aud.audioCreatePlaybackBin(audio);
    if (ok != true) {
        runtest.fail("audioCreatePlaybackBin() function reported an error");
    } else {
        runtest.pass("audioCreatePlaybackBin() function reported no errors");
    }
    if (audio->_audioPlaybackBin == NULL) {
        runtest.fail("the audioPlaybackBin wasn't created or a reference wasn't stored");
    } else {
        runtest.pass("the audioPlaybackBin reference is in the private class");
    }
    
    ok = aud.audioCreateSaveBin(audio);
    if (ok != true) {
        runtest.fail("audioCreateSaveBin() function reported an error");
    } else {
        runtest.pass("audioCreateSaveBin() function reported no errors");
    }
    if (audio->_audioSaveBin == NULL) {
        runtest.fail("the audioSaveBin wasn't created or a reference wasn't stored");
    } else {
        runtest.pass("the audioSaveBin reference is in the private class");
    }
    
    ok = aud.makeAudioSourcePlaybackLink(audio);
    if (ok != true) {
        runtest.fail("makeAudioSourcePlaybackLink() function reported an error");
    } else {
        runtest.pass("makeAudioSourcePlaybackLink() function reported no errors");
    }
    
    ok = aud.makeAudioSourceSaveLink(audio);
    if (ok != true) {
        runtest.fail("makeAudioSourceSaveLink() function reported an error");
    } else {
        runtest.pass("makeAudioSourceSaveLink() function reported no errors");
    }
    
    ok = aud.audioPlay(audio);
    if (ok != true) {
        runtest.fail("audioPlay() function reported an error");
    } else {
        runtest.pass("audioPlay() function reported no errors");
    }
    
    //sleep to record a few seconds of audio (from mic or test source)
    sleep(5);
    
    ok = aud.audioStop(audio);
    if (ok != true) {
        runtest.fail("audioStop() function reported an error");
    } else {
        runtest.pass("audioStop() function reported no errors");
    }
    
    struct stat st;
    std::string file = "./audioOut.ogg";
    if (stat(file.c_str(), &st) == 0) {
        runtest.pass("audioOut.ogg file is in testsuite/libmedia.all");
        if (st.st_blocks == 0) {
            runtest.fail("the output file is there, but there's no data in it!");
        } else {
            runtest.pass("the output file has data in it");
        }
    } else {
        runtest.fail("there's no output video file in testsuite/libmedia.all");
    }
    
}


#else

int
main(int /*argc*/, char /* *argv[]*/)
{
  // nop
    cerr << "This program needs to have DejaGnu installed!" << endl;
    return 0;  
}

#endif

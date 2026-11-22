/*
    Audio TnM - Audio Transcribtion and Modification tool
    is a tool for demonstrating a method to analyse and
    make a report of voice recordings by providing a comfortable
    interface that allows one to transcribe and read the
    conversation at hand using means of Artificial Intelligence,
    listen to parts of the conversation to verify the text
    and modify the audio in a way to encrypt sensitive chunks
    while maintaining the ability to listen to the audio.

    This project depends on third party tools, such as:
    - ffmpeg            by ffmpeg.org           v 9.0.2
        + libmp3lame    by lame.sourceforge.io  v 3.100
        + libopus       by opus-codec.org       v 1.6.1
    - libsodium         by libsodium.org        v 1.0.22
    - miniaudio         by miniaud.io           v 0.11.26
    - whisper.cpp       by ggml.org             v 1.9.5
    - wxWidgets         by wxwidgets.org        v 3.2.12
    Make sure to pay respect to them and precompile those
    tools in external_libs before compiling this one,
    except miniaudio which goes into "utils" (both .c and .h).

    This project also is created as a part of Master's Degree
    thesis for NTUU "Igor Sikorsky Kyiv Polytechnic Institute"

    Created by
    Skorobagatko I.A. IO-51mp
*/

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>

struct LeakDetector
{
    LeakDetector()
    {
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    }
};
static LeakDetector g_leakDetector;
#endif

#include "gui/gui.hpp"
#include "utils/config.hpp"

DECLARE_APP(AudioApp);
// This macro generates the platform-specific main() entry point
IMPLEMENT_APP(AudioApp);

bool AudioApp::OnInit()
{
    MainFrame *frame = new MainFrame(wxString::FromUTF8(app_name));
    frame->Show(true);
    return true;
}

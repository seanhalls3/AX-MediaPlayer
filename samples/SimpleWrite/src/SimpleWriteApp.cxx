//
//  SimpleWriteApp.cxx
//  SimpleWriteApp
//
//  Created by Andrew Wright on 17/08/21.
//  (c) 2021 AX Interactive
//

#include "cinder/app/RendererGl.h"
#include "cinder/app/App.h"
#include "cinder/gl/gl.h"
#include "cinder/audio/audio.h"
#include "AX-MediaPlayer.h"
#include "guiddef.h"

#include "MediaWriter.h"

#pragma comment(lib, "Mfreadwrite.lib")

#if __has_include( "cinder/CinderImGui.h")
#include "cinder/CinderImGui.h"
#define HAS_DEBUG_UI
namespace ui = ImGui;
#endif

#undef HAS_DEBUG_UI

#define FORCE_NVIDIA_CARD_IF_PRESENT

#ifdef FORCE_NVIDIA_CARD_IF_PRESENT
extern "C"
{
    __declspec( dllexport ) int NvOptimusEnablement = 0x00000001; // This forces Intel GPU / Nvidia card selection
}
#endif

using namespace ci;
using namespace ci::app;

class SimpleWriteApp : public app::App
{
public:
    void setup ( ) override;
    void update ( ) override;
    void draw ( ) override;
    void fileDrop ( FileDropEvent event ) override;
    void keyDown ( KeyEvent event ) override;

protected:
    void connectSignals ( );
    void loadDefaultVideo ( );

    AX::Video::MediaWriterRef     _writer;

    AX::Video::MediaPlayerRef     _player;
    AX::Video::MediaPlayer::Error _error{ AX::Video::MediaPlayer::Error::NoError };

    bool _hardwareAccelerated{ true };
    bool _approximateSeeking{ true };
    gl::TextureRef _texture;

    ci::gl::FboRef _fbo;


};

void SimpleWriteApp::setup ( )
{
#ifdef HAS_DEBUG_UI
    ui::Initialize ( );
#endif


    console ( ) << gl::getString ( GL_RENDERER ) << std::endl;
    console ( ) << gl::getString ( GL_VERSION ) << std::endl;

    loadDefaultVideo ( );
}

void SimpleWriteApp::loadDefaultVideo ( )
{
    auto fmt = AX::Video::MediaPlayer::Format ( ).HardwareAccelerated ( _hardwareAccelerated );
    _player = AX::Video::MediaPlayer::Create ( CINDER_PATH "/samples/QuickTimeBasic/assets/bbb.mp4", fmt );
    connectSignals ( );
}

void SimpleWriteApp::keyDown ( KeyEvent event )
{
    if (event.getChar ( ) == 'r')
    {
        loadDefaultVideo ( );
    }
}

void SimpleWriteApp::fileDrop ( FileDropEvent event )
{
    _error = AX::Video::MediaPlayer::Error::NoError;

    auto fmt = AX::Video::MediaPlayer::Format ( ).HardwareAccelerated ( _hardwareAccelerated );
    _player = AX::Video::MediaPlayer::Create ( loadFile ( event.getFile ( 0 ) ), fmt );
    connectSignals ( );
    _player->Play ( );
}

void SimpleWriteApp::connectSignals ( )
{
    _player->OnSeekStart.connect ( [=] { std::cout << "OnSeekStart\n"; } );
    _player->OnSeekEnd.connect ( [=] { std::cout << "OnSeekEnd\n"; } );
    _player->OnComplete.connect ( [=]
        {
            std::cout << "OnComplete\n";
            _writer->finalize ( );
            std::this_thread::sleep_for ( std::chrono::seconds ( 1 ) );
            app:quit ( );
        } );
    _player->OnReady.connect ( [=]
        {
            std::cout << "OnReady: " << _player->GetDurationInSeconds ( ) << std::endl;

            _fbo = ci::gl::Fbo::create ( _player->GetSize ( ).x, _player->GetSize ( ).y, ci::gl::Fbo::Format ( ).disableDepth ( ) );

            _writer = AX::Video::MediaWriter::Create ( _player->GetSize ( ) );

            if (_writer)
            {
                setWindowSize ( _player->GetSize ( ) );
                _player->Play ( );
            }
        } );
    _player->OnError.connect ( [=] ( AX::Video::MediaPlayer::Error error ) { _error = error; } );
    _player->OnBufferingStart.connect ( [=] { std::cout << "OnBufferingStart\n"; } );
    _player->OnBufferingEnd.connect ( [=] { std::cout << "OnBufferingEnd\n"; } );
}

void SimpleWriteApp::update ( )
{
    if (_player->IsHardwareAccelerated ( ))
    {
        // You can still use this method in software rendered mode
        // but it will create a new texture every time it's called
        // even if ::CheckNewFrame() was false. Use the above method 
        // for optimal texture creation in software mode but if you 
        // don't care, this block is functionally identical in both paths
        if (auto lease = _player->GetTexture ( ))
        {
            //gl::scale(1, -1, 1);
            // You can now use this texture until `lease` goes out
            // of scope (it will Unlock() the texture when destructing )
            //gl::draw ( *lease, getWindowBounds());

            auto textureRef = lease->ToTexture ( );
            if (_fbo)
            {
                {
                    ci::gl::ScopedFramebuffer scopedFbo ( _fbo );
                    ci::gl::ScopedModelMatrix scopedMM;
                    ci::gl::translate ( 0.0f, ( float ) _player->GetSize ( ).y, 0.0f );
                    ci::gl::scale ( 1.0f, -1.0f, 1.0f );
                    ci::gl::draw ( textureRef );
                }

                _writer->write ( _fbo, _player->GetSize ( ) );
            }
        }
    }
}



void SimpleWriteApp::draw ( )
{
    gl::clear ( Colorf::black ( ) );

    auto s = std::to_string ( ( _player->GetPositionInSeconds ( ) / _player->GetDurationInSeconds ( ) ) * 100 ) + "%";
    ci::gl::drawStringCentered ( s, ci::app::getWindowCenter ( ) );

    if (_player->CheckNewFrame ( ))
    {
        // Only true if using the CPU render path
        if (auto surface = _player->GetSurface ( ))
        {
            _texture = *_player->GetTexture ( );
        }
    }
}

void Init ( App::Settings* settings )
{
#ifdef CINDER_MSW
    settings->setConsoleWindowEnabled ( );
#endif
}

CINDER_APP ( SimpleWriteApp, RendererGl ( RendererGl::Options ( ) ), Init );

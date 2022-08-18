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
/*
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <Mfreadwrite.h>
#include <mferror.h>
*/
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
    __declspec(dllexport) int NvOptimusEnablement = 0x00000001; // This forces Intel GPU / Nvidia card selection
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
    void keyDown( KeyEvent event ) override;

protected:
    void connectSignals();
    void loadDefaultVideo();
    /*
    HRESULT setupWriter();
    HRESULT initializeSinkWriter ( IMFSinkWriter** ppWriter, DWORD* pStreamIndex );
    HRESULT copyPixels();
    HRESULT writeFrame ( IMFSinkWriter* pWriter, DWORD streamIndex, const LONGLONG& rtStart, const glm::ivec2 size, const LONGLONG& frameDuration, BYTE* videoBuffer );
    HRESULT finalizeAndReleaseWriter();
    IMFSinkWriter* _pSinkWriter = NULL;
    DWORD _stream;
    LONGLONG _rtStart = 0;
    long _videoFrameDuration = 0;
    std::vector<DWORD> _videoFrameBuffer;
    */
    AX::Video::MediaWriterRef     _writer;

    AX::Video::MediaPlayerRef     _player;
    AX::Video::MediaPlayer::Error _error{ AX::Video::MediaPlayer::Error::NoError };

    bool _hardwareAccelerated{ true };
    bool _approximateSeeking{ true };
    gl::TextureRef _texture;

    ci::gl::FboRef _fbo;

    /*
    template <class T> void SafeRelease(T** ppT)
    {
        if (*ppT)
        {
            (*ppT)->Release();
            *ppT = NULL;
        }
    }
    */
};

void SimpleWriteApp::setup ( )
{
#ifdef HAS_DEBUG_UI
    ui::Initialize ( );
#endif

  
    console() << gl::getString(GL_RENDERER) << std::endl;
    console() << gl::getString(GL_VERSION) << std::endl;

    loadDefaultVideo();
}

void SimpleWriteApp::loadDefaultVideo()
{
    auto fmt = AX::Video::MediaPlayer::Format().HardwareAccelerated( _hardwareAccelerated );
    //_writer = AX::Video::MediaWriter::Create();
    _player = AX::Video::MediaPlayer::Create( CINDER_PATH "/samples/QuickTimeBasic/assets/bbb.mp4", fmt );
    connectSignals();
    //_player->Play();
}

void SimpleWriteApp::keyDown( KeyEvent event )
{
    if( event.getChar() == 'r' )
    {
        loadDefaultVideo();
    }
}

void SimpleWriteApp::fileDrop ( FileDropEvent event )
{
    _error = AX::Video::MediaPlayer::Error::NoError;

    auto fmt = AX::Video::MediaPlayer::Format ( ).HardwareAccelerated ( _hardwareAccelerated );
    _player = AX::Video::MediaPlayer::Create ( loadFile ( event.getFile ( 0 ) ), fmt );
    connectSignals();
    _player->Play ( );
}

void SimpleWriteApp::connectSignals()
{
    _player->OnSeekStart.connect( [=] { std::cout << "OnSeekStart\n"; } );
    _player->OnSeekEnd.connect( [=] { std::cout << "OnSeekEnd\n"; } );
    _player->OnComplete.connect( [=] 
        { 
            std::cout << "OnComplete\n";
            _writer->finalize();
            //_writer.reset();
        } );
    _player->OnReady.connect( [=] 
        { 
            std::cout << "OnReady: " << _player->GetDurationInSeconds() << std::endl; 

            _fbo = ci::gl::Fbo::create(_player->GetSize().x, _player->GetSize().y, ci::gl::Fbo::Format().disableDepth());
            //_fbo->getColorTexture()->setTopDown(false); // Flip the y axis.

            //HRESULT hr = _writer->setupWriter(_player->GetSize());
            _writer = AX::Video::MediaWriter::Create(_player->GetSize());

            if (_writer)
            {
                setWindowSize(_player->GetSize());
                _player->Play();
            }
        } );
    _player->OnError.connect( [=] ( AX::Video::MediaPlayer::Error error ) { _error = error; } );
    _player->OnBufferingStart.connect( [=] { std::cout << "OnBufferingStart\n"; } );
    _player->OnBufferingEnd.connect( [=] { std::cout << "OnBufferingEnd\n"; } );
}

void SimpleWriteApp::update ( )
{
    if (_player->IsHardwareAccelerated())
    {
        // You can still use this method in software rendered mode
        // but it will create a new texture every time it's called
        // even if ::CheckNewFrame() was false. Use the above method 
        // for optimal texture creation in software mode but if you 
        // don't care, this block is functionally identical in both paths
        if (auto lease = _player->GetTexture())
        {
            //gl::scale(1, -1, 1);
            // You can now use this texture until `lease` goes out
            // of scope (it will Unlock() the texture when destructing )
            //gl::draw ( *lease, getWindowBounds());

            auto textureRef = lease->ToTexture();
            if (_fbo)
            {
                {
                    ci::gl::ScopedFramebuffer scopedFbo(_fbo);
                    ci::gl::ScopedModelMatrix scopedMM;
                    ci::gl::translate(0.0f, (float)_player->GetSize().y, 0.0f);
                    ci::gl::scale(1.0f, -1.0f, 1.0f);
                    ci::gl::draw(textureRef);
                }

                //_writer->copyPixels(lease->ToTexture(), _player->GetSize());
                _writer->write(_fbo, _player->GetSize());
            }
        }
    }
}

#ifdef HAS_DEBUG_UI
struct ScopedWindow2
{
    ScopedWindow2 ( const char * title, uint32_t flags )
    {
        ui::Begin ( title, nullptr, flags );
    }

    ~ScopedWindow2 ( )
    {
        ui::End ( );
    }
};
#endif

void SimpleWriteApp::draw ( )
{
    gl::clear ( Colorf::black ( ) );

#ifdef HAS_DEBUG_UI
    {
        ScopedWindow2 window{ "Settings", ImGuiWindowFlags_AlwaysAutoResize };

        ui::Checkbox ( "Use H/W Acceleration", &_hardwareAccelerated );
        if ( _error != AX::Video::MediaPlayer::Error::NoError )
        {
            ui::TextColored ( ImVec4 ( 0.8f, 0.1f, 0.1f, 1.0f ), "Error: %s", AX::Video::MediaPlayer::ErrorToString ( _error ).c_str ( ) );
            return;
        }

        if ( !_player ) return;

        float position = _player->GetPositionInSeconds ( );
        float duration = _player->GetDurationInSeconds ( );
        float percent = position / duration;

        ui::Text ( "%.2f FPS", getAverageFps ( ) );
        ui::Text ( "Hardware Accelerated: %s, HasAudio: %s, HasVideo : %s", _player->IsHardwareAccelerated() ? "true" : "false", _player->HasAudio ( ) ? "true" : "false", _player->HasVideo ( ) ? "true" : "false" );

        ui::Checkbox ( "Approximate Seeking", &_approximateSeeking );
        if ( ui::SliderFloat ( "Seek", &percent, 0.0f, 1.0f ) )
        {
            _player->SeekToPercentage ( percent, _approximateSeeking );
        }

        float rate = _player->GetPlaybackRate ( );
        if ( ui::SliderFloat ( "Playback Rate", &rate, -2.5f, 2.5f ) )
        {
            _player->SetPlaybackRate ( rate );
        }

        if ( _player->IsPaused ( ) )
        {
            if ( ui::Button ( "Play" ) )
            {
                _player->Play ( );
            }
        }
        else if ( _player->IsPlaying ( ) )
        {
            if ( ui::Button ( "Pause" ) )
            {
                _player->Pause ( );
            }
        }

        ui::SameLine ( );
        if ( ui::Button ( "Toggle" ) ) _player->TogglePlayback ( );
        
        ui::SameLine ( );
        bool loop = _player->IsLooping ( );
        if ( ui::Checkbox ( "Loop", &loop ) ) _player->SetLoop ( loop );

        ui::SameLine ( );
        bool mute = _player->IsMuted ( );
        if ( ui::Checkbox ( "Mute", &mute ) ) _player->SetMuted ( mute );

        if ( !mute )
        {
            float volume = _player->GetVolume ( );
            if ( ui::DragFloat ( "Volume", &volume, 0.01f, 0.0f, 1.0f ) )
            {
                _player->SetVolume ( volume );
            }
        }
    }
#endif
    auto s = std::to_string((_player->GetPositionInSeconds() / _player->GetDurationInSeconds()) * 100) + "%";
    ci::gl::drawStringCentered(s, ci::app::getWindowCenter());

    if ( _player->CheckNewFrame ( ) )
    {
        // Only true if using the CPU render path
        if ( auto surface = _player->GetSurface ( ) )
        {
            _texture = *_player->GetTexture ( );
        }
    }

    /*
    if ( _player->IsHardwareAccelerated ( ) )
    {
        // You can still use this method in software rendered mode
        // but it will create a new texture every time it's called
        // even if ::CheckNewFrame() was false. Use the above method 
        // for optimal texture creation in software mode but if you 
        // don't care, this block is functionally identical in both paths
        if (auto lease = _player->GetTexture())
        {
            //gl::scale(1, -1, 1);
            // You can now use this texture until `lease` goes out
            // of scope (it will Unlock() the texture when destructing )
            //gl::draw ( *lease, getWindowBounds());

            auto textureRef = lease->ToTexture();
            if ( _fbo )
            {
                {
                    ci::gl::ScopedFramebuffer scopedFbo(_fbo);
                    ci::gl::draw(textureRef);
                }
            
                //_writer->copyPixels(lease->ToTexture(), _player->GetSize());
                _writer->write(_fbo, _player->GetSize());
            }
        }
    }
    else
    {
        if (_texture)
        {
            gl::draw(_texture, getWindowBounds());
        }
    }
    */
}
/*
HRESULT SimpleWriteApp::setupWriter()
{
    std::cout << "width: " << _player->GetSize().x << " ";
    std::cout << "height: " << _player->GetSize().y << "\n";
    //videoFrameBuffer = new DWORD[VIDEO_PELS];
    _videoFrameBuffer.resize(_player->GetSize().x * _player->GetSize().y);//VIDEO_PELS);
    _videoFrameBuffer.clear();

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        hr = MFStartup(MF_VERSION);
        if (SUCCEEDED(hr))
        {
            hr = initializeSinkWriter(&_pSinkWriter, &_stream);
        }
    }

    if (SUCCEEDED(hr))
    {
        _rtStart = 0;
    }

    return hr;
}

HRESULT SimpleWriteApp::finalizeAndReleaseWriter()
{
    HRESULT hr = _pSinkWriter->Finalize();
    SafeRelease(&_pSinkWriter);

    MFShutdown();
    CoUninitialize();

    return hr;
}

HRESULT SimpleWriteApp::copyPixels()
{
    HRESULT hr = E_NOTIMPL;

    if (_pSinkWriter)
    {
        glReadnPixels(0, 0, _player->GetSize().x, _player->GetSize().y, GL_RGBA, GL_UNSIGNED_BYTE, (_player->GetSize().x * _player->GetSize().y) * sizeof(DWORD), _videoFrameBuffer.data());
        hr = writeFrame(_pSinkWriter, _stream, _rtStart, _player->GetSize(), _videoFrameDuration, (BYTE*)_videoFrameBuffer.data());// timeSinceLastDraw);
        if (!SUCCEEDED(hr))
        {
            std::cout << "error on write\n";
            return hr;
        }
        _rtStart += _videoFrameDuration;
        return hr;
    }

    return hr;
}

HRESULT SimpleWriteApp::initializeSinkWriter(IMFSinkWriter** ppWriter, DWORD* pStreamIndex)
{
    *ppWriter = NULL;
    *pStreamIndex = NULL;

    IMFSinkWriter* pSinkWriter = NULL;
    IMFMediaType* pMediaTypeOut = NULL;
    IMFMediaType* pMediaTypeIn = NULL;
    DWORD           streamIndex;
    
    IMFSourceReader* pMediaReader;
    IMFMediaType* pMediaType;

    int fps = 30;
    UINT32 video_bitrate = 0;

    HRESULT hr = MFCreateSourceReaderFromURL(L"bbb.mp4", NULL, &pMediaReader);
    if (SUCCEEDED(hr))
    {
        hr = pMediaReader->GetCurrentMediaType(1, &pMediaType);
        if (SUCCEEDED(hr))
        {
            UINT32 pNumerator;
            UINT32 pDenominator;
            hr = MFGetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, &pNumerator, &pDenominator);
            if (SUCCEEDED(hr))
            {
                fps = (float)pNumerator / (float)pDenominator;
                _videoFrameDuration = 0.4055375 * (10 * 1000 * 1000 / fps);
            }

            video_bitrate = MFGetAttributeUINT32(pMediaType, MF_MT_AVG_BITRATE, 0);
            std::cout << "video_bitrate: " << video_bitrate << "\n";
        }
    }

    if (SUCCEEDED(hr))
    {
        hr = MFCreateSinkWriterFromURL(L"bbb_copy.mp4", NULL, NULL, &pSinkWriter);
    }
    
    // Set the output media type.
    if (SUCCEEDED(hr))
    {
        hr = MFCreateMediaType(&pMediaTypeOut);
    }
    if (SUCCEEDED(hr))
    {
        hr = pMediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    }
    if (SUCCEEDED(hr))
    {
        hr = pMediaTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    }
    if (SUCCEEDED(hr))
    {
        hr = pMediaTypeOut->SetUINT32(MF_MT_AVG_BITRATE, video_bitrate);
    }
    if (SUCCEEDED(hr))
    {
        hr = pMediaTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    }
    if (SUCCEEDED(hr))
    {
        hr = MFSetAttributeSize(pMediaTypeOut, MF_MT_FRAME_SIZE, _player->GetSize().x, _player->GetSize().y);//VIDEO_WIDTH, VIDEO_HEIGHT);
    }
    if (SUCCEEDED(hr))
    {
        hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_FRAME_RATE, fps * 1.2, 1);
    }
    if (SUCCEEDED(hr))
    {
        hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    }
    if (SUCCEEDED(hr))
    {
        hr = pSinkWriter->AddStream(pMediaTypeOut, &streamIndex);
    }

    // Set the input media type.
    if (SUCCEEDED(hr))
    {
        hr = MFCreateMediaType(&pMediaTypeIn);
    }
    if (SUCCEEDED(hr))
    {
        hr = pMediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    }
    if (SUCCEEDED(hr))
    {
        hr = pMediaTypeIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    }
    if (SUCCEEDED(hr))
    {
        hr = pMediaTypeIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    }
    if (SUCCEEDED(hr))
    {
        hr = MFSetAttributeSize(pMediaTypeIn, MF_MT_FRAME_SIZE, _player->GetSize().x, _player->GetSize().y);//VIDEO_WIDTH, VIDEO_HEIGHT);
    }
    if (SUCCEEDED(hr))
    {
        hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_FRAME_RATE, fps, 1);
    }
    if (SUCCEEDED(hr))
    {
        hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    }
    if (SUCCEEDED(hr))
    {
        hr = pSinkWriter->SetInputMediaType(streamIndex, pMediaTypeIn, NULL);
    }

    // Tell the sink writer to start accepting data.
    if (SUCCEEDED(hr))
    {
        hr = pSinkWriter->BeginWriting();
    }

    if (hr == MF_E_TOPO_CODEC_NOT_FOUND)
    {
        std::cout << "no codec\n";
    }

    // Return the pointer to the caller.
    if (SUCCEEDED(hr))
    {
        std::cout << "sink writer initialized\n";
        *ppWriter = pSinkWriter;
        (*ppWriter)->AddRef();
        *pStreamIndex = streamIndex;
    }

    SafeRelease(&pSinkWriter);
    SafeRelease(&pMediaTypeOut);
    SafeRelease(&pMediaTypeIn);
    SafeRelease(&pMediaType);
    SafeRelease(&pMediaReader);

    return hr;
}

HRESULT SimpleWriteApp::writeFrame(
    IMFSinkWriter* pWriter,
    DWORD streamIndex,
    const LONGLONG& rtStart,        // Time stamp.
    const glm::ivec2 size,
    const LONGLONG& frameDuration, 
    BYTE* videoBuffer
)
{
    IMFSample* pSample = NULL;
    IMFMediaBuffer* pBuffer = NULL;

    const LONG cbWidth = 4 * size.x;//VIDEO_WIDTH;
    const DWORD cbBuffer = cbWidth * size.y;//VIDEO_HEIGHT;

    BYTE* pData = NULL;

    // Create a new memory buffer.
    HRESULT hr = MFCreateMemoryBuffer(cbBuffer, &pBuffer);
    // Lock the buffer and copy the video frame to the buffer.
    if (SUCCEEDED(hr))
    {
        hr = pBuffer->Lock(&pData, NULL, NULL);
    }
    if (SUCCEEDED(hr))
    {
        hr = MFCopyImage(
            pData,                      // Destination buffer.
            cbWidth,                    // Destination stride.
            videoBuffer,    // First row in source image.
            cbWidth,                    // Source stride.
            cbWidth,                    // Image width in bytes.
            size.y//VIDEO_HEIGHT                // Image height in pixels.
        );
    }
    if (pBuffer)
    {
        pBuffer->Unlock();
    }

    // Set the data length of the buffer.
    if (SUCCEEDED(hr))
    {
        hr = pBuffer->SetCurrentLength(cbBuffer);
    }
    // Create a media sample and add the buffer to the sample.
    if (SUCCEEDED(hr))
    {
        hr = MFCreateSample(&pSample);
    }
    if (SUCCEEDED(hr))
    {
        hr = pSample->AddBuffer(pBuffer);
    }
    // Set the time stamp and the duration.
    if (SUCCEEDED(hr))
    {
        hr = pSample->SetSampleTime(rtStart);
    }
    if (SUCCEEDED(hr))
    {
        hr = pSample->SetSampleDuration(frameDuration);// VIDEO_FRAME_DURATION);
    }

    // Send the sample to the Sink Writer.
    if (SUCCEEDED(hr))
    {
        hr = pWriter->WriteSample(streamIndex, pSample);
    }

    SafeRelease(&pSample);
    SafeRelease(&pBuffer);
    return hr;
}
*/

void Init ( App::Settings * settings )
{
#ifdef CINDER_MSW
    settings->setConsoleWindowEnabled ( );
#endif
}

CINDER_APP ( SimpleWriteApp, RendererGl ( RendererGl::Options() ), Init );

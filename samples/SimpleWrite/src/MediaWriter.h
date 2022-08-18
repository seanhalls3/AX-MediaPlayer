#pragma once
#include "cinder/app/RendererGl.h"
#include "cinder/app/App.h"
#include "cinder/gl/gl.h"
#include "cinder/audio/audio.h"
#include "guiddef.h"
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <Mfreadwrite.h>
#include <mferror.h>
#include <AX-MediaPlayer.h>

namespace AX::Video
{
    using MediaWriterRef = std::shared_ptr<class MediaWriter>;
    class MediaWriter : public ci::Noncopyable
    {
    public:
        static  MediaWriterRef Create(const glm::ivec2& size);
        MediaWriter(const glm::ivec2& size);
        ~MediaWriter();

        bool write(ci::gl::FboRef fbo, const ci::ivec2 &size);
        bool finalize();
    protected:
        HRESULT initializeSinkWriter(std::unique_ptr<IMFSinkWriter, std::function<void(IMFSinkWriter*)>> &ppWriter, DWORD& pStreamIndex, const glm::ivec2& size);
        HRESULT writeFrame(std::unique_ptr<IMFSinkWriter, std::function<void(IMFSinkWriter*)>>& pWriter, DWORD streamIndex, const LONGLONG& rtStart, const glm::ivec2& size, const LONGLONG& frameDuration, BYTE* videoBuffer);// std::shared_ptr<BYTE>& videoBuffer);
        std::unique_ptr<IMFSinkWriter, std::function<void(IMFSinkWriter*)>> _pSinkWriter;
        DWORD _stream = -1;
        LONGLONG _rtStart = 0;
        long _videoFrameDuration = 0;
        std::vector<DWORD> _videoFrameBuffer;
        bool _isReady = false;
        
        template <class T> void mfSafeRelease(T** ppT)
        {
            if (*ppT)
            {
                (*ppT)->Release();
                *ppT = NULL;
            }
        }
        
    };

}
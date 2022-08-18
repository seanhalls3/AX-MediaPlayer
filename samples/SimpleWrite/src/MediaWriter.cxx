#include "MediaWriter.h"

namespace AX::Video
{
    MediaWriterRef MediaWriter::Create(const glm::ivec2& size)
    {
        return MediaWriterRef(new MediaWriter(size));
    }

    MediaWriter::MediaWriter(const glm::ivec2& size) : _pSinkWriter(nullptr, [this](IMFSinkWriter* sw) { mfSafeRelease(&sw); })
    {
        _videoFrameBuffer.resize(size.x * size.y);//VIDEO_PELS);
        _videoFrameBuffer.clear();

        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (SUCCEEDED(hr))
        {
            hr = MFStartup(MF_VERSION);
            if (SUCCEEDED(hr))
            {
                hr = initializeSinkWriter(_pSinkWriter, _stream, size);
            }
        }

        if (SUCCEEDED(hr))
        {
            _rtStart = 0;
            _isReady = true;
        }
    }

    bool MediaWriter::finalize()
    {
        HRESULT hr = E_FAIL;

        if (_isReady)
        {
            hr = _pSinkWriter->Finalize();
            _isReady = false;
        }
        return SUCCEEDED(hr);
    }
    MediaWriter::~MediaWriter()
    {
        HRESULT hr = _pSinkWriter->Finalize();

        MFShutdown();
        CoUninitialize();
    }
    /*
    HRESULT MediaWriter::setupWriter(const glm::ivec2& size)
    {
        std::cout << "width: " << size.x << " ";
        std::cout << "height: " << size.y << "\n";
        //videoFrameBuffer = new DWORD[VIDEO_PELS];
        _videoFrameBuffer.resize(size.x * size.y);//VIDEO_PELS);
        _videoFrameBuffer.clear();

        //glGenBuffers(1, &_pbo);
        //glBindBuffer(GL_PIXEL_PACK_BUFFER, _pbo);
        //glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)0);// offset in bytes into "buffer", // not pointer to client memory!


        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (SUCCEEDED(hr))
        {
            hr = MFStartup(MF_VERSION);
            if (SUCCEEDED(hr))
            {
                hr = initializeSinkWriter(&_pSinkWriter.get(), &_stream, size);
            }
        }

        if (SUCCEEDED(hr))
        {
            _rtStart = 0;
        }

        return hr;
    }
    */
    /*
    HRESULT MediaWriter::finalizeAndReleaseWriter()
    {
        HRESULT hr = _pSinkWriter->Finalize();
        //SafeRelease(&_pSinkWriter);

        MFShutdown();
        CoUninitialize();

        return hr;
    }
    */
    //HRESULT MediaWriter::copyPixels(cinder::gl::Texture2dRef lease, const glm::ivec2 size)
    //HRESULT MediaWriter::write(MediaPlayer::FrameLease* lease, const glm::ivec2 size)
    bool MediaWriter::write(ci::gl::FboRef fbo, const glm::ivec2& size)
    {
        if (!_isReady)
            return false;

        HRESULT hr = E_FAIL;

        if (_pSinkWriter)
        {
            //glReadnPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, (size.x * size.y) * sizeof(DWORD), _videoFrameBuffer.data());
            auto surface = fbo->readPixels8u(fbo->getBounds());
            //glBindTexture(GL_TEXTURE_2D, textureRef->getId());
            //glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, (BYTE*)_videoFrameBuffer.data());
            //glGetTextureImage(textureRef->getId(), 0, GL_RGBA, GL_UNSIGNED_BYTE, (size.x * size.y) * sizeof(DWORD), _videoFrameBuffer.data());
            //BYTE* buffer = (BYTE*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

            //hr = writeFrame(_pSinkWriter, _stream, _rtStart, size, _videoFrameDuration, (BYTE*)_videoFrameBuffer.data());// timeSinceLastDraw);
            //hr = writeFrame(_pSinkWriter, _stream, _rtStart, size, _videoFrameDuration, buffer);// timeSinceLastDraw);
            //std::shared_ptr<BYTE> p(surface.getData());
            hr = writeFrame(_pSinkWriter, _stream, _rtStart, size, _videoFrameDuration, surface.getData());// timeSinceLastDraw);
            if (!SUCCEEDED(hr))
            {
                std::cout << "error on write\n";
                return false;
            }
            _rtStart += _videoFrameDuration;
            return SUCCEEDED(hr);
        }

        return SUCCEEDED(hr);
    }

    HRESULT MediaWriter::initializeSinkWriter(std::unique_ptr<IMFSinkWriter, std::function<void(IMFSinkWriter*)>>& ppWriter, DWORD& pStreamIndex, const glm::ivec2& size)
    {
        //*ppWriter = NULL;
        ppWriter.reset();
        pStreamIndex = NULL;

        //IMFSinkWriter* pSinkWriter = NULL;
        std::unique_ptr<IMFSinkWriter, std::function<void(IMFSinkWriter*)>> pSinkWriter(nullptr, [this](IMFSinkWriter* sw) { mfSafeRelease(&sw); });
        std::unique_ptr<IMFMediaType, std::function<void(IMFMediaType*)>> pMediaTypeOut(nullptr, [this](IMFMediaType* mt) { mfSafeRelease(&mt); });
        std::unique_ptr<IMFMediaType, std::function<void(IMFMediaType*)>> pMediaTypeIn(nullptr, [this](IMFMediaType* mt) { mfSafeRelease(&mt); });
        DWORD           streamIndex;

        std::unique_ptr<IMFSourceReader, std::function<void(IMFSourceReader*)>> pMediaReader(nullptr, [this](IMFSourceReader* sr) { mfSafeRelease(&sr); });
        std::unique_ptr<IMFMediaType, std::function<void(IMFMediaType*)>> pMediaType(nullptr, [this](IMFMediaType* mt) { mfSafeRelease(&mt); });

        int fps = 30;
        UINT32 video_bitrate = 0;

        //auto ppMediaReader = pMediaReader.get();
        IMFSourceReader* ppMediaReader;
        HRESULT hr = MFCreateSourceReaderFromURL(L"bbb.mp4", NULL, &ppMediaReader);
        pMediaReader.reset(ppMediaReader);
        if (SUCCEEDED(hr))
        {
            //auto ppMediaType = pMediaType.get();
            IMFMediaType* ppMediaType;
            hr = pMediaReader->GetCurrentMediaType(1, &ppMediaType);
            pMediaType.reset(ppMediaType);
            if (SUCCEEDED(hr))
            {
                UINT32 pNumerator;
                UINT32 pDenominator;
                hr = MFGetAttributeRatio(pMediaType.get(), MF_MT_FRAME_RATE, &pNumerator, &pDenominator);
                if (SUCCEEDED(hr))
                {
                    fps = (float)pNumerator / (float)pDenominator;
                    _videoFrameDuration = 0.4055375 * (10 * 1000 * 1000 / fps);
                }

                video_bitrate = MFGetAttributeUINT32(pMediaType.get(), MF_MT_AVG_BITRATE, 0);
                std::cout << "video_bitrate: " << video_bitrate << "\n";
            }
        }

        if (SUCCEEDED(hr))
        {
            //auto p = pSinkWriter.get();
            IMFSinkWriter* p;
            hr = MFCreateSinkWriterFromURL(L"bbb_copy.mp4", NULL, NULL, &p);
            pSinkWriter.reset(p);
        }

        // Set the output media type.
        if (SUCCEEDED(hr))
        {
            //auto p = pMediaTypeOut.get();
            IMFMediaType* p;
            hr = MFCreateMediaType(&p);
            pMediaTypeOut.reset(p);
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
            hr = MFSetAttributeSize(pMediaTypeOut.get(), MF_MT_FRAME_SIZE, size.x, size.y);//VIDEO_WIDTH, VIDEO_HEIGHT);
        }
        if (SUCCEEDED(hr))
        {
            hr = MFSetAttributeRatio(pMediaTypeOut.get(), MF_MT_FRAME_RATE, fps * 1.2, 1);
        }
        if (SUCCEEDED(hr))
        {
            hr = MFSetAttributeRatio(pMediaTypeOut.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        }
        if (SUCCEEDED(hr))
        {
            hr = pSinkWriter->AddStream(pMediaTypeOut.get(), &streamIndex);
        }

        // Set the input media type.
        if (SUCCEEDED(hr))
        {
            //auto p = pMediaTypeIn.get();
            IMFMediaType* p;
            hr = MFCreateMediaType(&p);
            pMediaTypeIn.reset(p);
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
            hr = MFSetAttributeSize(pMediaTypeIn.get(), MF_MT_FRAME_SIZE, size.x, size.y);//VIDEO_WIDTH, VIDEO_HEIGHT);
        }
        if (SUCCEEDED(hr))
        {
            hr = MFSetAttributeRatio(pMediaTypeIn.get(), MF_MT_FRAME_RATE, fps, 1);
        }
        if (SUCCEEDED(hr))
        {
            hr = MFSetAttributeRatio(pMediaTypeIn.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        }
        if (SUCCEEDED(hr))
        {
            hr = pSinkWriter->SetInputMediaType(streamIndex, pMediaTypeIn.get(), NULL);
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
            //*ppWriter = pSinkWriter;
            ppWriter = std::move(pSinkWriter);
            ppWriter.get()->AddRef();
            pStreamIndex = streamIndex;
        }

        //SafeRelease(&pSinkWriter);
        //SafeRelease(&pMediaTypeOut);
        //SafeRelease(&pMediaTypeIn);
        //SafeRelease(&pMediaType);
        //SafeRelease(&pMediaReader);
        //pMediaType.reset();
        //pMediaTypeIn.reset();
        //pMediaTypeOut.reset();
        //pMediaReader.reset();
        //pSinkWriter.reset();

        return hr;
    }

    HRESULT MediaWriter::writeFrame(
        std::unique_ptr<IMFSinkWriter, std::function<void(IMFSinkWriter*)>> &pWriter,
        DWORD streamIndex,
        const LONGLONG& rtStart,        // Time stamp.
        const glm::ivec2& size,
        const LONGLONG& frameDuration,
        //std::shared_ptr<BYTE> &videoBuffer
        BYTE* videoBuffer
    )
    {
        std::unique_ptr<IMFSample, std::function<void(IMFSample*)>> pSample(nullptr, [this](IMFSample* s) { mfSafeRelease(&s); });
        std::unique_ptr<IMFMediaBuffer, std::function<void(IMFMediaBuffer*)>> pBuffer(nullptr, [this](IMFMediaBuffer* mb) { mfSafeRelease(&mb); });

        const LONG cbWidth = 4 * size.x;//VIDEO_WIDTH;
        const DWORD cbBuffer = cbWidth * size.y;//VIDEO_HEIGHT;

        //std::unique_ptr<BYTE, std::function<void(BYTE*)>> pData(nullptr, [this](BYTE* b) { mfSafeRelease(&b); });
        BYTE* pData;

        // Create a new memory buffer.
        //auto p = pBuffer.get();
        IMFMediaBuffer* ppBuffer;
        HRESULT hr = MFCreateMemoryBuffer(cbBuffer, &ppBuffer);
        pBuffer.reset(ppBuffer);
        // Lock the buffer and copy the video frame to the buffer.
        if (SUCCEEDED(hr))
        {
            //auto p = pData.get();
            //BYTE* p;
            hr = pBuffer->Lock(&pData, NULL, NULL);
            //pData.reset(p);
        }
        if (SUCCEEDED(hr))
        {
            hr = MFCopyImage(
                pData,                      // Destination buffer.
                cbWidth,                    // Destination stride.
                videoBuffer,//.get(),    // First row in source image.
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
            //auto p = pSample.get();
            IMFSample* ppSample;
            hr = MFCreateSample(&ppSample);
            pSample.reset(ppSample);
        }
        if (SUCCEEDED(hr))
        {
            hr = pSample->AddBuffer(pBuffer.get());
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
            hr = pWriter->WriteSample(streamIndex, pSample.get());
        }

        //SafeRelease(&pSample);
        //SafeRelease(&pBuffer);
        pSample.reset();
        pBuffer.reset();

        return hr;
    }
}
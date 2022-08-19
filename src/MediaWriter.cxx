#include "MediaWriter.h"

namespace AX::Video
{
    MediaWriterRef MediaWriter::Create ( const ci::ivec2& size )
    {
        return MediaWriterRef ( new MediaWriter ( size ) );
    }

    MediaWriter::MediaWriter ( const ci::ivec2& size ) : _pSinkWriter ( nullptr, [this] ( IMFSinkWriter* sw ) { mfSafeRelease ( &sw ); } )
    {
        _videoFrameBuffer.resize ( size.x * size.y );
        _videoFrameBuffer.clear ( );

        HRESULT hr = CoInitializeEx ( NULL, COINIT_APARTMENTTHREADED );
        if (SUCCEEDED ( hr ))
        {
            hr = MFStartup ( MF_VERSION );
            if (SUCCEEDED ( hr ))
            {
                //hr = initializeSinkWriter ( _pSinkWriter, _stream, size );
                hr = InitializeSinkWriter ( size );
            }
        }

        if (SUCCEEDED ( hr ))
        {
            _fbo = ci::gl::Fbo::create ( size.x, size.y, ci::gl::Fbo::Format ( ).disableDepth ( ) );

            _rtStart = 0;
            _isReady = true;
        }
    }

    bool MediaWriter::Finalize ( )
    {
        HRESULT hr = E_FAIL;

        if (_isReady)
        {
            hr = _pSinkWriter->Finalize ( );
            _isReady = false;
        }
        return SUCCEEDED ( hr );
    }
    MediaWriter::~MediaWriter ( )
    {
        HRESULT hr = _pSinkWriter->Finalize ( );

        MFShutdown ( );
        CoUninitialize ( );
    }

    //bool MediaWriter::write ( ci::gl::FboRef fbo, const ci::ivec2& size )
    bool MediaWriter::Write ( ci::gl::TextureRef textureRef, const ci::ivec2& size, bool flip )
    {
        if (!_isReady)
            return false;

        HRESULT hr = E_FAIL;

        if (_pSinkWriter && _fbo)
        {
            if ( flip )
            {
                ci::gl::ScopedFramebuffer scopedFbo ( _fbo );
                ci::gl::ScopedModelMatrix scopedMM;
                ci::gl::translate ( 0.0f, ( float ) size.y, 0.0f );
                ci::gl::scale ( 1.0f, -1.0f, 1.0f );
                ci::gl::draw ( textureRef );
            }
            auto surface = _fbo->readPixels8u ( _fbo->getBounds ( ) );
            //hr = writeFrame ( _pSinkWriter, _stream, _rtStart, size, _videoFrameDuration, surface.getData ( ) );// timeSinceLastDraw);
            hr = WriteFrame ( size, surface.getData ( ) );// timeSinceLastDraw);
            if (!SUCCEEDED ( hr ))
            {
                std::cout << "error on write\n";
                return false;
            }
            _rtStart += _videoFrameDuration;
            return SUCCEEDED ( hr );
        }

        return SUCCEEDED ( hr );
    }

//    HRESULT MediaWriter::initializeSinkWriter ( std::unique_ptr<IMFSinkWriter, std::function<void ( IMFSinkWriter* )>>& ppWriter, DWORD& pStreamIndex, const ci::ivec2& size )
    HRESULT MediaWriter::InitializeSinkWriter ( const ci::ivec2& size )
    {
        //ppWriter.reset ( );
        //pStreamIndex = NULL;
        _stream = -1;

        std::unique_ptr<IMFSinkWriter, std::function<void ( IMFSinkWriter* )>> pSinkWriter ( nullptr, [this] ( IMFSinkWriter* sw ) { mfSafeRelease ( &sw ); } );
        std::unique_ptr<IMFMediaType, std::function<void ( IMFMediaType* )>> pMediaTypeOut ( nullptr, [this] ( IMFMediaType* mt ) { mfSafeRelease ( &mt ); } );
        std::unique_ptr<IMFMediaType, std::function<void ( IMFMediaType* )>> pMediaTypeIn ( nullptr, [this] ( IMFMediaType* mt ) { mfSafeRelease ( &mt ); } );
        DWORD           streamIndex;

        std::unique_ptr<IMFSourceReader, std::function<void ( IMFSourceReader* )>> pMediaReader ( nullptr, [this] ( IMFSourceReader* sr ) { mfSafeRelease ( &sr ); } );
        std::unique_ptr<IMFMediaType, std::function<void ( IMFMediaType* )>> pMediaType ( nullptr, [this] ( IMFMediaType* mt ) { mfSafeRelease ( &mt ); } );

        int fps = 30;
        UINT32 video_bitrate = 0;

        IMFSourceReader* ppMediaReader;
        HRESULT hr = MFCreateSourceReaderFromURL ( L"bbb.mp4", NULL, &ppMediaReader );
        pMediaReader.reset ( ppMediaReader );
        if (SUCCEEDED ( hr ))
        {
            IMFMediaType* ppMediaType;
            hr = pMediaReader->GetCurrentMediaType ( 1, &ppMediaType );
            pMediaType.reset ( ppMediaType );
            if (SUCCEEDED ( hr ))
            {
                UINT32 pNumerator;
                UINT32 pDenominator;
                hr = MFGetAttributeRatio ( pMediaType.get ( ), MF_MT_FRAME_RATE, &pNumerator, &pDenominator );
                if (SUCCEEDED ( hr ))
                {
                    fps = ( float ) pNumerator / ( float ) pDenominator;
                    _videoFrameDuration = 0.4055375 * ( 10 * 1000 * 1000 / fps );
                }

                video_bitrate = MFGetAttributeUINT32 ( pMediaType.get ( ), MF_MT_AVG_BITRATE, 0 );
                std::cout << "video_bitrate: " << video_bitrate << "\n";
            }
        }

        if (SUCCEEDED ( hr ))
        {
            IMFSinkWriter* p;
            hr = MFCreateSinkWriterFromURL ( L"bbb_copy.mp4", NULL, NULL, &p );
            pSinkWriter.reset ( p );
        }

        // Set the output media type.
        if (SUCCEEDED ( hr ))
        {
            IMFMediaType* p;
            hr = MFCreateMediaType ( &p );
            pMediaTypeOut.reset ( p );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = pMediaTypeOut->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = pMediaTypeOut->SetGUID ( MF_MT_SUBTYPE, MFVideoFormat_H264 );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = pMediaTypeOut->SetUINT32 ( MF_MT_AVG_BITRATE, video_bitrate );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = pMediaTypeOut->SetUINT32 ( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = MFSetAttributeSize ( pMediaTypeOut.get ( ), MF_MT_FRAME_SIZE, size.x, size.y );//VIDEO_WIDTH, VIDEO_HEIGHT);
        }
        if (SUCCEEDED ( hr ))
        {
            hr = MFSetAttributeRatio ( pMediaTypeOut.get ( ), MF_MT_FRAME_RATE, fps * 1.2, 1 );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = MFSetAttributeRatio ( pMediaTypeOut.get ( ), MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = pSinkWriter->AddStream ( pMediaTypeOut.get ( ), &streamIndex );
        }

        // Set the input media type.
        if (SUCCEEDED ( hr ))
        {
            IMFMediaType* p;
            hr = MFCreateMediaType ( &p );
            pMediaTypeIn.reset ( p );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = pMediaTypeIn->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = pMediaTypeIn->SetGUID ( MF_MT_SUBTYPE, MFVideoFormat_RGB32 );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = pMediaTypeIn->SetUINT32 ( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = MFSetAttributeSize ( pMediaTypeIn.get ( ), MF_MT_FRAME_SIZE, size.x, size.y );//VIDEO_WIDTH, VIDEO_HEIGHT);
        }
        if (SUCCEEDED ( hr ))
        {
            hr = MFSetAttributeRatio ( pMediaTypeIn.get ( ), MF_MT_FRAME_RATE, fps, 1 );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = MFSetAttributeRatio ( pMediaTypeIn.get ( ), MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = pSinkWriter->SetInputMediaType ( streamIndex, pMediaTypeIn.get ( ), NULL );
        }

        // Tell the sink writer to start accepting data.
        if (SUCCEEDED ( hr ))
        {
            hr = pSinkWriter->BeginWriting ( );
        }

        if (hr == MF_E_TOPO_CODEC_NOT_FOUND)
        {
            std::cout << "codec not found\n";
        }

        // Return the pointer to the caller.
        if (SUCCEEDED ( hr ))
        {
            //ppWriter = std::move ( pSinkWriter );
            _pSinkWriter = std::move ( pSinkWriter );
            //ppWriter.get ( )->AddRef ( );
            _pSinkWriter.get ( )->AddRef ( );
            //pStreamIndex = streamIndex;
            _stream = streamIndex;
        }

        return hr;
    }

    HRESULT MediaWriter::WriteFrame (
//        std::unique_ptr<IMFSinkWriter, std::function<void ( IMFSinkWriter* )>>& pWriter,
//        DWORD streamIndex,
//        const LONGLONG& rtStart,        // Time stamp.
        const ci::ivec2& size,
//        const LONGLONG& frameDuration,
        BYTE* videoBuffer
    )
    {
        std::unique_ptr<IMFSample, std::function<void ( IMFSample* )>> pSample ( nullptr, [this] ( IMFSample* s ) { mfSafeRelease ( &s ); } );
        std::unique_ptr<IMFMediaBuffer, std::function<void ( IMFMediaBuffer* )>> pBuffer ( nullptr, [this] ( IMFMediaBuffer* mb ) { mfSafeRelease ( &mb ); } );

        const LONG cbWidth = 4 * size.x;//VIDEO_WIDTH;
        const DWORD cbBuffer = cbWidth * size.y;//VIDEO_HEIGHT;

        //std::unique_ptr<BYTE, std::function<void(BYTE*)>> pData(nullptr, [this](BYTE* b) { mfSafeRelease(&b); });
        BYTE* pData;

        // Create a new memory buffer.
        IMFMediaBuffer* ppBuffer;
        HRESULT hr = MFCreateMemoryBuffer ( cbBuffer, &ppBuffer );
        pBuffer.reset ( ppBuffer );
        // Lock the buffer and copy the video frame to the buffer.
        if (SUCCEEDED ( hr ))
        {
            hr = pBuffer->Lock ( &pData, NULL, NULL );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = MFCopyImage (
                pData,          // Destination buffer.
                cbWidth,        // Destination stride.
                videoBuffer,    // First row in source image.
                cbWidth,        // Source stride.
                cbWidth,        // Image width in bytes.
                size.y          // Image height in pixels.
            );
        }
        if (pBuffer)
        {
            pBuffer->Unlock ( );
        }

        // Set the data length of the buffer.
        if (SUCCEEDED ( hr ))
        {
            hr = pBuffer->SetCurrentLength ( cbBuffer );
        }
        // Create a media sample and add the buffer to the sample.
        if (SUCCEEDED ( hr ))
        {
            IMFSample* ppSample;
            hr = MFCreateSample ( &ppSample );
            pSample.reset ( ppSample );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = pSample->AddBuffer ( pBuffer.get ( ) );
        }
        // Set the time stamp and the duration.
        if (SUCCEEDED ( hr ))
        {
            hr = pSample->SetSampleTime ( _rtStart );
        }
        if (SUCCEEDED ( hr ))
        {
            hr = pSample->SetSampleDuration ( _videoFrameDuration );
        }

        // Send the sample to the Sink Writer.
        if (SUCCEEDED ( hr ))
        {
            hr = _pSinkWriter->WriteSample ( _stream, pSample.get ( ) );
        }

        pSample.reset ( );
        pBuffer.reset ( );

        return hr;
    }
}
#include "MediaWriter.h"

namespace AX::Video
{
    MediaWriterRef MediaWriter::Create ( const ci::ivec2& size, int bitrate, int fps )
    {
        return MediaWriterRef ( new MediaWriter ( size, bitrate, fps ) );
    }

    MediaWriter::MediaWriter ( const ci::ivec2& size, int bitrate, int fps ) : _pSinkWriter ( nullptr, [this] ( IMFSinkWriter* sw ) { mfSafeRelease ( &sw ); } )
    {
        _size = size;
        _videoBitrate = bitrate;
        _fps = fps;

        _videoFrameDuration = 0.4055375 * ( 10 * 1000 * 1000 / fps );

        _videoFrameBuffer.resize ( _size.x * _size.y );
        _videoFrameBuffer.clear ( );

        HRESULT hr = CoInitializeEx ( nullptr, COINIT_APARTMENTTHREADED );
        if ( SUCCEEDED ( hr ) )
        {
            hr = MFStartup ( MF_VERSION );
            if ( SUCCEEDED ( hr ) )
            {
                hr = InitializeSinkWriter ( );
            }
        }

        if ( SUCCEEDED ( hr ) )
        {
            _fbo = ci::gl::Fbo::create ( _size.x, _size.y, ci::gl::Fbo::Format ( ).disableDepth ( ) );

            _rtStart = 0;
            _isReady = true;
        }
    }

    bool MediaWriter::Finalize ( )
    {
        HRESULT hr = E_FAIL;

        if ( _isReady )
        {
            hr = _pSinkWriter->Finalize ( );
            _isReady = false;
        }
        return SUCCEEDED ( hr );
    }
    MediaWriter::~MediaWriter ( )
    {
        Finalize ( );

        MFShutdown ( );
        CoUninitialize ( );
    }

    bool MediaWriter::Write ( ci::gl::TextureRef textureRef, bool flip )
    {
        if ( !_isReady )
            return false;

        HRESULT hr = E_FAIL;

        if ( _pSinkWriter && _fbo )
        {
            if ( flip )
            {
                ci::gl::ScopedFramebuffer scopedFbo ( _fbo );
                ci::gl::ScopedModelMatrix scopedMM;
                ci::gl::translate ( 0.0f, ( float ) _size.y, 0.0f );
                ci::gl::scale ( 1.0f, -1.0f, 1.0f );
                ci::gl::draw ( textureRef );
            }
            auto surface = _fbo->readPixels8u ( _fbo->getBounds ( ) );
            hr = WriteFrame ( surface.getData ( ) );
            if ( !SUCCEEDED ( hr ) )
            {
                ci::app::console ( ) << "error on write" << std::endl;
                return false;
            }
            _rtStart += _videoFrameDuration;
            return SUCCEEDED ( hr );
        }

        return SUCCEEDED ( hr );
    }

    HRESULT MediaWriter::InitializeSinkWriter ( )
    {
        _stream = -1;

        std::unique_ptr<IMFSinkWriter, std::function<void ( IMFSinkWriter* )>> pSinkWriter ( nullptr, [this] ( IMFSinkWriter* sw ) { mfSafeRelease ( &sw ); } );
        std::unique_ptr<IMFMediaType, std::function<void ( IMFMediaType* )>> pMediaTypeOut ( nullptr, [this] ( IMFMediaType* mt ) { mfSafeRelease ( &mt ); } );
        std::unique_ptr<IMFMediaType, std::function<void ( IMFMediaType* )>> pMediaTypeIn ( nullptr, [this] ( IMFMediaType* mt ) { mfSafeRelease ( &mt ); } );
        DWORD streamIndex;

        std::unique_ptr<IMFSourceReader, std::function<void ( IMFSourceReader* )>> pMediaReader ( nullptr, [this] ( IMFSourceReader* sr ) { mfSafeRelease ( &sr ); } );
        std::unique_ptr<IMFMediaType, std::function<void ( IMFMediaType* )>> pMediaType ( nullptr, [this] ( IMFMediaType* mt ) { mfSafeRelease ( &mt ); } );

        //int fps = 30;
        //UINT32 video_bitrate = 0;
        HRESULT hr = E_FAIL;

        /*
        IMFSourceReader* ppMediaReader;
        HRESULT hr = MFCreateSourceReaderFromURL ( L"bbb.mp4", nullptr, &ppMediaReader );
        pMediaReader.reset ( ppMediaReader );
        if ( SUCCEEDED ( hr ) )
        {
            IMFMediaType* ppMediaType;
            hr = pMediaReader->GetCurrentMediaType ( 1, &ppMediaType );
            pMediaType.reset ( ppMediaType );
            if ( SUCCEEDED ( hr ) )
            {
                UINT32 pNumerator;
                UINT32 pDenominator;
                hr = MFGetAttributeRatio ( pMediaType.get ( ), MF_MT_FRAME_RATE, &pNumerator, &pDenominator );
                if ( SUCCEEDED ( hr ) )
                {
                    fps = ( float ) pNumerator / ( float ) pDenominator;
                    ci::app::console ( ) << "fps: " << fps << std::endl;
                    _videoFrameDuration = 0.4055375 * ( 10 * 1000 * 1000 / fps );
                    ci::app::console ( ) << "video_frame_duration: " << _videoFrameDuration << std::endl;
                }

                video_bitrate = MFGetAttributeUINT32 ( pMediaType.get ( ), MF_MT_AVG_BITRATE, 0 );
                ci::app::console ( ) << "video_bitrate: " << video_bitrate << std::endl;
            }
        }
        */
         
        //if ( SUCCEEDED ( hr ) )
        {
            IMFSinkWriter* p;
            hr = MFCreateSinkWriterFromURL ( L"bbb_copy.mp4", nullptr, nullptr, &p );
            pSinkWriter.reset ( p );
        }

        // Set the output media type.
        if ( SUCCEEDED ( hr ) )
        {
            IMFMediaType* p;
            hr = MFCreateMediaType ( &p );
            pMediaTypeOut.reset ( p );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = pMediaTypeOut->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = pMediaTypeOut->SetGUID ( MF_MT_SUBTYPE, MFVideoFormat_H264 );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = pMediaTypeOut->SetUINT32 ( MF_MT_AVG_BITRATE, _videoBitrate );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = pMediaTypeOut->SetUINT32 ( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = MFSetAttributeSize ( pMediaTypeOut.get ( ), MF_MT_FRAME_SIZE, _size.x, _size.y );//VIDEO_WIDTH, VIDEO_HEIGHT);
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = MFSetAttributeRatio ( pMediaTypeOut.get ( ), MF_MT_FRAME_RATE, _fps * 1.2, 1 );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = MFSetAttributeRatio ( pMediaTypeOut.get ( ), MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = pSinkWriter->AddStream ( pMediaTypeOut.get ( ), &streamIndex );
        }

        // Set the input media type.
        if ( SUCCEEDED ( hr ) )
        {
            IMFMediaType* p;
            hr = MFCreateMediaType ( &p );
            pMediaTypeIn.reset ( p );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = pMediaTypeIn->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = pMediaTypeIn->SetGUID ( MF_MT_SUBTYPE, MFVideoFormat_RGB32 );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = pMediaTypeIn->SetUINT32 ( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = MFSetAttributeSize ( pMediaTypeIn.get ( ), MF_MT_FRAME_SIZE, _size.x, _size.y );//VIDEO_WIDTH, VIDEO_HEIGHT);
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = MFSetAttributeRatio ( pMediaTypeIn.get ( ), MF_MT_FRAME_RATE, _fps, 1 );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = MFSetAttributeRatio ( pMediaTypeIn.get ( ), MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = pSinkWriter->SetInputMediaType ( streamIndex, pMediaTypeIn.get ( ), nullptr );
        }

        // Tell the sink writer to start accepting data.
        if ( SUCCEEDED ( hr ) )
        {
            hr = pSinkWriter->BeginWriting ( );
        }

        if ( hr == MF_E_TOPO_CODEC_NOT_FOUND )
        {
            ci::app::console ( ) << "codec not found" << std::endl;
        }

        // Return the pointer to the caller.
        if ( SUCCEEDED ( hr ) )
        {
            _pSinkWriter = std::move ( pSinkWriter );
            _pSinkWriter.get ( )->AddRef ( );
            _stream = streamIndex;
        }

        return hr;
    }

    HRESULT MediaWriter::WriteFrame ( BYTE* videoBuffer )
    {
        std::unique_ptr<IMFSample, std::function<void ( IMFSample* )>> pSample ( nullptr, [this] ( IMFSample* s ) { mfSafeRelease ( &s ); } );
        std::unique_ptr<IMFMediaBuffer, std::function<void ( IMFMediaBuffer* )>> pBuffer ( nullptr, [this] ( IMFMediaBuffer* mb ) { mfSafeRelease ( &mb ); } );

        const LONG cbWidth = 4 * _size.x;
        const DWORD cbBuffer = cbWidth * _size.y;

        BYTE* pData;

        // Create a new memory buffer.
        IMFMediaBuffer* ppBuffer;
        HRESULT hr = MFCreateMemoryBuffer ( cbBuffer, &ppBuffer );
        pBuffer.reset ( ppBuffer );

        // Lock the buffer and copy the video frame to the buffer.
        if ( SUCCEEDED ( hr ) )
        {
            hr = pBuffer->Lock ( &pData, nullptr, nullptr );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = MFCopyImage (
                pData,          // Destination buffer.
                cbWidth,        // Destination stride.
                videoBuffer,    // First row in source image.
                cbWidth,        // Source stride.
                cbWidth,        // Image width in bytes.
                _size.y          // Image height in pixels.
            );
        }
        if ( pBuffer )
        {
            pBuffer->Unlock ( );
        }

        // Set the data length of the buffer.
        if ( SUCCEEDED ( hr ) )
        {
            hr = pBuffer->SetCurrentLength ( cbBuffer );
        }
        // Create a media sample and add the buffer to the sample.
        if ( SUCCEEDED ( hr ) )
        {
            IMFSample* ppSample;
            hr = MFCreateSample ( &ppSample );
            pSample.reset ( ppSample );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = pSample->AddBuffer ( pBuffer.get ( ) );
        }
        // Set the time stamp and the duration.
        if ( SUCCEEDED ( hr ) )
        {
            hr = pSample->SetSampleTime ( _rtStart );
        }
        if ( SUCCEEDED ( hr ) )
        {
            hr = pSample->SetSampleDuration ( _videoFrameDuration );
        }

        // Send the sample to the Sink Writer.
        if ( SUCCEEDED ( hr ) )
        {
            hr = _pSinkWriter->WriteSample ( _stream, pSample.get ( ) );
        }

        pSample.reset ( );
        pBuffer.reset ( );

        return hr;
    }
}
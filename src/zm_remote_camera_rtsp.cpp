﻿//
// ZoneMinder Remote Camera Class Implementation, $Date: 2009-06-03 09:10:28 +0100 (Wed, 03 Jun 2009) $, $Revision: 2907 $
// Copyright (C) 2001-2008 Philip Coombes
// 


#include "zm.h"

#if HAVE_LIBAVFORMAT

#include "zm_remote_camera_rtsp.h"
#include "zm_ffmpeg.h"
#include "zm_mem_utils.h"

#include <sys/types.h>
#include <sys/socket.h>

RemoteCameraRtsp::RemoteCameraRtsp( 
    int p_id, 
    const std::string &p_method, 
    const std::string &p_host, 
    const std::string &p_port, 
    const std::string &p_path, 
    int p_width, int p_height, 
    int p_colours, 
    int p_brightness, 
    int p_contrast, 
    int p_hue, 
    int p_colour, 
    bool p_capture ) :
        RemoteCamera( 
            p_id, 
            "rtsp",  /** 通过 RTSP 的方法取得视频 */
            p_host, 
            p_port, 
            p_path, 
            p_width, 
            p_height, 
            p_colours, 
            p_brightness, 
            p_contrast, 
            p_hue, 
            p_colour, 
            p_capture ),
    rtspThread( 0 )
{
    if ( p_method == "rtpUni" )
        method = RtspThread::RTP_UNICAST;
    else if ( p_method == "rtpMulti" )
        method = RtspThread::RTP_MULTICAST;
    else if ( p_method == "rtpRtsp" )
        method = RtspThread::RTP_RTSP;
    else if ( p_method == "rtpRtspHttp" )
        method = RtspThread::RTP_RTSP_HTTP;
    else
        Fatal( "Unrecognised method '%s' when creating RTSP camera %d", p_method.c_str(), id );

	if ( capture )
	{
		Initialise();
	}
}

RemoteCameraRtsp::~RemoteCameraRtsp()
{
	if ( capture )
	{
		Terminate();
	}
}

void RemoteCameraRtsp::Initialise()
{
    RemoteCamera::Initialise();

	int max_size = width*height*colours;

	buffer.size( max_size );

    if ( zm_dbg_level > ZM_DBG_INF )
        av_log_set_level( AV_LOG_DEBUG ); 
    else
        av_log_set_level( AV_LOG_QUIET ); 

    av_register_all();

    frameCount = 0;

    Connect();
}

void RemoteCameraRtsp::Terminate()
{
    avcodec_close( codecContext );
    av_free( codecContext );
    av_free( picture );

    Disconnect();
}

int RemoteCameraRtsp::Connect()
{
    /** 
        这里初始化了 Rtsp的 Socket的连接
    */
    rtspThread = new RtspThread( id, method, protocol, host, port, path, auth );

    rtspThread->start();

    return( 0 );
}

int RemoteCameraRtsp::Disconnect()
{
    if ( rtspThread )
    {
        rtspThread->stop();
        rtspThread->join();
        delete rtspThread;
        rtspThread = 0;
    }
    return( 0 );
}

int RemoteCameraRtsp::PrimeCapture()
{
    Debug( 2, "Waiting for sources" );
    for ( int i = 0; i < 100 && !rtspThread->hasSources(); i++ )
    {
        usleep( 100000 );
    }
    if ( !rtspThread->hasSources() )
        Fatal( "No RTSP sources" );

    Debug( 2, "Got sources" );

    formatContext = rtspThread->getFormatContext();

    // Find the first video stream
    int videoStream=-1;
    for ( int i = 0; i < formatContext->nb_streams; i++ )
        if ( formatContext->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO )
        {
            videoStream = i;
            break;
        }
    if ( videoStream == -1 )
        Fatal( "Unable to locate video stream" );

    // Get a pointer to the codec context for the video stream
    codecContext = formatContext->streams[videoStream]->codec;

    // Find the decoder for the video stream
    codec = avcodec_find_decoder( codecContext->codec_id );
    if ( codec == NULL )
        Fatal( "Unable to locate codec %d decoder", codecContext->codec_id );

    // Open codec
    if ( avcodec_open( codecContext, codec ) < 0 )
        Fatal( "Can't open codec" );

    picture = avcodec_alloc_frame();

    return( 0 );
}

int RemoteCameraRtsp::PreCapture()
{
    if ( !rtspThread->isRunning() )
        return( -1 );
    if ( !rtspThread->hasSources() )
    {
        Error( "Cannot precapture, no RTP sources" );
        return( -1 );
    }
    return( 0 );
}

/**
    这个是最重要的 Rtsp 取得图像的方法 
    
    传入图像的 Image的结构体指针，成功就返回 0 
*/
int RemoteCameraRtsp::Capture( Image &image )
{
    while ( true )
    {
        buffer.clear();
        if ( !rtspThread->isRunning() )
            break;
        //if ( rtspThread->stopped() )
            //break;
        if ( rtspThread->getFrame( buffer ) )
        {
            Debug( 3, "Read frame %d bytes", buffer.size() );
            Debug( 4, "Address %p", buffer.head() );
            Hexdump( 4, buffer.head(), 16 );

            static AVFrame *tmp_picture = NULL;

            if ( !tmp_picture )
            {
                //if ( c->pix_fmt != pf )
                //{
                    tmp_picture = avcodec_alloc_frame();
                    if ( !tmp_picture )
                    {
                        Fatal( "Could not allocate temporary opicture" );
                    }
                    int size = avpicture_get_size( PIX_FMT_RGB24, width, height);
                    uint8_t *tmp_picture_buf = (uint8_t *)malloc(size);
                    if (!tmp_picture_buf)
                    {
                        av_free( tmp_picture );
                        Fatal( "Could not allocate temporary opicture" );
                    }
                    avpicture_fill( (AVPicture *)tmp_picture, tmp_picture_buf, PIX_FMT_RGB24, width, height );
                //}
            }

            if ( !buffer.size() )
                return( -1 );

            int initialFrameCount = frameCount;
            while ( buffer.size() > 0 )
            {
                int got_picture = false;
                int len = avcodec_decode_video( codecContext, picture, &got_picture, buffer.head(), buffer.size() );
                if ( len < 0 )
                {
                    if ( frameCount > initialFrameCount )
                    {
                        // Decoded at least one frame
                        return( 0 );
                    }
                    Error( "Error while decoding frame %d", frameCount );
                    Hexdump( ZM_DBG_ERR, buffer.head(), buffer.size()>256?256:buffer.size() );
                    buffer.clear();
                    continue;
                    //return( -1 );
                }
                Debug( 2, "Frame: %d - %d/%d", frameCount, len, buffer.size() );
                //if ( buffer.size() < 400 )
                    //Hexdump( 0, buffer.head(), buffer.size() );

                if ( got_picture )
                {
                    /* the picture is allocated by the decoder. no need to free it */
                    Debug( 1, "Got picture %d", frameCount );

                    static struct SwsContext *img_convert_ctx = 0;

                    if ( !img_convert_ctx )
                    {
                        img_convert_ctx = sws_getContext( codecContext->width, codecContext->height, codecContext->pix_fmt, width, height, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL );
                        if ( !img_convert_ctx )
                            Fatal( "Unable to initialise image scaling context" );
                    }

                    sws_scale( img_convert_ctx, picture->data, picture->linesize, 0, height, tmp_picture->data, tmp_picture->linesize );

                    image.Assign( width, height, colours, tmp_picture->data[0] );

                    frameCount++;

                    return( 0 );
                }
                else
                {
                    Warning( "Unable to get picture from frame" );
                }
                buffer -= len;
            }
        }
    }
    return( -1 );
}

int RemoteCameraRtsp::PostCapture()
{
    return( 0 );
}
#endif // HAVE_LIBAVFORMAT

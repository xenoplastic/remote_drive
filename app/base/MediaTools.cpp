#include "MediaTools.h"
#include "BaseFunc.h"
#include "SoLogger.h"
#include "Defer.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavutil/error.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavdevice/avdevice.h"
}

#ifdef WIN32
#undef  av_err2str
char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
#define av_err2str(errnum) av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, errnum)
#endif

#ifdef linux
#include <string.h>
#define memcpy_s(dst, dstSize, src, srcSize) memcpy((dst), (src), (srcSize))
#endif

#define PrintError(message, devPath, outAddr, errCode)                  \
{                                                                       \
    auto errStr = err2Str(errCode);                                     \
    log_info << "FFmpegKits::PushVideo:" << message << ":ret="          \
    << errStr << ",devPath=" << devPath << ",outAddr=" << outAddr       \
    << std::endl;                                                       \
}

FFmpegKits::FFmpegKits() :
    m_mediaState(MediaState::Stopped),
    m_mediaSecSeekPos(0),
    m_mediaParseSyncSeek(false),
    m_mediaPlaySyncSeek(false)
{
    //av_register_all();
}

FFmpegKits::~FFmpegKits()
{
    Release();
}

int FFmpegKits::CreateDecoder(AVCodecID codecID)
{
    auto pAVCodec = avcodec_find_decoder(codecID);
    if (!pAVCodec)
        return -1;

    AVCodecContext* pAVCodecCtx = avcodec_alloc_context3(pAVCodec);
    if (!pAVCodecCtx)
        return -1;

    AVCodecParameters* codecParameters = avcodec_parameters_alloc();
    if (avcodec_parameters_from_context(codecParameters, pAVCodecCtx) < 0)
    {
        avcodec_parameters_free(&codecParameters);
        avcodec_free_context(&pAVCodecCtx);
        return -1;
    }

    m_AVCodecDecoder = avcodec_find_decoder(codecParameters->codec_id);
    if (!m_AVCodecDecoder)
        return -1;

    m_AVCodecCtxDecoder = avcodec_alloc_context3(m_AVCodecDecoder);
    if (!m_AVCodecCtxDecoder)
        return -1;

    if (avcodec_parameters_to_context(m_AVCodecCtxDecoder, codecParameters) < 0)
        return -1;

    if (avcodec_open2(m_AVCodecCtxDecoder, m_AVCodecDecoder, nullptr) < 0)
        return -1;

    m_AVPacketDecoder = av_packet_alloc();
    m_AVFrameDecoder = av_frame_alloc();
    m_frameYUVDecoder = av_frame_alloc();

    avcodec_parameters_free(&codecParameters);
    avcodec_free_context(&pAVCodecCtx);

    return 0;
}

void FFmpegKits::Release()
{
    StopMedia();

    sws_freeContext(m_mediaSwsContext);

    avcodec_free_context(&m_AVCodecCtxDecoder);
    av_freep(&m_AVFrameDecoder);
    av_frame_free(&m_frameYUVDecoder);
    av_packet_free(&m_AVPacketDecoder);
    avcodec_free_context(&m_AVCodecCtxEecoder);
    av_frame_free(&m_frameEncoder);
    av_packet_free(&m_pktEncoder);

    avformat_close_input(&m_mediaContext);
    av_freep(&m_mediaOutbuffer);
    av_packet_free(&m_mediaPacket);
    av_frame_free(&m_mediaFrame);
    av_frame_free(&m_mediaFrameYUV);

    ReleaseVideoCache();
}

int FFmpegKits::Decode(
    unsigned char* inbuf,
    int32_t inbufSize,
    int32_t* framePara,
    unsigned char* outRGBBuf,
    unsigned char** outYBuf,
    unsigned char** outUBuf,
    unsigned char** outVBuf,
    int32_t needKeyFrame,
    AVPictureType* ft
)
{
    // av_frame_unref(pAVFrame_decoder);
    // av_frame_unref(pFrameYUV_decoder);
    m_AVPacketDecoder->data = inbuf;
    m_AVPacketDecoder->size = inbufSize;
    int ret = avcodec_send_packet(m_AVCodecCtxDecoder, m_AVPacketDecoder);
    if (ret)
        return -1;

    ret = avcodec_receive_frame(m_AVCodecCtxDecoder, m_AVFrameDecoder);
    if (ret)
        return -1;

    if (ft)
        *ft = m_AVFrameDecoder->pict_type;
    if (needKeyFrame != 0 && m_AVFrameDecoder->key_frame != 1)
        return -1;

    if (framePara)
    {
        framePara[0] = m_AVFrameDecoder->width;
        framePara[1] = m_AVFrameDecoder->height;
    }

    if (outYBuf && outUBuf && outVBuf)
    {
        *outYBuf = m_AVFrameDecoder->data[0];
        *outUBuf = m_AVFrameDecoder->data[1];
        *outVBuf = m_AVFrameDecoder->data[2];
        if (framePara)
        {
            framePara[2] = m_AVFrameDecoder->linesize[0];
            framePara[3] = m_AVFrameDecoder->linesize[1];
            framePara[4] = m_AVFrameDecoder->linesize[2];
        }
    }

    if (outRGBBuf && m_AVFrameDecoder->data[0] && m_AVFrameDecoder->data[1] &&
        m_AVFrameDecoder->data[2])
    {
        m_frameYUVDecoder->data[0] = outRGBBuf;
        m_frameYUVDecoder->data[1] = NULL;
        m_frameYUVDecoder->data[2] = NULL;
        m_frameYUVDecoder->data[3] = NULL;
        int linesize[4] = { m_AVCodecCtxDecoder->width * 3,
            m_AVCodecCtxDecoder->height * 3, 0, 0 };
        struct SwsContext* pImageConvertCtx_decoder = sws_getContext(
            m_AVCodecCtxDecoder->width,
            m_AVCodecCtxDecoder->height,
            AV_PIX_FMT_YUV420P,
            m_AVCodecCtxDecoder->width,
            m_AVCodecCtxDecoder->height,
            AV_PIX_FMT_RGB24,
            SWS_SPLINE,
            NULL,
            NULL,
            NULL
        );
        if (pImageConvertCtx_decoder)
        {
            ret = sws_scale(
                pImageConvertCtx_decoder,
                (const uint8_t* const*)m_AVFrameDecoder->data,
                m_AVFrameDecoder->linesize,
                0,
                m_AVCodecCtxDecoder->height,
                m_frameYUVDecoder->data,
                linesize
            );
            sws_freeContext(pImageConvertCtx_decoder);
            if (m_AVCodecCtxDecoder->height != ret)
                return -1;
        }
        else
        {
            return -1;
        }
    }

    return 0;
}

int FFmpegKits::CreateEncoder(AVCodecID codecID)
{
    //avcodec_register_all();
    m_AVCodecEncoder = avcodec_find_encoder(codecID);
    if (!m_AVCodecEncoder)
        return -1;

    m_AVCodecCtxEecoder = avcodec_alloc_context3(m_AVCodecEncoder);
    if (!m_AVCodecCtxEecoder)
        return -1;

    m_frameEncoder = av_frame_alloc();
    if (!m_frameEncoder)
        return -1;

    m_pktEncoder = av_packet_alloc();
    if (!m_pktEncoder)
        return -1;

    return 0;
}

int FFmpegKits::Encode(
    uint8_t* ybuf,
    uint8_t* ubuf,
    uint8_t* vbuf,
    int32_t width,
    int32_t height,
    unsigned char** outBuf,
    int32_t* outBufSize,
    int32_t fps,
    int32_t bitRate
)
{
    /* put sample parameters */
    m_AVCodecCtxEecoder->bit_rate = bitRate;
    /* resolution must be a multiple of two */
    m_AVCodecCtxEecoder->width = width;
    m_AVCodecCtxEecoder->height = height;
    /* frames per second */
    m_AVCodecCtxEecoder->time_base = AVRational{ 1, fps };
    m_AVCodecCtxEecoder->framerate = AVRational{ fps, 1 };
    m_AVCodecCtxEecoder->codec_id = m_AVCodecEncoder->id;
    m_AVCodecCtxEecoder->codec_type = AVMEDIA_TYPE_VIDEO;
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
     // c->qmin = 10;
     // c->qmax = 51;
     // c->gop_size = 0;
     // c->max_b_frames = 0;
    m_AVCodecCtxEecoder->pix_fmt = AV_PIX_FMT_YUV420P;

    AVDictionary* param = nullptr;
    int ret = 0;
    if (m_AVCodecCtxEecoder->codec_id == AV_CODEC_ID_H264)
    {
        av_dict_set(&param, "preset", "slow", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
    }

    ret = avcodec_open2(m_AVCodecCtxEecoder, m_AVCodecEncoder, &param);
    av_dict_free(&param);
    if (ret < 0)
        return -1;

    m_frameEncoder->format = m_AVCodecCtxEecoder->pix_fmt;
    m_frameEncoder->width = m_AVCodecCtxEecoder->width;
    m_frameEncoder->height = m_AVCodecCtxEecoder->height;

    ret = av_frame_get_buffer(m_frameEncoder, 32);
    if (ret < 0)
        return -1;

    ret = av_frame_make_writable(m_frameEncoder);
    if (ret != 0)
        return -1;

    m_frameEncoder->data[0] = ybuf;
    m_frameEncoder->data[1] = ubuf;
    m_frameEncoder->data[2] = vbuf;
    ret = avcodec_send_frame(m_AVCodecCtxEecoder, m_frameEncoder);
    if (ret < 0)
        return -1;

    ret = avcodec_receive_packet(m_AVCodecCtxEecoder, m_pktEncoder);
    if (ret != 0 && ret != AVERROR_EOF)
        return -1;

    if (!ret)
    {
        *outBuf = m_pktEncoder->data;
        *outBufSize = m_pktEncoder->size;
        av_packet_unref(m_pktEncoder);
    }

    return 0;
}

void FFmpegKits::StartMediaThread()
{
    m_mediaState = MediaState::Playing;
    m_mediaDecoderThread = std::thread(&FFmpegKits::DecodeLoop, this);
    m_mediaPlayThread = std::thread(&FFmpegKits::PlayLoop, this);
}

int FFmpegKits::InitMediaEngine()
{
    if (m_initMediaEngine)
        return 0;

    m_mediaFrameYUV = av_frame_alloc();
    m_mediaPacket = av_packet_alloc();
    m_mediaFrame = av_frame_alloc();
    if (!m_mediaPacket || !m_mediaFrame || !m_mediaFrameYUV)
        return -1;
    m_initMediaEngine = true;
    return 0;
}

void FFmpegKits::StopMedia()
{
    m_mediaState = MediaState::Stopped;
    if (m_mediaOpenThread.joinable())
        m_mediaOpenThread.join();
    m_mediaCondVarState.notify_all();
    m_mediaConVarDecodeEOF.notify_all();
    m_mediaConVarCanWrite.notify_all();
    m_mediaConVarCanRead.notify_all();
    if (m_mediaDecoderThread.joinable())
        m_mediaDecoderThread.join();
    if (m_mediaPlayThread.joinable())
        m_mediaPlayThread.join();
}

void FFmpegKits::PauseMedia()
{
    if (m_mediaState == MediaState::Playing)
        m_mediaState = MediaState::Paused;
}

void FFmpegKits::PlayMedia()
{
    if (m_mediaState == MediaState::Paused)
    {
        m_mediaState = MediaState::Playing;
        m_mediaCondVarState.notify_all();
    }
    else if (m_mediaState == MediaState::Stopped)
    {
        Reset();
        SeekMedia(0);
        //StartMediaThread();
        StartParseMedia(m_mediaPath);
        //if (m_mediaSecDuration == -1)
        //	StartParseMedia(m_mediaPath);
        //else
        //	StartMediaThread();
    }
}

void FFmpegKits::SeekMedia(unsigned secTimestamp)
{
    if (secTimestamp > m_mediaSecDuration)
        return;
    m_mediaSecSeekPos = secTimestamp;
    m_mediaParseSyncSeek = true;
    m_mediaPlaySyncSeek = true;
    m_mediaConVarDecodeEOF.notify_all();
}

MediaState FFmpegKits::GetMediaState()
{
    return m_mediaState;
}

unsigned FFmpegKits::GetSeekPos()
{
    return m_mediaSecSeekPos;
}

int64_t FFmpegKits::GetDuration()
{
    return m_mediaSecDuration;
}

void FFmpegKits::StartMedia(
    const std::string& inputMediaFile,
    MEDIA_PARSE_CALLBACK videoCallback,
    int openTimeout/* = 10000*/,
    int readTimeout/* = 5000*/,
    const std::string& protocol/* = "tcp"*/,
    VideoData* videoData/* = nullptr*/
)
{
    if (InitMediaEngine() < 0)
    {
        videoCallback(VideoData(), MediaMessage::InitFail);
        return;
    }

    Reset();

    m_mediaPath = inputMediaFile;
    m_mediaParseCallback = videoCallback;
    m_mediaMillMediaTimeout = openTimeout;
    m_mediaMillMediaOpenTimeout = openTimeout;
    m_mediaMillMediaReadTimeout = readTimeout;
    m_rtspProtocol = protocol;
    //查看是否为url
    if (inputMediaFile.find('/') != -1)
    {
        //url则异步打开
        if (videoData)
        {
            VideoData tmp = *videoData;
            m_mediaOpenThread = std::thread(
                [=] {
                    VideoData data = tmp;
                    StartParseMedia(
                        inputMediaFile,
                        protocol,
                        openTimeout,
                        readTimeout,
                        &data
                    );
                }
            );
        }
        else
        {
			m_mediaOpenThread = std::thread(
				[=] {
					StartParseMedia(
						inputMediaFile,
						protocol,
						openTimeout,
						readTimeout
					);
				}
			);
        }
    }
    else
    {
        //本地文件直接打开并播放
        StartParseMedia(inputMediaFile);
    }
}

void FFmpegKits::RestartMedia()
{
    StartMedia(
        m_mediaPath,
        m_mediaParseCallback,
        m_mediaMillMediaOpenTimeout,
        m_mediaMillMediaReadTimeout,
        m_rtspProtocol
    );
}

void FFmpegKits::DecodeLoop()
{
    const int millSleepTimePerframe = 1000 / m_mediaFPS;
    VideoData* videoData = nullptr;
    const unsigned yBufSize = m_mediaCodecCtx->width * m_mediaCodecCtx->height;
    const unsigned uBufSize = yBufSize / 4;
    const unsigned vBufSize = uBufSize;
    unsigned seekPos = 0;
    int ret;
    while (m_mediaState != MediaState::Stopped)
    {
        if (DetechPause())
        {
            if (m_mediaState == MediaState::Stopped)
                break;
        }

        //检测是否指定位置解析
        if (m_mediaParseSyncSeek)
        {
            //清空帧缓存
            ResetVideoCache();

            m_mediaParseSyncSeek = false;
            seekPos = m_mediaSecSeekPos;

            bool success = true;
            for (unsigned i = 0; i < m_mediaContext->nb_streams; i++)
            {
                auto videoStream = m_mediaContext->streams[m_mediaVideoStreamIndex];
                int64_t seekPts = (int64_t)(seekPos / av_q2d(videoStream->time_base));
                m_mediaMillCallTime = GetMillTimestampSinceEpoch();
                ret = av_seek_frame(
                    m_mediaContext,
                    m_mediaVideoStreamIndex,
                    seekPts,
                    AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME
                );
                if (ret < 0)
                {
                    success = false;
                    break;
                }
            }

            avcodec_flush_buffers(m_mediaCodecCtx);

            if (!success)
            {
                m_mediaParseCallback({}, MediaMessage::SeekFail);
                m_mediaState = MediaState::Stopped;
                break;
            }
        }

        if (!videoData)
        {
            m_mediaMutexWVideoData.lock();
            if (!m_mediaWriteVideoData.empty())
            {
                videoData = m_mediaWriteVideoData.front();
                m_mediaWriteVideoData.pop_front();
            }
            m_mediaMutexWVideoData.unlock();

            if (!videoData)
            {
                std::mutex mutex;
                std::unique_lock<std::mutex> lk(mutex);
                m_mediaConVarCanWrite.wait_for(lk, std::chrono::seconds(1));
                continue;
            }
        }

        m_mediaMillCallTime = GetMillTimestampSinceEpoch();
        auto errCode = av_read_frame(m_mediaContext, m_mediaPacket);
        if (errCode != 0)
        {
            if (errCode != AVERROR_EOF || m_mediaSecDuration == -1)
            {
                m_mediaParseCallback({}, MediaMessage::ReadFail);
                m_mediaState = MediaState::Stopped;
                break;
            }
            else
            {
                //睡眠等到拖动位置并播放,或者等待退出指令
                std::mutex mutex;
                std::unique_lock<std::mutex> lk(mutex);
                m_mediaConVarDecodeEOF.wait_for(lk, std::chrono::seconds(1));
                continue;
            }
        }

        if (m_mediaPacket->stream_index != m_mediaVideoStreamIndex)
        {
            av_packet_unref(m_mediaPacket);
            continue;
        }

        ret = avcodec_send_packet(m_mediaCodecCtx, m_mediaPacket);
        if (ret)
        {
            av_packet_unref(m_mediaPacket);
            continue;
        }

        std::string strSeiData;
        if ((m_mediaPacket->data[4] & 0x1F) == 6)
        {
            int jsonPos;
            unsigned seiSize = ParseSeiSize(m_mediaPacket->data, jsonPos);
            if (seiSize != -1)
            {
                char* seiData = new char[seiSize] { 0 };
                memcpy_s(seiData, seiSize, m_mediaPacket->data + jsonPos, seiSize);
                seiData[seiSize - 1] = 0;
                strSeiData = seiData;
                delete[] seiData;
            }
        }

        av_packet_unref(m_mediaPacket);

        ret = avcodec_receive_frame(m_mediaCodecCtx, m_mediaFrame);
        if (ret)
        {
            //std::string errStr = av_err2str(ret);
            continue;
        }

        auto curMediTimestamp = m_mediaFrame->pts * av_q2d(
            m_mediaContext->streams[m_mediaVideoStreamIndex]->time_base);
        if (m_mediaSecDuration != -1 && curMediTimestamp < seekPos)
        {
            continue;
        }

        AVFrame** frame = nullptr;
        if (m_mediaFrame->width != m_mediaFrame->linesize[0])
        {
            //解决yuv显示花屏(花屏原因:ffmpeg直接解码出来的yuv宽会按照CPU位数对齐)
            int ret = sws_scale(m_mediaSwsContext, (uint8_t const* const*)m_mediaFrame->data,
                m_mediaFrame->linesize, 0, m_mediaFrame->height,
                m_mediaFrameYUV->data, m_mediaFrameYUV->linesize);
            if (ret == m_mediaFrame->height)
                frame = &m_mediaFrameYUV;
        }
        else
        {
            frame = &m_mediaFrame;
        }

        if (frame)
        {
            //将pFrame中数据复制到videoData中，并插入m_mediaReadVideoData，
            memcpy_s(videoData->yBuf, yBufSize, (*frame)->data[0], yBufSize);
            memcpy_s(videoData->uBuf, uBufSize, (*frame)->data[1], uBufSize);
            memcpy_s(videoData->vBuf, vBufSize, (*frame)->data[2], vBufSize);

            videoData->seiData = strSeiData;
            videoData->secTimestamp = curMediTimestamp;

            m_mediaMutexRVideoData.lock();
            m_mediaReadVideoData.push_back(videoData);
            m_mediaMutexRVideoData.unlock();
            m_mediaConVarCanRead.notify_all();

            videoData = nullptr;
        }
    }

    if (videoData)
    {
        m_mediaMutexRVideoData.lock();
        m_mediaReadVideoData.push_back(videoData);
        m_mediaMutexRVideoData.unlock();
    }
}

void FFmpegKits::PlayLoop()
{
    uint64_t millPlayTimePos = 0;
    uint64_t millPauseTotalDuration = 0;
    uint64_t millStartPlayTime = -1;
    int64_t millSleepTime = 0;
    double secLastTimestamp = 0;
    uint64_t secSeekPos = 0;
    uint64_t millPauseDuration;
    const uint64_t millSleepTimePerframe = 1000 / m_mediaFPS;
    auto resetSeekData = [&] {
        millStartPlayTime = GetMillTimestampSinceEpoch();
        secSeekPos = m_mediaSecSeekPos;
        secLastTimestamp = (double)secSeekPos;
        m_mediaPlaySyncSeek = false;
        millPauseTotalDuration = 0;
        ResetVideoCache();
    };
    while (m_mediaState != MediaState::Stopped)
    {
        millPauseDuration = DetechPause();
        if (millPauseDuration)
        {
            millPauseTotalDuration += millPauseDuration;
            if (m_mediaState == MediaState::Stopped)
                break;
        }

        VideoData* videoData = nullptr;
        m_mediaMutexRVideoData.lock();
        if (!m_mediaReadVideoData.empty())
        {
            videoData = m_mediaReadVideoData.front();
            m_mediaReadVideoData.pop_front();
        }
        m_mediaMutexRVideoData.unlock();

        if (videoData)
        {
            if (millStartPlayTime == -1)
            {
                millStartPlayTime = GetMillTimestampSinceEpoch();
            }
            else if(m_mediaSecDuration != -1)
            {
                millPlayTimePos = GetMillTimestampSinceEpoch() - millStartPlayTime + secSeekPos * 1000 -
                    millPauseTotalDuration;
                //如果当前视频点早于videoData->secTimestamp则睡眠
                millSleepTime = (int64_t)(videoData->secTimestamp * 1000) - millPlayTimePos;
                while (millSleepTime > 0)
                {
                    //如已经拖动播放位置,从头开始执行
                    if (m_mediaPlaySyncSeek)
                        break;

                    if (millSleepTime > 1000)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                        //发送临时数据用于更新UI时间
                        VideoData transitVideodata{ 0 };
                        transitVideodata.secTimestamp = ++secLastTimestamp;
                        m_mediaParseCallback(transitVideodata, MediaMessage::NoError);
                        millSleepTime -= 1000;
                    }
                    else
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(millSleepTime));
                        break;
                    }

                    millPauseDuration = DetechPause();
                    if (millPauseDuration)
                        millPauseTotalDuration += millPauseDuration;

                    if (m_mediaState == MediaState::Stopped)
                        break;
                }
            }

            if (m_mediaPlaySyncSeek)
            {
                resetSeekData();
            }
            else
            {
                m_mediaParseCallback(*videoData, MediaMessage::NoError);
                secLastTimestamp = videoData->secTimestamp;
            }

            m_mediaMutexWVideoData.lock();
            m_mediaWriteVideoData.push_back(videoData);
            m_mediaMutexWVideoData.unlock();

            m_mediaConVarCanWrite.notify_all();
        }
        else
        {
            //如果已经拖动播放位置,从头开始执行
            if (m_mediaPlaySyncSeek)
            {
                resetSeekData();
                continue;
            }

            if (m_mediaSecDuration == -1)
            {
                std::mutex mutex;
                std::unique_lock<std::mutex> lk(mutex);
                m_mediaConVarCanRead.wait_for(lk, std::chrono::seconds(1));
                continue;
            }

            //按帧率睡眠一帧间隔
            std::this_thread::sleep_for(std::chrono::milliseconds(millSleepTimePerframe));
            secLastTimestamp += (double)millSleepTimePerframe / 1000;
            if (secLastTimestamp >= m_mediaSecDuration)
            {
                m_mediaParseCallback({}, MediaMessage::Eof);
                m_mediaState = MediaState::Stopped;
                break;
            }

            VideoData videoData{ 0 };
            videoData.secTimestamp = secLastTimestamp;
            m_mediaParseCallback(videoData, MediaMessage::NoError);
        }
    }
}

int FFmpegKits::OpenMedia(
    const std::string& inputMediaFile,
    const std::string& protocol/* = "udp"*/,
    unsigned timeout/* = 10000*/,
    VideoData* urlVideoData/* = nullptr*/
)
{
    m_mediaState = MediaState::Opening;
    m_mediaMillMediaTimeout = timeout;

    if (InitMediaEngine() < 0)
        return -1;

    //std::this_thread::sleep_for(std::chrono::seconds(1));
    m_mediaContext = avformat_alloc_context();
    if (!m_mediaContext)
        return -1;

    //m_mediaPacket = av_packet_alloc();
    //m_mediaFrame = av_frame_alloc();
    //if (!m_mediaPacket || !m_mediaFrame)
    //	return -1;

    // 设置参数
    AVDictionary* options = nullptr;
    av_dict_set(&options, "rtsp_transport", protocol.c_str(), 0);
	//// 设置TCP连接最大延时时间
	av_dict_set(&options, "max_delay", "1000000", 0);
	/// 设置“buffer_size”缓存容量
	av_dict_set(&options, "buffer_size", "1048576", 0);
    ////// 设置avformat_open_input超时时间为3秒
    auto strTimeout = std::to_string(m_mediaMillMediaTimeout * 1000);
#ifdef WIN32
    av_dict_set(&options, "stimeout", strTimeout.c_str(), 0);
#else
    av_dict_set(&options, "timeout", strTimeout.c_str(), 0);
#endif

    m_mediaContext->interrupt_callback.callback = [](void* data) ->int {
        auto pThis = static_cast<FFmpegKits*>(data);
        if (pThis->m_mediaState == MediaState::Stopped)
            return 1;

        auto escapeTime = GetMillTimestampSinceEpoch()- pThis->m_mediaMillCallTime;
        return escapeTime > pThis->m_mediaMillMediaTimeout ? 1 : 0;
    };
    m_mediaContext->interrupt_callback.opaque = this;

    m_mediaMillCallTime = GetMillTimestampSinceEpoch();
    //if(urlVideoData)
    //	m_mediaContext->flags |= AVFMT_FLAG_NOBUFFER;
    auto ret = avformat_open_input(&m_mediaContext, inputMediaFile.c_str(),
        nullptr, &options);
    av_dict_free(&options);
    if (ret < 0)
    {
        //std::string str = av_err2str(ret);
        return -1;
    }

    if (!urlVideoData)
    {
        m_mediaMillCallTime = GetMillTimestampSinceEpoch();
        if (avformat_find_stream_info(m_mediaContext, nullptr) < 0)
            return -1;
    }

    m_mediaVideoStreamIndex = av_find_best_stream(
        m_mediaContext,
        AVMEDIA_TYPE_VIDEO,
        -1, -1, NULL, 0
    );
    if (m_mediaVideoStreamIndex < 0)
        return -1;

    auto videoStream = m_mediaContext->streams[m_mediaVideoStreamIndex];
    if (!urlVideoData)
    {
        if (videoStream->avg_frame_rate.num > videoStream->avg_frame_rate.den)
            m_mediaFPS = videoStream->avg_frame_rate.num / videoStream->avg_frame_rate.den;
        else
            m_mediaFPS = 1;
    }
    else
    {
        m_mediaFPS = urlVideoData->frameRate;
    }

    if (m_mediaContext->duration > 0)
        m_mediaSecDuration = m_mediaContext->duration / AV_TIME_BASE;
    else
        m_mediaSecDuration = -1;

//	m_mediaCodecCtx = m_mediaContext->streams[m_mediaVideoStreamIndex]->codec;

//	AVCodec* pCodex = avcodec_find_decoder(m_mediaCodecCtx->codec_id);
//	if (!pCodex)
//		return -1;

    AVCodecParameters* origin_par = videoStream->codecpar;
    auto pCodex = avcodec_find_decoder(origin_par->codec_id);
    if (!pCodex)
        return -1;
    m_mediaCodecCtx = avcodec_alloc_context3(pCodex);
    //if (urlVideoData)
    //	m_mediaCodecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    if (avcodec_parameters_to_context(m_mediaCodecCtx, origin_par))
        return -1;

    if (avcodec_open2(m_mediaCodecCtx, pCodex, nullptr) < 0)
        return -1;

    if (!urlVideoData)
    {
        if (!m_mediaCodecCtx->width || !m_mediaCodecCtx->height)
            return -1;
    }
    else
    {
        m_mediaCodecCtx->width = urlVideoData->width;
        m_mediaCodecCtx->height = urlVideoData->height;
        m_mediaCodecCtx->pix_fmt = urlVideoData->pixFmt;
    }
    m_mediaSwsContext = sws_getContext(
        m_mediaCodecCtx->width,
        m_mediaCodecCtx->height,
        m_mediaCodecCtx->pix_fmt,
        m_mediaCodecCtx->width,
        m_mediaCodecCtx->height,
        AV_PIX_FMT_YUV420P,
        SWS_BICUBIC,
        nullptr,
        nullptr,
        nullptr
    );
    if (!m_mediaSwsContext)
        return -1;

    int bufSize = av_image_get_buffer_size(
        AV_PIX_FMT_YUV420P,
        m_mediaCodecCtx->width,
        m_mediaCodecCtx->height,
        1
    );
    m_mediaOutbuffer = (uint8_t*)av_malloc(bufSize);
    if (av_image_fill_arrays(
        m_mediaFrameYUV->data,
        m_mediaFrameYUV->linesize,
        m_mediaOutbuffer,
        AV_PIX_FMT_YUV420P,
        m_mediaCodecCtx->width,
        m_mediaCodecCtx->height,
        1
    ) < 0)
        return -1;

    ReleaseVideoCache();

    unsigned yBufSize = m_mediaCodecCtx->width * m_mediaCodecCtx->height;
    unsigned uBufSize = yBufSize / 4;
    unsigned vBufSize = uBufSize;
    for (int i = 0; i < m_mediaFPS; i++)
    {
        auto videoData = new VideoData{ 0 };
        videoData->yBuf = new uint8_t[yBufSize]{ 0 };
        videoData->uBuf = new uint8_t[uBufSize]{ 0 };
        videoData->vBuf = new uint8_t[vBufSize]{ 0 };
        videoData->width = m_mediaCodecCtx->width;
        videoData->height = m_mediaCodecCtx->height;
        videoData->frameRate = m_mediaFPS;
        m_mediaWriteVideoData.push_back(videoData);
    }

    return 0;
}

std::string FFmpegKits::GetMediaPath() const
{
    return m_mediaPath;
}

std::string FFmpegKits::GetOutAddr() const
{
    return m_outAddr;
}

bool FFmpegKits::IsValid(
	const std::string& inputMediaFile,
	const std::string& protocol/* = "tcp"*/,
	unsigned timeout/* = 5000*/,
	unsigned testCount/* = 5*/,
	bool testRead/* = false*/
)
{
    m_mediaState = MediaState::Opening;
    m_mediaMillMediaTimeout = timeout;
    if (InitMediaEngine() < 0)
        return false;

    //std::this_thread::sleep_for(std::chrono::seconds(1));
    m_mediaContext = avformat_alloc_context();
    if (!m_mediaContext)
        return false;

    //m_mediaPacket = av_packet_alloc();
    //m_mediaFrame = av_frame_alloc();
    //if (!m_mediaPacket || !m_mediaFrame)
    //	return -1;

    // 设置参数
    AVDictionary* options = nullptr;
    av_dict_set(&options, "rtsp_transport", protocol.c_str(), 0);
    //// 设置TCP连接最大延时时间
    //av_dict_set(&options, "max_delay", "500", 0);
    /// 设置“buffer_size”缓存容量
    //av_dict_set(&options, "buffer_size", "1024", 0);
    ////// 设置avformat_open_input超时时间为3秒
    auto strTimeout = std::to_string(m_mediaMillMediaTimeout * 1000);
#ifdef WIN32
    av_dict_set(&options, "stimeout", strTimeout.c_str(), 0);
#else
    av_dict_set(&options, "timeout", strTimeout.c_str(), 0);
#endif

    m_mediaContext->interrupt_callback.callback = [](void* data) ->int {
        auto pThis = static_cast<FFmpegKits*>(data);
        if (pThis->m_mediaState == MediaState::Stopped)
            return 1;

        auto escapeTime = GetMillTimestampSinceEpoch()- pThis->m_mediaMillCallTime;
        return escapeTime > pThis->m_mediaMillMediaTimeout ? 1 : 0;
    };
    m_mediaContext->interrupt_callback.opaque = this;

    m_mediaMillCallTime = GetMillTimestampSinceEpoch();
    auto ret = avformat_open_input(&m_mediaContext, inputMediaFile.c_str(),
        nullptr, &options);
    av_dict_free(&options);
    if (ret < 0)
    {
        //std::string str = av_err2str(ret);
        return false;
    }

    if(testRead)
    {
        m_mediaMillCallTime = GetMillTimestampSinceEpoch();
        for(unsigned i = 0; i <= testCount; ++i)
        {
            auto errCode = av_read_frame(m_mediaContext, m_mediaPacket);
            av_packet_unref(m_mediaPacket);
            if(errCode != 0)
                return false;
        }
    }

    return true;
}

void FFmpegKits::StartParseMedia(
    const std::string& inputMediaFile,
    const std::string& protocol/* = "tcp"*/,
    int openTimeout/* = 10000*/,
    int readTimeout/* = 5000*/,
    VideoData* videoData/* = nullptr*/
)
{
    int res = OpenMedia(inputMediaFile, protocol, openTimeout, videoData);
    if (res)
    {
        m_mediaParseCallback(VideoData(), MediaMessage::OpenFail);
    }
    else
    {
        //可能打开过程中被终止
        if (m_mediaState != MediaState::Opening)
            return;
        m_mediaMillMediaTimeout = readTimeout;
        StartMediaThread();
        m_mediaParseCallback(VideoData(), MediaMessage::OpenSuccess);
    }
}

int FFmpegKits::ParseSeiSize(const uint8_t* data, int& seiDataPos) const
{
    //从头部解析数据体大小
    int bodySize = 0;
    uint8_t* bodySizePtr = (uint8_t*)&bodySize;
    for (int i = 0; i < 4; i++)
    {
        bodySizePtr[i] = data[3 - i];
    }

    if (bodySize <= 0)
        return -1;

    //解析sei数据大小
    int seiBodySize = bodySize - 3;
    int discreetSeiDataSize = 0;
    int tmp = 0;
    int seiHeadPos = 6;
    for (int i = 0;; i++)
    {
        tmp += data[seiHeadPos++];
        discreetSeiDataSize = seiBodySize - i - 1;
        if (discreetSeiDataSize <= 0)
            return -1;

        if (tmp == discreetSeiDataSize)
        {
            seiDataPos = seiHeadPos;
            return tmp;
        }
        else if (tmp > discreetSeiDataSize)
        {
            return -1;
        }
    }
}

void FFmpegKits::ResetVideoCache()
{
    std::lock_guard<std::mutex> rlock(m_mediaMutexRVideoData);
    std::lock_guard<std::mutex> wlock(m_mediaMutexWVideoData);
    size_t size = m_mediaReadVideoData.size();
    for (size_t i = 0; i < size; i++)
    {
        auto videoData = m_mediaReadVideoData.front();
        videoData->seiData.clear();
        videoData->secTimestamp = 0;
        m_mediaWriteVideoData.push_back(videoData);
        m_mediaReadVideoData.pop_front();
        m_mediaConVarCanWrite.notify_all();
    }
}

uint64_t FFmpegKits::DetechPause()
{
    if (m_mediaState == MediaState::Paused)
    {
        uint64_t millPauseTimePos = GetMillTimestampSinceEpoch();
        std::mutex mutex;
        std::unique_lock<std::mutex> lk(mutex);
        while (m_mediaState == MediaState::Paused)
        {
            m_mediaCondVarState.wait_for(lk, std::chrono::seconds(1), [=]
                {
                    return m_mediaState != MediaState::Paused;
                }
            );
        }
        return GetMillTimestampSinceEpoch() - millPauseTimePos;
    }
    return 0;
}

void FFmpegKits::ReleaseVideoCache()
{
    for (auto& it : m_mediaWriteVideoData)
    {
        delete[] it->yBuf;
        delete[] it->uBuf;
        delete[] it->vBuf;
        delete it;
    }
    m_mediaWriteVideoData.clear();

    for (auto& it : m_mediaReadVideoData)
    {
        delete[] it->yBuf;
        delete[] it->uBuf;
        delete[] it->vBuf;
        delete it;
    }
    m_mediaReadVideoData.clear();
}

void FFmpegKits::Reset()
{
    StopMedia();
    av_freep(&m_mediaOutbuffer);
    ReleaseVideoCache();
    avformat_close_input(&m_mediaContext);
}

std::string FFmpegKits::err2Str(int errorCode)
{
    char errStr[255]{0};
    av_strerror(errorCode, errStr, 255);
    return errStr;
}

void FFmpegKits::PushVideo(
	const std::string& devPath,
	const std::string& outAddr,
	const VideoPushParam& videoPushParam,
	MEDIA_PUSH_CALLBACK callback,
	int millTimeout /*= 10000*/
)
{
    TransProtocol transProtocol;
    //set video format according to protocol
    if(outAddr.substr(0, 4) == "rtsp")
    {
        transProtocol = TransProtocol::rtsp;
    }
    else if(outAddr.substr(0, 3) == "srt")
    {
        transProtocol = TransProtocol::srt;
    }
    else
    {
        return;
    }

    m_mediaState = MediaState::Opening;
    m_mediaPath = devPath;
    m_mediaMillMediaTimeout = millTimeout;
    m_outAddr = outAddr;
    auto pushVideoFun = [=]() mutable
    {
        avdevice_register_all();

        int ret;
        int64_t startPushTime = GetMillTimestampSinceEpoch();
        AVFormatContext *outfmtCtx = nullptr;
        AVFormatContext *infmt_ctx = nullptr;
        AVFrame *frameYUV = nullptr;
        AVFrame *rawFrame = nullptr;
        AVPacket *decPkt = nullptr;
        AVPacket *encPkt = nullptr;
        AVCodecContext *encodecCtx = nullptr;
        AVCodecContext *decodecCtx = nullptr;
        AVDictionary *inFmtOpt = nullptr;
        int videoindex = -1;
        AVCodecParameters *originPar = nullptr;
        const AVCodec *decodec = nullptr;
        const AVCodec *encodec = nullptr;
        struct SwsContext *imgConvertCtx = nullptr;
        AVDictionary *encodecOpts = nullptr;
        AVStream *outStream = nullptr;
        int vpts = 0;
        int64_t escapteTime = 0;
        auto strTimeout = std::to_string(m_mediaMillMediaTimeout * 1000);
        bool haveSendNoError = false;
        int64_t readCameraTime = 0;
        int64_t previousReadPacketTime = 0;

        Defer defer([&]{
            m_mediaState = MediaState::Stopped;

            av_frame_free(&rawFrame);
            av_frame_free(&frameYUV);
            av_packet_free(&decPkt);
            av_packet_free(&encPkt);
            if(outfmtCtx)
                avio_closep(&outfmtCtx->pb);
            avcodec_free_context(&encodecCtx);
            avcodec_free_context(&decodecCtx);
            avformat_close_input(&infmt_ctx);
            avformat_close_input(&outfmtCtx);
        });

#ifdef WIN32
        av_dict_set(&inFmtOpt, "stimeout", strTimeout.c_str(), 0);
#else
        av_dict_set(&inFmtOpt, "timeout", strTimeout.c_str(), 0);
#endif

        auto interCallback = [](void *data) -> int
        {
            auto pThis = static_cast<FFmpegKits *>(data);
            if (pThis->m_mediaState == MediaState::Stopped)
                return 1;

            auto escapeTime = GetMillTimestampSinceEpoch() - pThis->m_mediaMillCallTime;
            if (escapeTime > pThis->m_mediaMillMediaTimeout)
            {
                pThis->m_mediaState = MediaState::Stopped;
                log_info << "FFmpegKits::PushVideo:interCallback:escapeTime > pThis->m_mediaMillMediaTimeout" 
                    << ",m_mediaPath=" << pThis->m_mediaPath << std::endl;
                return 1;
            }
            else
            {
                return 0;
            }
        };

        infmt_ctx = avformat_alloc_context();
        //infmt_ctx->flags |= AVFMT_FLAG_NOBUFFER;
        infmt_ctx->interrupt_callback.callback = interCallback;
        infmt_ctx->interrupt_callback.opaque = this;
        infmt_ctx->flags |= AVFMT_FLAG_NONBLOCK;

        const AVInputFormat *ifmt = nullptr;
        if(videoPushParam.mediaType == MediaType::Camera)
        {
#ifdef WIN32
        ifmt = av_find_input_format("dshow");
#elif linux
        ifmt = av_find_input_format("video4linux2");
#endif
        }

        m_mediaMillCallTime = GetMillTimestampSinceEpoch();
        ret = avformat_open_input(&infmt_ctx, devPath.c_str(), (AVInputFormat *)ifmt, &inFmtOpt);
        av_dict_free(&inFmtOpt);
        if (ret < 0)
        {
            PrintError("avformat_open_input", devPath, outAddr, ret);
            callback(MediaMessage::OpenFail);
            return;
        }

        m_mediaMillCallTime = GetMillTimestampSinceEpoch();
        ret = avformat_find_stream_info(infmt_ctx, NULL);
        if (ret < 0)
        {
            PrintError("avformat_find_stream_info", devPath, outAddr, ret);
            callback(MediaMessage::OpenFail);
            return;
        }

        videoindex = av_find_best_stream(
                    infmt_ctx,
                    AVMEDIA_TYPE_VIDEO,
                    -1, -1, nullptr, 0);
        if (videoindex < 0)
        {
            log_info << "FFmpegKits::PushVideo:av_find_best_stream:videoindex=" << videoindex 
                << ",devPath=" << devPath << std::endl;
            callback(MediaMessage::OpenFail);
            return;
        }

        originPar = infmt_ctx->streams[videoindex]->codecpar;
        decodec = avcodec_find_decoder(originPar->codec_id);
        if (!decodec)
        {
            PrintError("avcodec_find_decoder", devPath, outAddr, ret);
            callback(MediaMessage::OpenFail);
            return;
        }

        decodecCtx = avcodec_alloc_context3(decodec);
        // decodec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
        ret = avcodec_parameters_to_context(decodecCtx, originPar);
        if (ret < 0)
        {
            PrintError("avcodec_parameters_to_context", devPath, outAddr, ret);
            callback(MediaMessage::OpenFail);
            return;
        }

        ret = avcodec_open2(decodecCtx, decodec, nullptr);
        if (ret < 0)
        {
            PrintError("avcodec_open2", devPath, outAddr, ret);
            callback(MediaMessage::OpenFail);
            return;
        }

        encodec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!encodec)
        {
            log_info << "FFmpegKits::PushVideo:avcodec_find_encoder:encodec=NULL" << ",devPath=" 
                << devPath << std::endl;
            callback(MediaMessage::OpenFail);
            return;
        }

        encodecCtx = avcodec_alloc_context3(encodec);
        if (!encodecCtx)
        {
            log_info<< "FFmpegKits::PushVideo:avcodec_alloc_context3:encodec_ctx=NULL" << ",devPath=" 
                << devPath << std::endl;
            callback(MediaMessage::OpenFail);
            return;
        }
        encodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER | AV_CODEC_FLAG_LOW_DELAY;
        encodecCtx->codec_id = encodec->id;
        encodecCtx->bit_rate = videoPushParam.bitRate;
        encodecCtx->width = videoPushParam.width;
        encodecCtx->height = videoPushParam.height;
        encodecCtx->time_base = AVRational{1, videoPushParam.frameRate};
        encodecCtx->framerate = AVRational{videoPushParam.frameRate, 1};
        encodecCtx->gop_size = videoPushParam.frameGroup;
        encodecCtx->keyint_min = videoPushParam.keyIntMin;
        encodecCtx->max_b_frames = 0;
        encodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
        av_opt_set(encodecCtx->priv_data, "preset", videoPushParam.preset.c_str(), 0);
        av_opt_set(encodecCtx->priv_data, "tune", "zerolatency", 0);
        av_dict_set(&encodecOpts, "profile", "baseline", 0);
        // av_opt_set(encodec_ctx->priv_data, "crf", "18", 0);
        ret = avcodec_open2(encodecCtx, encodec, &encodecOpts);
        av_dict_free(&encodecOpts);
        if (ret < 0)
        {
            PrintError("avcodec_open2", devPath, outAddr, ret);
            callback(MediaMessage::OpenFail);
            return;
        }

        imgConvertCtx = sws_getCachedContext(
            imgConvertCtx, 
            decodecCtx->width, 
            decodecCtx->height, 
            decodecCtx->pix_fmt,
            encodecCtx->width, 
            encodecCtx->height, 
            AV_PIX_FMT_YUV420P, 
            SWS_BICUBIC, 0, 0, 0);
        if (!imgConvertCtx)
        {
            log_info<< "FFmpegKits::PushVideo:sws_getCachedContext:img_convert_ctx=NULL" << ",devPath=" 
                << devPath << std::endl;
            callback(MediaMessage::OpenFail);
            return;
        }

        rawFrame = av_frame_alloc();
        frameYUV = av_frame_alloc();
        frameYUV->format = AV_PIX_FMT_YUV420P;
        frameYUV->width = encodecCtx->width;
        frameYUV->height = encodecCtx->height;
        frameYUV->pts = 0;
        ret = av_frame_get_buffer(frameYUV, 0);
        if (ret < 0)
        {
            PrintError("av_frame_get_buffer", devPath, outAddr, ret);
            callback(MediaMessage::OpenFail);
            return;
        }
        decPkt = av_packet_alloc();
        encPkt = av_packet_alloc();

        switch (transProtocol) {
        case TransProtocol::rtsp:
            ret = avformat_alloc_output_context2(&outfmtCtx, nullptr, "rtsp", outAddr.c_str());
            break;
        case TransProtocol::srt:
            ret = avformat_alloc_output_context2(&outfmtCtx, nullptr, "mpegts", outAddr.c_str());
            break;
        default:
            return;
        }

        if (ret < 0)
        {
            PrintError("avformat_alloc_output_context2", devPath, outAddr, ret);
            callback(MediaMessage::OpenFail);
            return;
        }
        if (transProtocol == TransProtocol::rtsp)
            av_opt_set(outfmtCtx->priv_data, "rtsp_transport", videoPushParam.protocol.c_str(), 0);
		av_opt_set(outfmtCtx->priv_data, "buffer_size", std::to_string(videoPushParam.bitRate * 2).c_str(), 0);
		av_opt_set(outfmtCtx->priv_data, "muxdelay", "1", 0);

        // outfmt_ctx->max_interleave_delta = 10000;
        outfmtCtx->interrupt_callback.callback = interCallback;
        outfmtCtx->interrupt_callback.opaque = this;

        outStream = avformat_new_stream(outfmtCtx, nullptr);
        if (!outStream)
        {
            log_info<< "FFmpegKits::PushVideo:avformat_new_stream:out_stream=NULL" 
                << ",devPath=" << devPath << std::endl;
            callback(MediaMessage::OpenFail);
            return;
        }
        outStream->codecpar->codec_tag = 0;
        outStream->time_base = {1, videoPushParam.frameRate};

        ret = avcodec_parameters_from_context(outStream->codecpar, encodecCtx);
        if (ret < 0)
        {
            PrintError("avcodec_parameters_from_context", devPath, outAddr, ret);
            callback(MediaMessage::OpenFail);
            return;
        }

        //av_dump_format(outfmtCtx, 0, outAddr.c_str(), 1);
        // outfmt_ctx->flags |= AVFMT_FLAG_NONBLOCK;
        if (!(outfmtCtx->oformat->flags & AVFMT_NOFILE))
        {
            ret = avio_open(&outfmtCtx->pb, outAddr.c_str(), AVIO_FLAG_WRITE);
            if (ret < 0)
            {
                PrintError("avio_open", devPath, outAddr, ret);
                callback(MediaMessage::OpenFail);
                return;
            }
        }

        m_mediaMillCallTime = GetMillTimestampSinceEpoch();
        ret = avformat_write_header(outfmtCtx, nullptr);
        if (ret < 0)
        {
            PrintError("avformat_write_header", devPath, outAddr, ret);
            callback(MediaMessage::OpenFail);
            return;
        }
        escapteTime = GetMillTimestampSinceEpoch() - m_mediaMillCallTime;
        log_info<< "FFmpegKits::PushVideo:start push video, devPath=" << devPath << ",outAddr=" << outAddr
                << ",total open duration=" << GetMillTimestampSinceEpoch() - startPushTime
                << ",avformat_write_header escapte=" << escapteTime << std::endl;

        const auto millFrameTime = 1000 / videoPushParam.frameRate;
        while (m_mediaState != MediaState::Stopped)
        {
            DetechPause();

            ret = av_read_frame(infmt_ctx, decPkt);
            if (ret < 0 || decPkt->size <= 0)
            {
                if(!readCameraTime)
                    readCameraTime = GetMillTimestampSinceEpoch();

                if (ret < 0)
                {
                    auto posixErr = AVERROR(EAGAIN);
                    if(posixErr != ret)
                    {
                        if(!haveSendNoError)
                            callback(MediaMessage::OpenFail);
                        else
                            callback(MediaMessage::ReadFail);
                        PrintError("av_read_frame", devPath, outAddr, ret);
                        break;
                    }
                }

                auto curTime = GetMillTimestampSinceEpoch();
                auto millPrePackageEscapeTime = curTime - previousReadPacketTime;
                auto millReadCamEscapateTime = curTime - readCameraTime;
                //log_info << "millPrePackageEscapeTime=" << millPrePackageEscapeTime << ",millReadCamEscapateTime=" <<
                //            millReadCamEscapateTime << std::endl;
                if(millPrePackageEscapeTime < millFrameTime)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(millFrameTime - millPrePackageEscapeTime));
                    continue;
                }
                else if(millReadCamEscapateTime >= m_mediaMillMediaReadTimeout)
                {
					log_info << "FFmpegKits::PushVideo:millEscapeTime >= m_mediaMillMediaReadTimeout, devPath=" 
                        << devPath << ",outAddr=" << outAddr << ",millReadCamEscapateTime=" << millReadCamEscapateTime 
                        << std::endl;
					break;
                }

                //PrintError("av_read_frame", devPath, outAddr, ret);
                std::this_thread::sleep_for(std::chrono::milliseconds(millFrameTime));
                continue;
            }
            readCameraTime = 0;
            previousReadPacketTime = GetMillTimestampSinceEpoch();

            if(decPkt->stream_index != videoindex)
            {
                continue;
            }

            ret = avcodec_send_packet(decodecCtx, decPkt);
            av_packet_unref(decPkt);
            if (ret < 0)
            {
                PrintError("avcodec_send_packet", devPath, outAddr, ret);
                std::this_thread::sleep_for(std::chrono::milliseconds(millFrameTime));
                continue;
            }

            ret = avcodec_receive_frame(decodecCtx, rawFrame);
            if (ret < 0)
            {
                PrintError("avcodec_receive_frame2", devPath, outAddr, ret);
                std::this_thread::sleep_for(std::chrono::milliseconds(millFrameTime));
                continue;
            }

            frameYUV->pts = vpts;
            vpts += 1;
            ret = sws_scale(imgConvertCtx, (const uint8_t *const *)rawFrame->data, rawFrame->linesize, 0,
                            rawFrame->height, frameYUV->data, frameYUV->linesize);
            if (ret <= 0)
            {
                PrintError("sws_scale", devPath, outAddr, ret);
                std::this_thread::sleep_for(std::chrono::milliseconds(millFrameTime));
                continue;
            }

            ret = avcodec_send_frame(encodecCtx, frameYUV);
            if (ret < 0)
            {
                PrintError("avcodec_send_frame", devPath, outAddr, ret);
                std::this_thread::sleep_for(std::chrono::milliseconds(millFrameTime));
                continue;
            }
            ret = avcodec_receive_packet(encodecCtx, encPkt);
            if (ret < 0 || encPkt->size <= 0)
            {
                PrintError("avcodec_receive_packet", devPath, outAddr, ret);
                std::this_thread::sleep_for(std::chrono::milliseconds(millFrameTime));
                continue;
            }

            encPkt->pts = av_rescale_q(encPkt->pts, encodecCtx->time_base, outStream->time_base);
            encPkt->dts = av_rescale_q(encPkt->dts, encodecCtx->time_base, outStream->time_base);
            encPkt->duration = av_rescale_q(encPkt->duration, encodecCtx->time_base, outStream->time_base);

            m_mediaMillCallTime = GetMillTimestampSinceEpoch();
            ret = av_interleaved_write_frame(outfmtCtx, encPkt);
            if (ret < 0)
            {
                PrintError("av_interleaved_write_frame", devPath, outAddr, ret);
                if(!haveSendNoError)
                    callback(MediaMessage::OpenFail);
                break;
            }
            else
            {
                if(m_mediaState == MediaState::Stopped)
                {
                    if(!haveSendNoError)
                        callback(MediaMessage::OpenFail);
                    break;
                }
            }
            escapteTime = GetMillTimestampSinceEpoch() - m_mediaMillCallTime;
            if (escapteTime >= 1000)
                log_info<< "FFmpegKits::PushVideo:av_interleaved_write_frame:escape="
                << escapteTime << ",devPath=" << devPath << std::endl;

            if(m_mediaState == MediaState::Opening)
            {
                m_mediaState = MediaState::Playing;
                log_info<< "FFmpegKits::PushVideo:success:outAddr=" << outAddr << ",devPath=" 
                    << devPath << std::endl;
                callback(MediaMessage::NoError);
                haveSendNoError = true;
            }
        }
        if(haveSendNoError)
            callback(MediaMessage::Eof);
        av_write_trailer(outfmtCtx);
    };

    m_pushVideoThread = std::thread(pushVideoFun);
}

void FFmpegKits::StopPushing()
{
    m_mediaState = MediaState::Stopped;
    if(m_pushVideoThread.joinable())
        m_pushVideoThread.join();
}

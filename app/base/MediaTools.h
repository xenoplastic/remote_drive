/**
 * @brief lijun
 * ffmpeg封装类
 */

#pragma once

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavutil/pixfmt.h"
}

#include <functional>
#include <thread>
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <condition_variable>

struct AVFrame;
struct AVFormatContext;
struct SwsContext;

struct VideoData
{
	uint8_t* yBuf;
	uint8_t* uBuf;
	uint8_t* vBuf;
	std::string seiData;
	double secTimestamp;
	int	width;
	int	height;
    AVPixelFormat pixFmt;
	int frameRate;
};

enum class MediaType
{
    File,
    Camera,
    NetStream,
};

struct VideoPushParam
{
    int	width;                          //视频宽度
    int	height;							//视频高度
	int frameRate;						//平均帧率
    int frameGroup;						//两个关键帧之间的帧数称为一个GOP
    int keyIntMin;						//最小关键帧间隔 
    int bitRate;						//码率
    std::string preset;					//ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow
    std::string protocol;				//udp,tcp
    MediaType mediaType;
};

enum class MediaMessage
{
	OpenSuccess,
	InitFail,
	NoError,
	OpenFail,
	ReadFail,
	SeekFail,
    WriteFail,
	Eof
};

enum class  MediaState
{
	Opening,
	Playing,
	Paused,
    Stopped
};

using MEDIA_OPEN_CALLBACK = std::function<void(int)>;
using MEDIA_PARSE_CALLBACK = std::function<void(const VideoData&, MediaMessage)>;
using MEDIA_PUSH_CALLBACK = std::function<void(MediaMessage)>;

class FFmpegKits
{
public:
	FFmpegKits();
	virtual ~FFmpegKits();

	//创建编码器
	int CreateEncoder(AVCodecID codecID);

	//创建解码器
	int CreateDecoder(AVCodecID codecID);

	void Release();
	
	/// <summary>
	/// 解码
	/// </summary>
	/// <param name="inbuf">输入的经过编码的裸流</param>
	/// <param name="inbufSize">inbuf大小</param>
	/// <param name="framePara">依次返回视频的宽、高、Y、U、V缓冲宽度(单位字节)</param>
	/// <param name="outRGBBuf">输出的RGB24格式流，由外部提前分配内存</param>
	/// <param name="outYBuf">输出的Y缓冲，函数内部分配内存</param>
	/// <param name="outUBuf">输出的U缓冲，函数内部分配内存</param>
	/// <param name="outVBuf">输出的V缓冲，函数内部分配内存</param>
	/// <param name="need_key_frame">是否要求被解码的帧为关键帧</param>
	/// <param name="ft">返回被解码的帧的类型</param>
	/// <returns>成功:0,失败:非0</returns>
	int Decode(
		unsigned char* inbuf,
		int32_t inbufSize,
		int32_t* framePara,
		unsigned char* outRGBBuf,
		unsigned char** outYBuf,
		unsigned char** outUBuf,
		unsigned char** outVBuf,
		int32_t needKeyFrame,
		AVPictureType* ft
	);

	/// <summary>
	/// 编码
	/// </summary>
	/// <param name="ybuf">输入的Y缓冲</param>
	/// <param name="ubuf">输入的U缓冲</param>
	/// <param name="vbuf">输入的V缓冲</param>
	/// <param name="width">输入的视频的宽度</param>
	/// <param name="height">输入的视频的高度</param>
	/// <param name="outBuf">输出的编码数据地址,函数内部维护内存</param>
	/// <param name="outBufSize">输出的编码数据的字节大小</param>
	/// <param name="fps">编码后的帧率</param>
	/// <param name="bit_rate">编码后的码率</param>
	/// <returns>成功:0,失败:非0</returns>
	int Encode(
		uint8_t* ybuf, 
		uint8_t* ubuf, 
		uint8_t* vbuf, 
		int32_t width, 
		int32_t height, 
		unsigned char** outBuf, 
		int32_t* outBufSize, 
		int32_t fps, 
		int32_t bitRate
	);

	/// <summary>
	/// 获取媒体文件总时长(单位:秒)
	/// </summary>
	/// <returns></returns>
	int64_t GetDuration();

	/// <summary>
	/// 解析媒体文件
	/// </summary>
	/// <param name="inputMediaFile">媒体文件路径(本地磁盘路径或者媒体url)</param>
	/// <param name="videoCallback">回调</param>
	/// <param name="readTimeout">读取超时</param>
	void StartMedia(
		const std::string& inputMediaFile, 
		MEDIA_PARSE_CALLBACK videoCallback,
        int openTimeout = 10000,
        int readTimeout = 5000,
		const std::string& protocol = "tcp",
		VideoData* videoData = nullptr
	);

	/// <summary>
	/// 重新打开视频
	/// </summary>
	void RestartMedia();

	/// <summary>
	/// 退出所有解码和播放线程,停止播放
	/// </summary>
	void StopMedia();

	/// <summary>
	/// 暂停播放
	/// </summary>
	void PauseMedia();

	/// <summary>
	/// 继续播放,如果播放完毕或者停止播放,则从头开始播放
	/// </summary>
	void PlayMedia();

	/// <summary>
	/// 指定播放的位置
	/// </summary>
	/// <param name="secTimestamp">媒体的时间位置(单位:秒)</param>
	void SeekMedia(unsigned secTimestamp);

	/// <summary>
	/// 获取媒体当前状态
	/// </summary>
	MediaState GetMediaState();

	/// <summary>
	/// 获取指定播放的位置
	/// </summary>
	unsigned GetSeekPos();

	/// <summary>
	/// 直接同步方式打开媒体
	/// </summary>
	/// <param name="inputMediaFile">媒体路径</param>
	/// <param name="urlVideoData">媒体数据(打开的媒体为文件时,不可填充)</param>
	/// <returns></returns>
    int OpenMedia(
		const std::string& inputMediaFile, 
		const std::string& protocol = "udp", 
		unsigned timeout = 10000,
		VideoData* urlVideoData = nullptr
	);

	/// <summary>
	/// 获取媒体路径
	/// </summary>
	/// <returns></returns>
	std::string GetMediaPath() const;

    /// <summary>
    /// 获取推流输出路径
    /// </summary>
    /// <returns></returns>
    std::string GetOutAddr() const;

    /// <summary>
    /// Check wether media url is valid
    /// </summary>
    /// <returns></returns>
	bool IsValid(
		const std::string& inputMediaFile,
		const std::string& protocol = "tcp",
		unsigned timeout = 5000,
		unsigned testCount = 5,
		bool testRead = false
	);

	/// <summary>
    /// 推送摄像头视频
    /// </summary>
	/// <param name="devPath">设备路径</param>
	/// <param name="outAddr">输出地址</param>
	/// <param name="videoPushParam">视频属性</param>
	/// <param name="callback">回调函数地址</param>
	void PushVideo(
		const std::string& devPath,
		const std::string& outAddr,
		const VideoPushParam& videoPushParam,
		MEDIA_PUSH_CALLBACK callback,
		int millTimeout = 10000
	);

	/// <summary>
    /// 停止推流
    /// </summary>
	void StopPushing();

private:
	void DecodeLoop();

	void PlayLoop();

	void StartMediaThread();

	int InitMediaEngine();

	void StartParseMedia(
		const std::string& inputMediaFile, 
		const std::string& protocol = "tcp",
		int openTimeout = 10000,
		int readTimeout = 5000,
		VideoData* videoData = nullptr
	);

	int ParseSeiSize(const uint8_t* data, int& seiDataPos) const;

	//重置缓存，相当于清空读到的缓存
	void ResetVideoCache();

	inline uint64_t DetechPause();

	void ReleaseVideoCache();

	void Reset();

    std::string err2Str(int errorCode);

private:
    enum class TransProtocol
    {
        srt,
        rtsp
    };

    int64_t m_mediaMillCallTime = 0;
    std::string m_mediaPath;

	//解码相关
	AVCodecContext* m_AVCodecCtxDecoder = nullptr;
    const AVCodec* m_AVCodecDecoder = nullptr;
	AVPacket* m_AVPacketDecoder = nullptr;
	AVFrame* m_AVFrameDecoder = nullptr;
	AVFrame* m_frameYUVDecoder = nullptr;
	
	//编码相关
	AVPacket* m_pktEncoder = nullptr;
	AVCodecContext* m_AVCodecCtxEecoder = nullptr;
	AVFrame* m_frameEncoder = nullptr;
    const AVCodec* m_AVCodecEncoder = nullptr;

	//解码媒体流(mp4、avi等封装文件，RTSP、RTMP流)
	SwsContext* m_mediaSwsContext = nullptr;
	AVFormatContext* m_mediaContext = nullptr;
	AVCodecContext* m_mediaCodecCtx = nullptr;
	AVFrame* m_mediaFrame = nullptr;
	AVFrame* m_mediaFrameYUV = nullptr;
	uint8_t* m_mediaOutbuffer = nullptr;
	AVPacket* m_mediaPacket = nullptr;
	std::thread m_mediaDecoderThread;
	std::thread m_mediaPlayThread;
	std::thread m_mediaOpenThread;
    std::atomic<MediaState> m_mediaState;
	std::deque<VideoData*> m_mediaWriteVideoData;
	std::deque<VideoData*> m_mediaReadVideoData;
	std::mutex m_mediaMutexWVideoData;
	std::mutex m_mediaMutexRVideoData;
	MEDIA_PARSE_CALLBACK m_mediaParseCallback;
	int64_t m_mediaSecDuration = 0;	//m_mediaSecDuration为-1时,表示rtsp类似实时流		
	int m_mediaFPS = 0;
	int m_mediaVideoStreamIndex = -1;
    int m_mediaMillMediaTimeout = 10000;
	int m_mediaMillMediaOpenTimeout = 10000;
    int m_mediaMillMediaReadTimeout = 2000;
    std::atomic_uint m_mediaSecSeekPos;
    std::atomic_bool m_mediaParseSyncSeek;
    std::atomic_bool m_mediaPlaySyncSeek;
	std::condition_variable m_mediaCondVarState;
	std::condition_variable m_mediaConVarDecodeEOF;
	std::condition_variable	m_mediaConVarCanWrite;
	std::condition_variable	m_mediaConVarCanRead;
	bool m_initMediaEngine = false;
	std::string m_rtspProtocol;

	//推流相关
	std::thread m_pushVideoThread;
    std::string m_outAddr;
};

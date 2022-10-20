#include "MediaElement.h"
#include "common.h"

MediaElement::MediaElement(QWidget* parent /*= 0*/) :
	OpenGLView(parent)
{
	connect(
		this, 
		&MediaElement::mediaVideoIncoming,
		this, 
		&MediaElement::OnMediaVideoIncomming
	);
}

MediaElement::~MediaElement()
{
	StopMedia();
}

void MediaElement::StartMedia(
	const QString& inputMediaFile,
	const QString& protocol/* = "udp"*/,
	int openTimeout/* = 10000*/,
	int readTimeout /*= 5000 */,
	VideoData* videoData/* = nullptr*/
)
{
	MEDIA_PARSE_CALLBACK videoCallback = [this](
		const VideoData& videoData, 
		MediaMessage mediaMsg
		) 
	{
		switch (mediaMsg)
		{
		case MediaMessage::NoError:
			break;
		case MediaMessage::OpenFail:
		case MediaMessage::InitFail:
		case MediaMessage::ReadFail:
		case MediaMessage::SeekFail:
			emit mediaMessage(mediaMsg, GetMediaPath());
			return;
		case MediaMessage::Eof:
			emit mediaPlayFinished();
			return;
		case MediaMessage::OpenSuccess:
			ResetRender(true);
			emit mediaOpenSuccess();
			break;
		default:
			return;
		}

		VideoData* tmp = new VideoData(videoData);
		if (videoData.yBuf && videoData.uBuf && videoData.vBuf)
		{
			unsigned yBufSize = videoData.width * videoData.height;
			unsigned uBufSize = yBufSize / 4;
			unsigned vBufSize = uBufSize;
			tmp->yBuf = new uint8_t[yBufSize];
			tmp->uBuf = new uint8_t[uBufSize];
			tmp->vBuf = new uint8_t[vBufSize];
			if (!tmp->yBuf || !tmp->uBuf || !tmp->vBuf)
			{
				delete[] tmp->yBuf;
				delete[] tmp->uBuf;
				delete[] tmp->vBuf;
				return;
			}
			//copy影响效率
			memcpy_s(tmp->yBuf, yBufSize, videoData.yBuf, yBufSize);
			memcpy_s(tmp->uBuf, uBufSize, videoData.uBuf, uBufSize);
			memcpy_s(tmp->vBuf, vBufSize, videoData.vBuf, vBufSize);
		}

		emit mediaVideoIncoming(tmp);
	};
    FFmpegKits::StartMedia(
		inputMediaFile.toStdString(), 
		videoCallback, 
		openTimeout,
		readTimeout, 
		protocol.toStdString(),
		videoData
	);
}

void MediaElement::StopMedia()
{
	FFmpegKits::StopMedia();
	ResetRender(false);
}

void MediaElement::SetRender(MediaElement* mediaMediaElement)
{
	m_videoRender = mediaMediaElement;
}

MediaElement* MediaElement::Render() const
{
	return m_videoRender;
}

void MediaElement::SwapRender(MediaElement* mediaMediaElement)
{
	m_videoRender = mediaMediaElement;
	mediaMediaElement->m_videoRender = this;
}

void MediaElement::OnMediaVideoIncomming(VideoData* videoData)
{
	if (videoData->yBuf && videoData->uBuf && videoData->vBuf)
	{
		int videoWidth = videoData->width;
		int videoHeight = videoData->height;
		m_videoRender->LoadYUV(
			videoData->yBuf,
			videoData->uBuf,
			videoData->vBuf,
			videoWidth,
			videoHeight
		);
		delete[]videoData->yBuf;
		delete[]videoData->uBuf;
		delete[]videoData->vBuf;
		delete videoData;
	}
}


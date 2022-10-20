#pragma once

#include <string>

#include "OpenGLView.h"
#include "MediaTools.h"

class MediaElement : public OpenGLView, public FFmpegKits
{
	Q_OBJECT
public:
	MediaElement(QWidget* parent = nullptr);
	~MediaElement();

	void StartMedia(
		const QString& inputMediaFile,
		const QString& protocol = "udp",
		int openTimeout = 10000,
		int readTimeout = 5000,
		VideoData* videoData = nullptr
	);

	void StopMedia();

	void SetRender(MediaElement* mediaMediaElement);

	MediaElement* Render() const;

	void SwapRender(MediaElement* mediaMediaElement);

public slots:
	void OnMediaVideoIncomming(VideoData* videoData);

signals:
	void mediaVideoIncoming(VideoData*);
	void mediaOpenSuccess();
	void mediaPlayFinished();
	void mediaMessage(MediaMessage mediaMsg, const std::string& mediaPath);

private:
	MediaElement* m_videoRender = this;
};
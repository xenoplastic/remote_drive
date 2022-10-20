#pragma once

#include <QObject>
#include <QByteArray>
#include <QThread>
#include <QString>
#include <QHash>
#include <QMutex>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <memory>

#include "Defines.h"
#include "MediaTools.h"

struct Config;
class QProcess;
class WebSocketServerHost;

using FFMPEGKITS_PTR = std::shared_ptr<FFmpegKits>;

struct FFmpegKitsVideoInfo
{
    QString roomId;
    FFMPEGKITS_PTR ffmpegKits;
};

class TaskPump : public QObject
{
    Q_OBJECT
public:
    TaskPump();
    ~TaskPump();

    void Start();
    void Stop();
    void PushTask(Action act,const QByteArray& data);
    void SetInitServerTime(qint64 initServerTime);
    void SetServerTimeOffset(qint64 serverTimeOffset);

signals:
    void DataOut(const QString& data);
    void TaskIncoming(const QByteArray& data);
    void FinishPushingVideo(const QString& successCameras);
    void StopPushingVideo();
    void PushVideoMessageIncoming(MediaMessage mediaMessage, const QString& roomId,
                                  const QString& cameraName);

protected slots:
    void OnTaskIncoming(const QByteArray& data);
    void OnPushVideoMessageIncoming(MediaMessage mediaMessage, const QString& roomId,
                                    const QString& cameraName);

private:
    void PushVideo(FFMPEGKITS_PTR ffmpegKits, const QString& cameraName, const QString& driverId,
                   const QString& roomId);

    void OnActPushVideo(const QJsonObject& obj);
    inline void StopPushVideoProcesses();

private:
    QThread m_workThread;
    Config* m_config;
    std::atomic_bool m_pushVideo;
    QHash<QString, FFmpegKitsVideoInfo> m_camsFFmpegKits;
    QHash<QString, QSet<QString>> m_camsListeners;
};

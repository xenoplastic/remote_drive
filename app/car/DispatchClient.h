#pragma once

#include "WebSocket.h"
#include "SysInfo.h"

#include <QObject>

struct Config;
class TaskPump;
class WebSocketServerHost;
class HttpClient;
class UpdateDetector;

class DispatchClient : public QObject
{
    Q_OBJECT
public:
    explicit DispatchClient(QObject *parent = nullptr);
    ~DispatchClient();

    void StartWork();

protected slots:
	void OnMessageRecieve(const WebSocketMessage& msg);
    void OnGetServerTimeFail();
    void OnRosDataIncoming(const QString& data);
    void OnDownloadNewVersionFail();

signals:
    void SysDataIncoming(const SysData& sysData);
    void GetServerTimeFail();
    void DownloadNewVersionFail();

private:
    void HandleDate(const QByteArray& data);
    void SendCarData(const QString& driverID);
    bool SendCarData();
    void Update();
    void ConnectDispatchServer();
    void GetDispatchTime();
    void DownloadNewVersion();
    void CancelUpdate();
    void RepalceCarProgram(const QString& newPackage, const QString& outDir);
    void SendUpdateFail();

private:
    HttpClient* m_httpClient;
    WebSocket* m_webSocket;
    TaskPump* m_taskPump;
    SysInfo m_sysInfo;
    quint64 m_netDelay = 0;
    qint64 m_initServerTime;
    qint64 m_serverTimeOffset;
    WebSocketServerHost* m_websocketServer;
    Config* m_config;
    UpdateDetector* m_updateDetector;
    bool m_updating = false;
};

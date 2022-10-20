#pragma once

#include <QString>
#include <QByteArray>
#include <QWebSocket>

class QWebSocket;
class QTimer;

enum class WebSocketMsgCode
{
	connectSuccess,
	connectFail,
	connectTimeout,
    connectRefused,
	disconnected,
    dataRecieved,
};

struct WebSocketMessage
{
    WebSocketMsgCode msgCode;
    QString data;
};

class WebSocket : public QWebSocket
{
    Q_OBJECT
public:
    WebSocket(QObject* parent = nullptr);
    virtual ~WebSocket();

    //连接
    void Connect(const QString& url, int mSecond = 10000);
    //重连
    void Reconnect(int timeout);
    //停止重连
    void StopReconnect();

    void SetPingPongInterval(int mSecond);

    int GetPingPongInterval() const;

    QAbstractSocket::SocketState GetState() const;

signals:
    void MessageRecieve(const WebSocketMessage& msg);
    void NetDelayChange(qint64 netDely);

protected:
    quint64 m_netDelay = -1;
    QTimer* m_timer;
    QTimer* m_pingPongTimer;
    QAbstractSocket::SocketState m_curState;
    int m_pingPongInterval = 30 * 1000;
};

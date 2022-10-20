#pragma once

#include <QList>
#include <QObject>

class QWebSocketServer;
class QWebSocket;

class WebSocketServerHost : public QObject
{
	Q_OBJECT
public:
    WebSocketServerHost(quint16 port, QObject* parent = nullptr);
    ~WebSocketServerHost();

	bool Listen();
	void SendTextMessage(const QString& data);
    void Close();

signals:
    void textMessageReceived(const QString& messag);
    void sendData(const QString& data);

private:
	QList<QWebSocket*> m_clients;
	QWebSocketServer* m_websocketServer;
	quint16 m_port;
};

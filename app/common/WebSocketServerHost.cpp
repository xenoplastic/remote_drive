#include "WebSocketServerHost.h"

#include <QWebSocketServer>
#include <QWebSocket>

WebSocketServerHost::WebSocketServerHost(quint16 port, QObject* parent /*= nullptr*/) : 
    QObject(parent)
{
    m_websocketServer = new QWebSocketServer(QStringLiteral("WebSocketServerHost"),
                                             QWebSocketServer::NonSecureMode, this);
    m_port = port;
    connect(m_websocketServer, &QWebSocketServer::newConnection,
        this, [this]
        {
            qDebug() << "QWebSocketServer::newConnection" << "\n";
            QWebSocket* pSocket = m_websocketServer->nextPendingConnection();
            connect(pSocket, &QWebSocket::textMessageReceived, this, [this](QString message)
                {
                    //qDebug() << message << "\n";
                    emit textMessageReceived(message);
                }
            );
            connect(pSocket, &QWebSocket::disconnected, this, [this]
                {
                    QWebSocket* pClient = qobject_cast<QWebSocket*>(sender());
                    if (pClient)
                    {
                        m_clients.removeAll(pClient);
                        pClient->deleteLater();
                    }
                }
            );
            m_clients << pSocket;
        }
    );
    connect(this, &WebSocketServerHost::sendData, this, [this](const QString& data)
        {
            for (auto client : m_clients)
            {
                if(client->isValid())
                    client->sendTextMessage(data);
            }
        }
    );
}

WebSocketServerHost::~WebSocketServerHost()
{
    m_websocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

bool WebSocketServerHost::Listen()
{
	return m_websocketServer->listen(QHostAddress::Any, m_port);
}

void WebSocketServerHost::SendTextMessage(const QString& data)
{
    emit sendData(data);
}

void WebSocketServerHost::Close()
{
    m_websocketServer->close();
}

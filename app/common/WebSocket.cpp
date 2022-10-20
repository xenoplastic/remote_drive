#include "WebSocket.h"

#include <QTimer>
#include <QNetworkRequest>

WebSocket::WebSocket(QObject* parent) :
	QWebSocket("", QWebSocketProtocol::VersionLatest, parent),
    m_timer(new QTimer(parent)),
    m_pingPongTimer(new QTimer(parent))
{
	connect(this, &QWebSocket::connected, this, [=]()
		{     
            if(!m_timer->isActive())
            {
                this->abort();
                return;
            }
			WebSocketMessage msg;
			msg.msgCode = WebSocketMsgCode::connectSuccess;
			emit MessageRecieve(msg);
			m_timer->stop();
			m_curState = QAbstractSocket::ConnectedState;
			m_pingPongTimer->setSingleShot(false);
			m_pingPongTimer->start(m_pingPongInterval);
		});
	m_timer->callOnTimeout([=]()
		{
			WebSocketMessage msg;
			msg.msgCode = WebSocketMsgCode::connectTimeout;
			emit MessageRecieve(msg);
			m_curState = QAbstractSocket::UnconnectedState;
            //must not call abort() or close(), that cause that can not connect forever
		});
	m_pingPongTimer->callOnTimeout([=]()
		{
			this->ping();
		});
    connect(this, &QWebSocket::textMessageReceived, this,
            [=](const QString& message)
    {
        WebSocketMessage msg;
        msg.msgCode = WebSocketMsgCode::dataRecieved;
        msg.data = message;
        emit MessageRecieve(msg);
    });
    connect(this, &QWebSocket::disconnected, this, [=]()
    {
        if (m_curState == QAbstractSocket::UnconnectedState)
            return;
        m_timer->stop();
        m_pingPongTimer->stop();
        WebSocketMessage msg;
        if (m_curState == QAbstractSocket::ConnectingState)
            msg.msgCode = WebSocketMsgCode::connectFail;
        else
            msg.msgCode = WebSocketMsgCode::disconnected;
        emit MessageRecieve(msg);
    });
	connect(this, &QWebSocket::pong, this, [this]
	(quint64 elapsedTime, const QByteArray& payload)
		{
			if (m_netDelay != elapsedTime)
				emit NetDelayChange(elapsedTime);
			m_netDelay = elapsedTime;
		}
	);
}

WebSocket::~WebSocket()
{
    m_pingPongTimer->stop();
	m_timer->stop();
	this->close();
    m_curState = QAbstractSocket::ClosingState;
}

void WebSocket::Connect(const QString& url, int  mSecond/* = 10000*/)
{
	m_timer->setSingleShot(true);
    m_timer->start(mSecond);
	m_curState = QAbstractSocket::ConnectingState;
    this->open(QNetworkRequest(url));
}

void WebSocket::Reconnect(int timeout)
{
	this->abort();
	this->open(QUrl(this->requestUrl()));
	m_timer->setSingleShot(true);
	m_timer->start(timeout);
    m_curState = QAbstractSocket::ConnectingState;
}

void WebSocket::StopReconnect()
{
	m_timer->stop();
	this->abort();
    m_curState = QAbstractSocket::ClosingState;
}

void WebSocket::SetPingPongInterval(int mSecond)
{
    if(m_pingPongInterval == mSecond)
        return;
    m_pingPongInterval = mSecond;
    if (m_pingPongTimer->isActive())
    {
        m_pingPongTimer->stop();
        m_pingPongTimer->setInterval(mSecond);
        m_pingPongTimer->start();
    }
    else
    {
        m_pingPongTimer->setInterval(mSecond);
    }
}

int WebSocket::GetPingPongInterval() const
{
    return m_pingPongInterval;
}

QAbstractSocket::SocketState WebSocket::GetState() const
{
    return m_curState;
}

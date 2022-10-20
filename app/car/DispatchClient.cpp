#include "DispatchClient.h"
#include "Config.h"
#include "TaskPump.h"
#include "CarData.h"
#include "WebSocketServerHost.h"
#include "HttpClient.h"
#include "UpdateDetector.h"
#include "Defines.h"
#include "SoLogger.h"
#include "BaseFunc.h"

#include <iostream>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QProcess>
#include <QSettings>

using namespace std;

DispatchClient::DispatchClient(QObject *parent)
    : QObject{parent},
      m_webSocket(new WebSocket(this)),
      m_httpClient(new HttpClient(this)),
      m_taskPump(new TaskPump()),
      m_updateDetector(new UpdateDetector(this))
{
    connect(this, &DispatchClient::DownloadNewVersionFail, this, 
        &DispatchClient::OnDownloadNewVersionFail, Qt::QueuedConnection);
    connect(this, &DispatchClient::GetServerTimeFail, this, 
        &DispatchClient::OnGetServerTimeFail, Qt::QueuedConnection);
    connect(m_webSocket, &WebSocket::MessageRecieve, this, &DispatchClient::OnMessageRecieve);
    connect(m_taskPump, &TaskPump::DataOut, this, [this](const QString& data){
        m_webSocket->sendTextMessage(data);
    });
    connect(m_webSocket, &WebSocket::pong, this, [this]
    (quint64 elapsedTime, const QByteArray &payload)
    {
        m_netDelay = elapsedTime;
    });
    connect(this, &DispatchClient::SysDataIncoming, this, [this](const SysData& sysData)
    {
        QString data = QString(
                    "{"
                        "\"action\":%1, \"car_id\":\"%2\","
                        "\"netDelay\":%3,"
                        "\"cpu_percent\":%4,"
                        "\"mb_memory_total\":%5,\"mb_memory_free\":%6,"
                        "\"mb_disk_total\":%7, \"mb_disk_free\":%8,"
                        "\"kb_upload_net_speed\":%9, \"kb_down_net_speed\":%10"
                    "}"
                    ).
                arg((int)Action::ActCarLocalDataUp).arg(Config::getInstance()->m_clientId).
                arg(m_netDelay).
                arg(sysData.cpuPercent).
                arg(sysData.mbMemoryTotal).arg(sysData.mbMemoryFree).
                arg(sysData.mbDiskTotal).arg(sysData.mbDiskFree).
                arg(sysData.kUploadNetSpeed).arg(sysData.kDownNetSpeed);
        m_webSocket->sendTextMessage(data);
    });
    connect(m_taskPump, &TaskPump::FinishPushingVideo, this,
            [this](const QString& successCameras)
    {
        if(!successCameras.isEmpty() && !m_sysInfo.IsMonitoring())
        {
            m_sysInfo.StartMinitor(
                        SysInfoType::all,
                        3,
                        [this](const SysData& sysData)
            {
                emit SysDataIncoming(sysData);
            });
        }
    });
    connect(m_taskPump, &TaskPump::StopPushingVideo, this,[this]
    {
        m_sysInfo.StopMinitor();
    });
    connect(m_updateDetector, &UpdateDetector::UpdateFinish, this, [this](UpdateResult updateResult)
    {
        log_info << "updateResult=" << (int)updateResult;
        switch (updateResult) {
        case UpdateResult::UpdateError:
        case UpdateResult::IsUpToDate:
            CancelUpdate();
            ConnectDispatchServer();
            break;
        case UpdateResult::HaveNewerVersion:
            ConnectDispatchServer();
            DownloadNewVersion();
            break;
        default:
            break;
        }
    },
    Qt::QueuedConnection);

    m_config = Config::getInstance();
    m_websocketServer = new WebSocketServerHost(m_config->m_websocketServerPort);
    if(!m_websocketServer->Listen())
    {
        qDebug() << "m_websocketServer->Listen fail" << "\n";
    }
    else
    {
        connect(m_websocketServer, &WebSocketServerHost::textMessageReceived, this,
                &DispatchClient::OnRosDataIncoming);
    }
}

DispatchClient::~DispatchClient()
{
    delete m_taskPump;
}

void DispatchClient::OnMessageRecieve(const WebSocketMessage& msg)
{
    switch (msg.msgCode)
    {
    case WebSocketMsgCode::connectSuccess:
        log_info << "WebSocketMsgCode::connectSuccess" << endl;
        m_taskPump->Start();
        break;
    case WebSocketMsgCode::connectFail:
    {
        QThread::sleep(1);
        log_info << "WebSocketMsgCode::connectFail" << endl;
        ConnectDispatchServer();
    }
        break;
    case WebSocketMsgCode::connectTimeout:
    {
        QThread::sleep(1);
        log_info << "WebSocketMsgCode::connectTimeout" << endl;
        ConnectDispatchServer();
    }
        break;
    case WebSocketMsgCode::disconnected:
    {
        QThread::sleep(1);
        log_info << "WebSocketMsgCode::disconnected" << endl;
        m_sysInfo.StopMinitor();
        ConnectDispatchServer();
    }
        break;
    case WebSocketMsgCode::dataRecieved:
    {
        if(m_updating)
            return;
        cout << "DispatchClient::OnMessageRecieve:data=" << msg.data.toStdString() << endl;
        QStringList list = msg.data.split("\n");
        for (auto& data : list)
        {
            HandleDate(data.toUtf8());
        }
    }
        break;
    default:
        break;
    }
}

void DispatchClient::OnGetServerTimeFail()
{
    QThread::sleep(1);
    GetDispatchTime();
}

void DispatchClient::OnDownloadNewVersionFail()
{
    QThread::sleep(1);
    DownloadNewVersion();
}

void DispatchClient::StartWork()
{   
    //run Update
    //  case 1:run GetDispatchTime when update return UPdateResult::UpdateError and UPdateResult::IsUpToDate
    //  case 2:run DownloadNewVersion when update return UPdateResult::HaveNewerVersion
    GetDispatchTime();
}

void DispatchClient::Update()
{
    QString version;
    QString versionFilePath = QCoreApplication::applicationDirPath() + "/VERSION";
    QFile file(versionFilePath);
    if(file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        auto date = file.readAll();
        version = QString::fromUtf8(date);
        version.replace(" ", "");
        version.replace("\n", "");
        file.close();
    }
    m_updating = true;

    QString url = "http://" + m_config->m_logisticsAddress + URL_LOGISTICS_GET_CAR_VERSION;
    m_updateDetector->CheckUpdate(url, version);
}

void DispatchClient::ConnectDispatchServer()
{
    auto carData = CarData::getInstance(m_config->m_clientId);
    QString webSocketUrl = QString("ws://%1/ws?id=%2&info=%3&state=%4&role=0").
            arg(m_config->m_dispatchAddress, m_config->m_clientId, m_config->m_carInfo).
            arg(m_updating ? (int)CarState::Updating : (int)carData->m_carState);
    m_webSocket->SetPingPongInterval(m_config->m_dispatchPingpongTime);
    m_webSocket->Connect(webSocketUrl);
}

void DispatchClient::GetDispatchTime()
{
    //首先获取服务器时间戳
    QString getTimeUrl = QString("http://%1%2?request_time=%3").
        arg(m_config->m_dispatchAddress, URL_DISPATCH_GET_TIME).
        arg(QDateTime::currentMSecsSinceEpoch());

    m_httpClient->Get(getTimeUrl,
        [=](const QByteArray& data, HttpErrorCode resCode) {
            if(resCode != HttpErrorCode::Success)
            {
                log_info << "DispatchClient::GetDispatchTime:resCode != HttpErrorCode::Success" << endl;
                emit GetServerTimeFail();
                return;
            }

            log_info << "GetDispatchTime:resCode == HttpErrorCode::Success" << endl;
            QJsonParseError jsonError;
            QJsonDocument doucment = QJsonDocument::fromJson(data, &jsonError);
            if (jsonError.error == QJsonParseError::NoError &&
                doucment.isObject())
            {
                const QJsonObject& obj = doucment.object();
                auto requestTime = obj["request_time"].toString().toULongLong();
                auto serverTime = obj["server_time"].toInteger();
                m_serverTimeOffset = QDateTime::currentMSecsSinceEpoch();

                auto escapeTime = m_serverTimeOffset - requestTime;
                log_info << "DispatchClient::GetDispatchTime:" << "escapeTime=" << escapeTime << endl;
                if (escapeTime >= m_config->m_dispatchGetTimeTimeout)
                {                  
                    emit GetServerTimeFail();
                    return;
                }

                m_initServerTime = serverTime + (escapeTime) / 2;

                Update();
            }
            else
            {
                log_info << "DispatchClient::GetDispatchTime:doucment is not json object" << endl;
				emit GetServerTimeFail();
				return;
            }
        }
    );
}

void DispatchClient::DownloadNewVersion()
{
    log_info << "start DownloadNewVersion";
    QString appDir = QCoreApplication::applicationDirPath();
    QString strdownloadDir = QString("%1/../download").arg(appDir);
    QDir downloadDir;
    if(!downloadDir.exists(strdownloadDir))
    {
        downloadDir.mkpath(strdownloadDir);
    }
    auto updateTimestamp = GetMillTimestampSinceEpoch();
    QString downPath = strdownloadDir + "/remote_drive_car.gz";
    QString url = "http://" + m_config->m_logisticsAddress + URL_LOGISTICS_CAR_PACKAGE_URL;
    m_updateDetector->Download(url, downPath,
                               [=](qint64 bytesReceived, qint64 writeSize,
                               qint64 totalSize, HttpErrorCode resCode) mutable
    {
        switch (resCode) {
        case HttpErrorCode::Downloading:
        {          
            //notify driver process
            auto curTimestamp = GetMillTimestampSinceEpoch();
            auto updateEscapeTime = curTimestamp - updateTimestamp;
            if(updateEscapeTime <= 2000)
            {
                return;
            }                  
            updateTimestamp = curTimestamp;

            if(totalSize)
            {
                log_info << "HttpErrorCode::Downloading:" << "bytesReceived=" << bytesReceived <<
                         " m_writeSize=" << writeSize << " m_totalSize=" << totalSize
                         << ",percent=" << (double)writeSize/totalSize << std::endl;
            }
            else
            {
                log_info << "HttpErrorCode::Downloading:" << "bytesReceived=" << bytesReceived <<
                         " m_writeSize=" << writeSize << " m_totalSize=" << totalSize;
            }

            QString sendData =  QString(
                        "{"
                            "\"car_id\":\"%1\","
                            "\"action\":%2,"
                            "\"total_size\":%3,"
                            "\"written_size\":%4"
                        "}"
                        ).
                    arg(m_config->m_clientId).
                    arg((int)Action::ActCarUpdateProcess).
                    arg(totalSize).
                    arg(writeSize);
            m_webSocket->sendTextMessage(sendData);
        }
            break;
        case HttpErrorCode::Success:
            log_info << "HttpErrorCode::Success:start RepalceCarProgram" << std::endl;
            RepalceCarProgram(downPath, appDir);
            break;
        case HttpErrorCode::Fail:
        {
            log_info << "HttpErrorCode::Fail" << std::endl;
            emit DownloadNewVersionFail();
        }
            break;
        case HttpErrorCode::OpenFileFial:
        {
            log_info << "HttpErrorCode::OpenFileFial" << std::endl;
            //notify driver update fail
            //CancelUpdate();
            SendUpdateFail();
        }
            break;
        default:
            break;
        }
    });
}

void DispatchClient::CancelUpdate()
{
    log_info << "CancelUpdate";
    m_updating = false;
}

void DispatchClient::RepalceCarProgram(const QString& newPackage, const QString& outDir)
{
    log_info << "start RepalceCarProgram";

    auto config = Config::getInstance();
    QProcess proc;
    QString cmd = QString("rm -rf %1").arg(outDir);
    proc.startCommand(cmd);
    proc.waitForFinished();

    QDir dirOut;
    if (dirOut.exists(outDir))
    {
        SendUpdateFail();
        return;
    }
    else
    {
        if (!dirOut.mkpath(outDir))
        {
            SendUpdateFail();
            return;
        }
        QString cmd = QString("tar zxvf %1 -C %2 --strip-components 2").arg(newPackage, outDir);
        proc.startCommand(cmd);
        if(!proc.waitForFinished())
        {
            SendUpdateFail();
            return;
        }

        //update config
        QString configPath = QString("%1/ConfigCar.ini").arg(outDir);
        QSettings set(configPath, QSettings::IniFormat);
        set.setValue("/Camera/left", config->m_leftCameraPath);
        set.setValue("/Camera/right", config->m_rightCameraPath);
        set.setValue("/Camera/front", config->m_frontCameraPath);
        set.setValue("/Camera/back", config->m_backCameraPath);
        set.setValue("/Account/info", config->m_carInfo);
    }

    m_websocketServer->Close();
    m_webSocket->close();
    m_updating = false;
    QString versionFilePath = outDir + "/VERSION";
    QFile file(versionFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
     {
        SendUpdateFail();
        return;
    }
    else
    {
        auto newVersion = m_updateDetector->GetServerCarVersion();
        QByteArray  byteNewVersion = newVersion.toUtf8();
        file.write(byteNewVersion, byteNewVersion.length());
        file.close();
    }

    qApp->quit();
    QProcess::startDetached(qApp->applicationFilePath());
}

void DispatchClient::SendUpdateFail()
{
    QString sendData =  QString(
                "{"
                    "\"car_id\":\"%1\","
                    "\"action\":%2"
                "}"
                ).
            arg(m_config->m_clientId).
            arg((int)Action::ActCarUpdateFail);
    m_webSocket->sendTextMessage(sendData);
}

void DispatchClient::OnRosDataIncoming(const QString& data)
{
    auto updateResult = CarData::getInstance(m_config->m_clientId)->UpdateData(data);
    if(updateResult == ErrorCarData::ECD_DATA_NO_CHANGE ||
            updateResult == ErrorCarData::ECD_NOT_STATE_DATA ||
            updateResult == ErrorCarData::ECD_NO_TYPE)
    {
        //qDebug () << "TaskPump::OnRosDataIncoming:return:data=" << data << "\n";
        return;
    }

    if(m_updating)
        return;

    QString sendData =  QString(
                "{"
                    "\"car_id\":\"%1\","
                    "\"action\":%2,"
                    "\"data\":%3"
                "}"
                ).
            arg(m_config->m_clientId).
            arg((int)Action::ActRosDataToDrivers).
            arg(data);
    //qDebug () << "TaskPump::OnRosDataIncoming:sendData=" << sendData << "\n";
    m_webSocket->sendTextMessage(sendData);
}

void DispatchClient::HandleDate(const QByteArray& data)
{
    QJsonParseError jsonError;
    QJsonDocument doucment = QJsonDocument::fromJson(data, &jsonError);
    if (jsonError.error == QJsonParseError::NoError && doucment.isObject())
    {
        const QJsonObject& obj = doucment.object();
        auto act = (Action)obj.find("action")->toInt();
        switch (act)
        {
        case Action::ActPushVideo:
        {
            auto dirverID = obj.find("driver_id")->toString();
            SendCarData(dirverID);
            m_taskPump->PushTask(act, data);
        }
            break;
        case Action::ActExitPushVideo:
        {
           m_taskPump->PushTask(act, data);
        }
            break;
        case Action::ActDriveDataToRos:
        {
            auto valData = obj["data"];
            if(valData.isObject())
            {
                auto dataGlobalTime = obj["global_time"].toInteger();
                auto curTime = QDateTime::currentMSecsSinceEpoch();
                auto curGlobalTime = m_initServerTime + (curTime - m_serverTimeOffset);
                auto curGlobalEscapeTime = curGlobalTime - dataGlobalTime;
                if(curGlobalEscapeTime <= 0)
                {
                    //由于驾驶端和车端获取服务端时间戳存在误差，因此会造成负值
                    log_info << "curGlobalEscapeTime<=0" << endl;
                    curGlobalEscapeTime = m_netDelay;
                }

                auto objData = valData.toObject();
                objData.insert("epoch_millsec_timestamp", curTime);
                objData.insert("global_escape_time", curGlobalEscapeTime);

                auto strData = QString(QJsonDocument(objData).toJson(QJsonDocument::Compact));
                log_info << "Action::ActDriveDataToRos:data="
                         << strData.toStdString() << ",curGlobalEscapeTime=" << curGlobalEscapeTime << endl;
                //过滤数据
                //1.对于控制车移动的数据，如果到达车端时已经超过配置文件中的时间,则丢弃
                auto rosDataType = objData["type"].toString();
                if(rosDataType == "remote_control")
                {
                    if (curGlobalEscapeTime >= m_config->m_rosControlTimeout)
                    {
                        return;
                    }
                }

                m_websocketServer->SendTextMessage(strData);
                qDebug() << "TaskPump::OnTaskIncoming:Action::ActDriveDataToRos"
                         << strData << "\n";
            }
        }
            break;
        case Action::ActNewDriverUp:
        {
            auto dirverID = obj.find("driver_id")->toString();
            SendCarData(dirverID);
        }
            break;
        default:
            break;
        }
    }
}

void DispatchClient::SendCarData(const QString& driverID)
{
    auto carData = CarData::getInstance(m_config->m_clientId)->m_data;
    if(carData.isEmpty())
    {
        cout << "DispatchClient::SendCarData:carData.isEmpty()" << endl;
        return;
    }
    QString sendData = QString(
                "{"
                    "\"action\":%1,"
                    "\"car_id\":\"%2\","
                    "\"driver_id\":\"%3\","
                    "\"data\":%4"
                "}"
                ).
            arg((int)Action::ActRosDataToDriver).
            arg(
                m_config->m_clientId,
                driverID,
                carData
                );
    m_webSocket->sendTextMessage(sendData);
    cout << "DispatchClient::SendCarData:m_webSocket->SendTextMessage(sendData),sendData="
         << sendData.toStdString() << endl;
}

bool DispatchClient::SendCarData()
{
    QString data = CarData::getInstance(m_config->m_clientId)->m_data;
    if(!m_webSocket->isValid() || data.isEmpty())
        return false;

    QString sendData =  QString(
                "{"
                    "\"car_id\":\"%1\","
                    "\"action\":%2,"
                    "\"data\":%3"
                "}"
                ).
            arg(m_config->m_clientId).
            arg((int)Action::ActRosDataToDrivers).
            arg(data);
    qDebug () << "TaskPump::OnRosDataIncoming:sendData=" << sendData << "\n";
    m_webSocket->sendTextMessage(sendData);
    return true;
}

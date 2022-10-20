#include "Config.h"

#include <QCoreApplication>
#include <QSettings>
#include <QUuid>

using namespace std;

std::unique_ptr<Config> Config::m_instance = nullptr;

Config::Config()
{
    QString configPath = QString("%1/ConfigCar.ini").arg(
                QCoreApplication::applicationDirPath());
    QSettings set(configPath, QSettings::IniFormat);
    m_logisticsAddress = set.value("/Server/logistics_address").toString();
    m_dispatchAddress = set.value("/Server/dispatch_address").toString();
    m_mediaAddr = set.value("/Server/media_address").toString();
    m_carInfo =  set.value("/Account/info").toString();
    m_leftCameraPath =  set.value("/Camera/left").toString();
    m_rightCameraPath =  set.value("/Camera/right").toString();
    m_frontCameraPath =  set.value("/Camera/front").toString();
    m_backCameraPath = set.value("/Camera/back").toString();
    m_clientId = QUuid::createUuid().toString();
    m_pushVideoTimeout = set.value("/Server/push_video_timeout").toInt();
    m_websocketServerPort = set.value("/Common/websocketserver_port").toInt();
    m_videoPreset = set.value("/Camera/videoPreset").toInt();
    m_videoWidth = set.value("/Camera/videoWidth").toString();
    m_videoHeight = set.value("/Camera/videoHeight").toString();
    m_bitRate = set.value("/Camera/bitRate").toString();
    m_frameRate = set.value("/Camera/frameRate").toString();
    m_frameGroup = set.value("/Camera/frameGroup").toString();
    m_keyIntMin = set.value("/Camera/keyIntMin").toString();
    m_rtspTransProtocol = set.value("/Camera/rtsp_trans_protocol").toString();
    m_mediaTransProtocols = set.value("/Camera/media_trans_protocol").toString();
    m_rosControlTimeout = set.value("/Common/ros_control_timeout").toInt();
    m_dispatchPingpongTime = set.value("/Common/dispatch_pingpong_time").toInt();
    m_dispatchGetTimeTimeout = set.value("/Common/dispatch_gettime_timeout").toInt();
}

Config* Config::getInstance()
{
    if(m_instance)
        return m_instance.get();
    auto p = new Config();
    m_instance.reset(p);
    return p;
}

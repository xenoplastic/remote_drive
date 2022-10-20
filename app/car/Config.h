#pragma once

#include <QString>
#include <memory>

struct Config
{
    static Config* getInstance();

    QString m_logisticsAddress;
    QString m_dispatchAddress;
    QString m_mediaAddr;
    QString m_carInfo;
    QString m_clientId;
    QString m_leftCameraPath;
    QString m_rightCameraPath;
    QString m_frontCameraPath;
    QString m_backCameraPath;
    QString m_videoWidth;                //视频宽度
    QString m_videoHeight;               //视频高度
    QString m_bitRate;                   //推流码率
    QString m_frameRate;                 //推流帧率
    QString m_frameGroup;                //GOP间隔
    QString m_keyIntMin;                 //最小关键帧间隔
    QString m_rtspTransProtocol;         //RTSP传输协议
    QString m_mediaTransProtocols;       //流媒体协议
    int m_videoPreset;                   //视频压缩级别，数值越大，视频越逼真
    int m_pushVideoTimeout;
    quint16 m_websocketServerPort;
    int m_rosControlTimeout;
    int m_dispatchPingpongTime;
    int m_dispatchGetTimeTimeout;

private:
    Config();

    static std::unique_ptr<Config> m_instance;
};

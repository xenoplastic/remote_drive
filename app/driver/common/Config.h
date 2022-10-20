#pragma once

#include <QString>
#include <memory>
#include <QHash>

struct Config
{
    static Config* getInstance();
    void SetSpeeds(const QHash<int, double>& speeds);
    void SetVideoModelVisible(bool show);
    void SetShowCarSystemInfo(bool show);

    QString m_logisticsAddress;
    QString m_dispatchAddress;
    QString m_mediaAddr;
    QString m_driverInfo;
    QString m_clientId;
    QString m_area;
    QString m_pullVideoProtocol;
    QString m_mediaTransProtocol;
    int m_logitechUpdateRate;
    QHash<int, double> m_carSpeedsConfig{
        {0, 0},
        {1, 0.4},
        {2, 0.8},
        {3, 1.2},
        {4, 1.6},
        {5, 2.0},
        {6, -0.4},
    };
    int m_dispatchPingpongTime;
    int m_dispatchGetTimeTimeout;
    int m_openVideoTimeout;
    int m_readVideoTimeout;
    int m_netWarningDelay;
	double m_mapOriginX;
	double m_mapOriginY;
	double m_mapResolution;
    bool m_showCarSystemInfo = false;
    bool m_showVideoModel = false;

private:
    Config();

    static std::unique_ptr<Config> m_instance;
};

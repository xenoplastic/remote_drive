#include "Config.h"
#include "DriverDefines.h"

#include <QCoreApplication>
#include <QSettings>
#include <QUuid>
#include <QStandardPaths>
#include <QFile>

using namespace std;

std::unique_ptr<Config> Config::m_instance = nullptr;

Config::Config()
{
    QString configDrivePath = QString("%1/ConfigDriver.ini").arg(
                QCoreApplication::applicationDirPath());
    QSettings set(configDrivePath, QSettings::IniFormat);
    m_dispatchAddress = set.value("/Server/dispatch_address").toString();
    m_logisticsAddress = set.value("/Server/logistics_address").toString();
    m_mediaAddr = set.value("/Server/media_address").toString();
    m_driverInfo =  set.value("/Account/info").toString();
    m_clientId = QUuid::createUuid().toString();
    m_logitechUpdateRate = set.value("/Common/LogitechUpdateRate").toInt();
    m_dispatchPingpongTime = set.value("/Common/dispatch_pingpong_time").toInt();
    m_dispatchGetTimeTimeout = set.value("/Common/dispatch_gettime_timeout").toInt();
    m_area = set.value("/Account/area").toString();
    m_pullVideoProtocol = set.value("/Common/rtspTransProtocol").toString();
	m_openVideoTimeout = set.value("/Common/openVideoTimeout").toInt();
	m_readVideoTimeout = set.value("/Common/readVideoTimeout").toInt();
    m_netWarningDelay = set.value("/Common/netWarningDelay").toInt();
	m_mapOriginX = set.value("/Common/mapOriginX").toDouble(); 
	m_mapOriginY = set.value("/Common/mapOriginY").toDouble();
	m_mapResolution = set.value("/Common/mapResolution").toDouble();
	m_mediaTransProtocol = set.value("/Common/mediaTransProtocol").toString();

    QString location = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString configPath = QString("%1/%2/%3").arg(location, CONFIG_DIR, CONFIG_FILE_NAME); 
    QFile fileConfig(configPath);
    if (fileConfig.exists())
    {
		QSettings setConfig(configPath, QSettings::IniFormat);
		for (int i = 1; i <= 6; ++i)
		{

			auto str = QString("/Speed/Speed%1").arg(i);
			if (setConfig.contains(str))
				m_carSpeedsConfig[i] = setConfig.value(str).toDouble();
		}

		if (setConfig.contains("/Common/showVideoModel"))
			m_showVideoModel = setConfig.value("/Common/showVideoModel").toBool();

		if (setConfig.contains("/Common/showCarSystemInfo"))
			m_showCarSystemInfo = setConfig.value("/Common/showCarSystemInfo").toBool();
    }
}

Config* Config::getInstance()
{
    if(m_instance)
        return m_instance.get();
    auto p = new Config();
    m_instance.reset(p);
    return p;
}

void Config::SetSpeeds(const QHash<int, double>& speeds)
{
	QString location = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	QString configPath = QString("%1/%2/%3").arg(location, CONFIG_DIR, CONFIG_FILE_NAME);
	QSettings setConfig(configPath, QSettings::IniFormat);
	for (auto it = speeds.begin(); it != speeds.end(); ++it)
	{
        auto str = QString("/Speed/Speed%1").arg(it.key());
		m_carSpeedsConfig[it.key()] = it.value();
        setConfig.setValue(str, it.value());
	}
}

void Config::SetVideoModelVisible(bool show)
{
	QString location = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	QString configPath = QString("%1/%2/%3").arg(location, CONFIG_DIR, CONFIG_FILE_NAME);
	QSettings setConfig(configPath, QSettings::IniFormat);
    m_showVideoModel = show;
    setConfig.setValue("/Common/showVideoModel", show);
}

void Config::SetShowCarSystemInfo(bool show)
{
	QString location = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	QString configPath = QString("%1/%2/%3").arg(location, CONFIG_DIR, CONFIG_FILE_NAME);
	QSettings setConfig(configPath, QSettings::IniFormat);
	m_showCarSystemInfo = show;
	setConfig.setValue("/Common/showCarSystemInfo", show);
}

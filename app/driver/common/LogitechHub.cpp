#include "LogitechHub.h"
#include "LogitechSteeringWheelLib.h"
#include "Common.h"
#include "mainwindow.h"
#include "Config.h"

#include <QTimer>

LogitechHub::LogitechHub(QObject* parent/* = nullptr*/) :
	QObject(parent),
	m_timer(new QTimer(parent))
{
	auto config = Config::getInstance();
	m_timer->setSingleShot(false);
	m_timer->setInterval(1000 / config->m_logitechUpdateRate);
	m_timer->callOnTimeout([=]
		{
			ControlData data;
			if(!GetControlData(data))
				emit UpdateFail();
			else
				emit ControlFire(data);
		});
}

LogitechHub::~LogitechHub()
{
	Stop();
}

bool LogitechHub::Start()
{
	auto mainWindow = GetFirstWindowPtr<MainWindow>();
	if (mainWindow)
	{
		HWND hwnd = (HWND)mainWindow->winId();
		if (LogiSteeringInitializeWithWindow(true, hwnd))
		{
			m_timer->start();
			return true;
		}
	}
	return false;
}

void LogitechHub::Stop()
{
	if (m_timer->isActive())
	{
		m_timer->stop();
		LogiSteeringShutdown();
	}
}

bool LogitechHub::GetControlData(ControlData& data)
{
	if (!LogiIsConnected(0))
		return false;

	if (!LogiUpdate())
	{
		return false;
	}
	else
	{
		auto deviceState = LogiGetState(0);
		if (!deviceState)
			return false;
		//XY轴设备最大值为32767，最小值为-32768，正值为向右或者加速,此处归一化处理
		//X轴(方向盘)的值
		if (deviceState->lX > 0)
		{
			data.angularRate = -(double)deviceState->lX / 32767;
			data.angularRate = max(data.angularRate, -1);
		}
		else
		{
			data.angularRate = -(double)deviceState->lX / 32768;
			data.angularRate = min(data.angularRate, 1);
		}
		//Y轴(油门)的值，油门松开状态下值为32767，踩到底为-32768
		double speed = -(double)(deviceState->lY - 32767) / 65535;
		//qDebug() << "deviceState->lY=" << deviceState->lY << "\n";
		data.lineRate = min(speed, 1);
		//查看档位(档位按钮为12-17)
		for (int gear = 12; gear <= 17; ++gear)
		{
			if (deviceState->rgbButtons[gear] & 0x80)
			{
				data.gear = gear - 11;
				break;
			}
		}
		if (data.gear == 6)
			data.lineRate = -data.lineRate;
		if (deviceState->rgbButtons[19] & 0x80)
		{
			data.jackUp = 1;
		}
		if (deviceState->rgbButtons[20] & 0x80)
		{
			data.jackDown = 1;
		}
		if (deviceState->rgbButtons[2] & 0x80)
		{
			data.switchDriveMode = 1;
		}
		if (deviceState->rgbButtons[23] & 0x80)
		{
			data.switchObstacle = 1;
		}
		//qDebug() << "LogitechHub::GetControlData:gear=" << data.gear <<
		//	",angularRate = " << data.angularRate << ",jackDown =" << data.jackDown
		//	<< ",jackUp" << data.jackUp << ",lineRate=" << data.lineRate << "\n";

		//LogiPlayLeds(0, speed, 0.1f, 1.0f);
		LogiPlaySpringForce(0, 0, 20 + abs(data.angularRate) * 30, 100);
		//LogiPlayDamperForce(0, 0);

		return true;		
	}
}

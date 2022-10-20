#pragma once

#include <QObject>

class QTimer;

struct ControlData
{
	double angularRate = 0;			//角速度，正值为左转，负值为右转
	double lineRate = 0;			//直线速度,正数前进，负数倒退(只有倒挡时取负数)
	int gear = 0;					//档位,支持范围1到6，第6档为倒挡(需要将lineRate设置为负数)
	int jackUp = 0;					//顶升,置1有效
	int jackDown = 0;				//下降,置1有效 
	int switchDriveMode = 0;		//切换驾驶模式,置1有效
	int switchObstacle = 0;			//切换避障模式,置1有效
};

/**
 * 罗技设备关键词说明:
 * X轴:指的方向盘的值
 * Y轴:指的油门的值
 * X-inut:支持Xbox模式的输入(一般忽略该输入)
 */

class LogitechHub : public QObject
{
	Q_OBJECT
public:
	LogitechHub(QObject* parent = nullptr);
	~LogitechHub();

	bool Start();
	void Stop();
	bool GetControlData(ControlData& data);

signals:
	void ControlFire(const ControlData&);
	void UpdateFail();

private:
	QTimer* m_timer;
};



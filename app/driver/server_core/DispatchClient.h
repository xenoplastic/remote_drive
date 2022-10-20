#pragma once

#include <QString>
#include <QVector>
#include <QHash>
#include <functional>
#include <memory>
#include <string>
#include <map>

#include "WebSocket.h"
#include "HttpClient.h"
#include "Defines.h"
#include "LogitechHub.h"
#include "CarData.h"

enum CAMERA_POS {
	LEFT_CAMERA			= 0x0001,
	RIGHT_CAMERA		= 0X0002,
	FRONT_CAMERA		= 0X0004,
	BACK_CAMERA			= 0X0008,
};

enum class CUR_CAR_CONTROL_STATE
{
	NO_CONTROL,
	APPLYING_CONTROL,
	CONTROLLING,
};

struct InfoCompare
{
	bool operator()(const std::string& l, const std::string& r)const;
};

struct DispatchCarInfo
{
	QString info;
	QString id;
	CarState state;
};
Q_DECLARE_METATYPE(DispatchCarInfo)

struct CarLocalData
{
	int netDelay = 0;
	double cpuPercent = 0.f;
	double mbMemoryTotal = 0.f;
	double mbMemoryFree = 0.f;
	uint64_t mbDiskTotal = 0;
	uint64_t mbDiskFree = 0;
	double kUploadNetSpeed = 0.f;
	double kDownNetSpeed = 0.f;
};

using VEC_STRS = QVector<QString>;
using VEC_DISPATCH_CARS = QVector<DispatchCarInfo>;
using MAP_ID_CARSINFO = std::multimap<std::string, std::string, InfoCompare>;
using HASH_STR_STR = QHash<std::string, std::string>;

using PULLCARSINFO_CALLBACK = std::function<void(
	const MAP_ID_CARSINFO&,
	const HASH_STR_STR&,
	HttpErrorCode
)>;
using HTTP_CALLBACK_WITH_ERRORCODE = std::function<void(
	ErrorCode, 
	HttpErrorCode
)>;
using PULLVIDEO_CALLBACK = std::function<void(
	ErrorCode,					//调度服务业务错误码
	HttpErrorCode,				//http请求错误码
	const VEC_STRS&,			//推流成功的摄像头
	const VEC_STRS&,			//推流失败的摄像头
	const QString&				//房间号 
)>;

class DispatchClient : public QObject
{
	Q_OBJECT
public:
	static DispatchClient* getInstance();

	~DispatchClient();

	void PullCarsInfo(PULLCARSINFO_CALLBACK callback);
	void PullVideo(
		const DispatchCarInfo& car,
		int cameraPos,
		PULLVIDEO_CALLBACK callback
	);
	void RePullVideo(int cameraPos);
	bool IsPullingVideo() const;
	bool StartLogitech();
	void StopLogitech();
	void ExitVideo(bool later = false);
	DispatchCarInfo GetCurCarInfo() const;

	void ApplyControlCar(const QString& carId);
	void ExitControlCar();
	void SetCarAutoDrive(const QString& carId);
	void SetCurCarAutoDrive();
	void SetRemoteConfig(const QHash<int, double>& carSpeedsConfig, const QString& carId = "");
	void SetObstacle(const QString& carId, bool enable);
	void SetCurObstacle(bool enable);
	void SetStopPushingVideo(const QString& carId);
	void SetStopPushingVideoLater(const QString& carId);

	void Response(
		Action act,
		const QString& carId, 
		const QString& driverId,
		ErrorCode errorCode
	);

	ControlData GetControlData() const;
	CUR_CAR_CONTROL_STATE GetCurCarControlState();
	void SetCurCarControlState(CUR_CAR_CONTROL_STATE curCarControlState);

signals:
	void Connected();
	void Disconnected();
	void ConnectTimeout();
	void ConnectFail();
	void CarOnline(const DispatchCarInfo& carInfo, const QString& nextCarId);
	void CarOff(const QString&);
	void CarApplyBeControlled(const QString&);
	//车端推流通知
	void PushVideoNotice(
		const QString&,		//车id
		const QString&,		//房间id
		const VEC_STRS&,	//推流成功的摄像头名
		const VEC_STRS&,	//推流失败的摄像头名
		int videoWidth,
		int videoHeight,
		int videoFPS
	);
	void DeviceDataUpdateFail();
	void CarDataChange(const QString& carId, const CarData& carData);
	void FinishRemoteConfig(qint64 status);
	void ControlDataIncoming(const ControlData& data);
	void NetDelayChange(qint64 netDely);
	void CarLocalDataUp(const QString& carId, const CarLocalData&);
	void DriverApplyControlCar(const QString& carId, const QString& driverId);
	void ApplyControlCarResult(ErrorCode errorCode);
	void ApplyingControlCar();
	void GetServerTimeFail();
	void CarUpdateProcess(const QString& carId, qint64 writeSize, qint64 totalSize);
	void CarUpdateFail(const QString& carId);
	void CarHealthChange(const DispatchCarInfo& carInfo, const QString& nextCarId);
	void SwitchDriveMode();
	void SwitchObstacle();
	
protected slots:
	void OnMessageRecieve(const WebSocketMessage& msg);
	void OnControlFire(const ControlData& data);
	void OnGetServerTimeFail();

protected:
	void HandleData(const QString& data);
	inline void SendDataToRos(const QString& carId, const QString& data);
	void SendControlData(const QString& carId, const ControlData& data);
	void SendCurControlData(const ControlData& data);
	inline void ConnectDispatchServer();
	std::string AddCar(const DispatchCarInfo& carInfo);
	void DeleteCar(const QString& carId);
	std::string UpdateCar(const DispatchCarInfo& carInfo);

private:
	DispatchClient(QObject* parent = nullptr);

private:
	HttpClient* m_httpClient;
	WebSocket* m_webSocket;
	LogitechHub* m_logitechHub;
	DispatchCarInfo m_curVideoCar;
	PULLVIDEO_CALLBACK m_curPullVideoCallback;
	CUR_CAR_CONTROL_STATE m_curCarControlState;
	int64_t m_applyingControlCarTimestamp;
	const int m_applyingControlCarTimeout = 10000;
	qint64 m_initServerTime;
	qint64 m_serverTimeOffset;
	MAP_ID_CARSINFO m_healthyCars;					//健康车辆id集合
	HASH_STR_STR m_faultedCars;						//故障车辆id集合
	ControlData m_controlData;	
	QVector<QString> m_carsOfExitVideoLater;

	static std::unique_ptr<DispatchClient> m_instance;
};

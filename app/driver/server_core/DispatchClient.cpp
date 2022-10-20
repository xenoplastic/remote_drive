#include "DispatchClient.h"
#include "Defines.h"
#include "Config.h"
#include "BaseFunc.h"

#include <QSettings>
#include <QCoreApplication>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>

bool InfoCompare::operator()(const std::string& l, const std::string& r)const
{
	auto findNumStartPosFun = [](const std::string& str)->size_t
	{
		size_t pos = -1;
		size_t len = str.length();
		if (len <= 0)
			return pos;
		for (size_t i = len - 1; i != 0; --i)
		{
			if ('0' <= str[i] && str[i] <= '9')
			{
				pos = i;
			}
			else
			{
				break;
			}
		}
		return pos;
	};

	size_t leftStrNumStartPos = findNumStartPosFun(l);
	size_t rightStrNumStartPos = findNumStartPosFun(r);
	std::string leftInfoLab = l.substr(0, leftStrNumStartPos);
	std::string rightInfoLab = r.substr(0, rightStrNumStartPos);
	if (leftStrNumStartPos <= 0 || rightStrNumStartPos <= 0 ||
		leftInfoLab != rightInfoLab)
	{
		return l < r;
	}

	int leftStrNum = std::stoi(l.substr(leftStrNumStartPos));
	int rightStrNum = std::stoi(r.substr(rightStrNumStartPos));
	return leftStrNum < rightStrNum;
}

std::unique_ptr<DispatchClient> DispatchClient::m_instance = nullptr;

DispatchClient* DispatchClient::getInstance()
{
	if (m_instance)
		return m_instance.get();
	auto p = new DispatchClient();
	m_instance.reset(p);
	return p;
}

DispatchClient::DispatchClient(QObject* parent/* = nullptr*/) : 
	QObject(parent),
	m_webSocket(new WebSocket(this)),
	m_httpClient(new HttpClient(this)),
	m_logitechHub(new LogitechHub(this))
{
	QString configPath = QString("%1/config.ini").arg(
		QCoreApplication::applicationDirPath());
	QSettings set(configPath, QSettings::IniFormat);

	//ConnectDispatchServer();

	connect(m_webSocket, &WebSocket::MessageRecieve, this, &DispatchClient::OnMessageRecieve);
	connect(m_logitechHub, &LogitechHub::UpdateFail, this, &DispatchClient::DeviceDataUpdateFail);
	connect(m_logitechHub, &LogitechHub::ControlFire, this, &DispatchClient::OnControlFire);
	connect(m_webSocket, &WebSocket::NetDelayChange, this, &DispatchClient::NetDelayChange);
	connect(this, &::DispatchClient::GetServerTimeFail, this, &DispatchClient::OnGetServerTimeFail,
		Qt::QueuedConnection);
}

DispatchClient::~DispatchClient()
{

}

void DispatchClient::PullCarsInfo(PULLCARSINFO_CALLBACK callback)
{
	m_healthyCars.clear();
	m_faultedCars.clear();

	QString url = QString("http://%1%2").arg(Config::getInstance()->m_dispatchAddress, 
		URL_DISPATCH_GET_CARS_INFO);
	m_httpClient->Get(url,
		[=](const QByteArray& data, HttpErrorCode resCode) {
			QJsonParseError jsonError;
			QJsonDocument doucment = QJsonDocument::fromJson(data, &jsonError);
			VEC_DISPATCH_CARS carsInfo;
			if (resCode == HttpErrorCode::Success && 
				jsonError.error == QJsonParseError::NoError && 
				doucment.isObject())
			{
				const QJsonObject obj = doucment.object();
				auto objCars = obj.find("cars");
				if (objCars != obj.end())
				{
					QJsonArray arrayCars = objCars.value().toArray();				
					for (auto arrayItCar = arrayCars.begin(); arrayItCar != arrayCars.end(); ++arrayItCar)
					{
						auto objCar = arrayItCar->toObject();
						auto carInfo = DispatchCarInfo{
								objCar.find("info")->toString(),
								objCar.find("id")->toString(),
								CarState(objCar.find("state")->toInt()),
						};						
						carsInfo.push_back(carInfo);

						AddCar(carInfo);
					}					
				}
			}
			callback(m_healthyCars, m_faultedCars, resCode);
			if(resCode == HttpErrorCode::Success)
				ConnectDispatchServer();
		}
	);
}

void DispatchClient::PullVideo(
	const DispatchCarInfo& car, 
	int cameraPos,
	PULLVIDEO_CALLBACK callback
)
{	
	m_curVideoCar = car;
	m_curPullVideoCallback = callback;

	QString cameras;
	if (cameraPos & LEFT_CAMERA)
		cameras += "\"left_camera\"";
	if (cameraPos & RIGHT_CAMERA)
	{
		if (!cameras.isEmpty())
			cameras += ",";
		cameras += "\"right_camera\"";
	}
	if (cameraPos & FRONT_CAMERA)
	{
		if (!cameras.isEmpty())
			cameras += ",";
		cameras += "\"front_camera\"";
	}
	if (cameraPos & BACK_CAMERA)
	{
		if (!cameras.isEmpty())
			cameras += ",";
		cameras += "\"back_camera\"";
	}

	QString url = QString("http://%1%2").arg(Config::getInstance()->m_dispatchAddress,
		URL_DISPATCH_PULL_VIDEO);
	QString data = QString("{\"car_id\":\"%1\",\"driver_id\":\"%2\",\"cameras\":[%3]}"
	).arg(
		car.id,
		Config::getInstance()->m_clientId,
		cameras
	);
	m_httpClient->Post(url, data,
		[callback](const QByteArray& data, HttpErrorCode resCode){
			VEC_STRS vecSuccessCam;
			VEC_STRS vecFailCam;
			QJsonParseError jsonError;
			QJsonDocument doucment = QJsonDocument::fromJson(data, &jsonError);
			ErrorCode dataErrCode = ErrorCode::ErrUnknow;
			QString roomId;
			if (resCode == HttpErrorCode::Success &&
				jsonError.error == QJsonParseError::NoError &&
				doucment.isObject())
			{
				const QJsonObject& obj = doucment.object();
				dataErrCode = (ErrorCode)obj.find("error_code")->toInt();
				auto jsonSuccessCams = obj.find("success_camera")->toArray();
				for (auto it : jsonSuccessCams)
				{
					vecSuccessCam.append(it.toString());
				}
				auto jsonFailCams = obj.find("fail_camera")->toArray();
				for (auto it : jsonFailCams)
				{
					vecFailCam.append(it.toString());
				}
				roomId = obj.find("room_id")->toString();
			}
			callback(dataErrCode, resCode, vecSuccessCam, vecFailCam, roomId);
		}
	);
}

void DispatchClient::RePullVideo(int cameraPos)
{
	QString cameras;
	if (cameraPos & LEFT_CAMERA)
		cameras += "\"left_camera\"";
	if (cameraPos & RIGHT_CAMERA)
	{
		if (!cameras.isEmpty())
			cameras += ",";
		cameras += "\"right_camera\"";
	}
	if (cameraPos & FRONT_CAMERA)
	{
		if (!cameras.isEmpty())
			cameras += ",";
		cameras += "\"front_camera\"";
	}
	if (cameraPos & BACK_CAMERA)
	{
		if (!cameras.isEmpty())
			cameras += ",";
		cameras += "\"back_camera\"";
	}

	QString url = QString("http://%1%2").arg(Config::getInstance()->m_dispatchAddress,
		URL_DISPATCH_PULL_VIDEO);
	QString data = QString("{\"car_id\":\"%1\",\"driver_id\":\"%2\",\"cameras\":[%3]}"
	).arg(
		m_curVideoCar.id,
		Config::getInstance()->m_clientId,
		cameras
	);
	m_httpClient->Post(url, data,
		[this](const QByteArray& data, HttpErrorCode resCode) {
			VEC_STRS vecSuccessCam;
			VEC_STRS vecFailCam;
			QJsonParseError jsonError;
			QJsonDocument doucment = QJsonDocument::fromJson(data, &jsonError);
			ErrorCode dataErrCode = ErrorCode::ErrUnknow;
			QString roomId;
			if (resCode == HttpErrorCode::Success &&
				jsonError.error == QJsonParseError::NoError &&
				doucment.isObject())
			{
				const QJsonObject& obj = doucment.object();
				dataErrCode = (ErrorCode)obj.find("error_code")->toInt();
				auto jsonSuccessCams = obj.find("success_camera")->toArray();
				for (auto it : jsonSuccessCams)
				{
					vecSuccessCam.append(it.toString());
				}
				auto jsonFailCams = obj.find("fail_camera")->toArray();
				for (auto it : jsonFailCams)
				{
					vecFailCam.append(it.toString());
				}
				roomId = obj.find("room_id")->toString();
			}
			m_curPullVideoCallback(dataErrCode, resCode, vecSuccessCam, vecFailCam, roomId);
		}
	);
}

bool DispatchClient::IsPullingVideo() const
{
	return !m_curVideoCar.id.isEmpty();
}

bool DispatchClient::StartLogitech()
{
	return m_logitechHub->Start();
}

void DispatchClient::StopLogitech()
{
	m_logitechHub->Stop();
}

void DispatchClient::ExitVideo(bool later/* = false*/)
{
	m_logitechHub->Stop();
	if (later)
	{
		SetStopPushingVideoLater(m_curVideoCar.id);
	}
	else
	{
		for (auto it : m_carsOfExitVideoLater)
		{
			SetStopPushingVideo(it);
		}
		SetStopPushingVideo(m_curVideoCar.id);
	}	
	m_curVideoCar.id.clear();
	SetCurCarAutoDrive();	
	m_curCarControlState = CUR_CAR_CONTROL_STATE::NO_CONTROL;
}

DispatchCarInfo DispatchClient::GetCurCarInfo() const
{
	return m_curVideoCar;
}

void DispatchClient::ApplyControlCar(const QString& carId)
{
	QString data = QString(
		"{\"action\":%1, \"car_id\":\"%2\", \"driver_id\":\"%3\"}"
	).
		arg(int(Action::ActApplyControlCar)).
		arg(carId).
		arg(Config::getInstance()->m_clientId);
	m_webSocket->sendTextMessage(data);
	m_curCarControlState = CUR_CAR_CONTROL_STATE::APPLYING_CONTROL;
	emit ApplyingControlCar();
	m_applyingControlCarTimestamp = GetMillTimestampSinceEpoch();
}

void DispatchClient::ExitControlCar()
{	
	SetCurCarAutoDrive();
	m_curCarControlState = CUR_CAR_CONTROL_STATE::NO_CONTROL;
}

void DispatchClient::SendControlData(const QString& carId, const ControlData& controlData)
{
	QString bufsStr;
	for (int i = 1; i <= 10; ++i)
	{
		bufsStr += (i == controlData.gear ? "1" : "0");
		bufsStr += ",";
	}
	bufsStr += (1 == controlData.jackUp ? "1" : "0");
	bufsStr += ",";
	bufsStr += (1 == controlData.jackDown ? "1" : "0");
	QString data = QString(
		"{"
			"\"type\":\"remote_control\","
			"\"axes\":[%1,%2,0,0,0,0],"
			"\"buttons\":[%3]"
		"}"
	).arg(controlData.angularRate).arg(controlData.lineRate).arg(bufsStr);
	qDebug() << "DispatchClient::SendControlData:" << data << "\n";
	SendDataToRos(carId, data);
}

void DispatchClient::SendCurControlData(const ControlData& data)
{
	SendControlData(m_curVideoCar.id, data);
}

void DispatchClient::ConnectDispatchServer()
{
	auto config = Config::getInstance();

	//首先获取服务器时间戳
	QString getTimeUrl = QString("http://%1%2?request_time=%3").
		arg(config->m_dispatchAddress).
		arg(URL_DISPATCH_GET_TIME).
		arg(QDateTime::currentMSecsSinceEpoch());

	m_httpClient->Get(getTimeUrl,
		[=](const QByteArray& data, HttpErrorCode resCode) {
			if (resCode != HttpErrorCode::Success)
			{
				emit GetServerTimeFail();
				return;
			}

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
				if (escapeTime >= config->m_dispatchGetTimeTimeout)
				{
					emit GetServerTimeFail();
					return;
				}

				m_initServerTime = serverTime + (m_serverTimeOffset - requestTime) / 2;					

				QString webSocketUrl = QString("ws://%1/ws?id=%2&info=%3&state=0&role=1").
					arg(config->m_dispatchAddress).
					arg(config->m_clientId).
					arg(config->m_driverInfo);
				m_webSocket->SetPingPongInterval(config->m_dispatchPingpongTime);
				m_webSocket->Connect(webSocketUrl);
			}		
		}
	);
}

std::string DispatchClient::AddCar(const DispatchCarInfo& carInfo)
{
	//返回该车辆插入的位置下一个车的id，如果为空，则该车为最后一个
	std::string nextCarId;
	auto carData = CarData::getInstance(carInfo.id);
	auto info = carInfo.info.toStdString();
	auto id = carInfo.id.toStdString();
	carData->m_carState = carInfo.state;
	carData->m_carInfo = carInfo.info;
	if (carInfo.state != CarState::Fault)
	{
		m_healthyCars.insert(std::pair<std::string, std::string>(info,id));
		auto it = m_healthyCars.upper_bound(info);
		if (it != m_healthyCars.end())
			nextCarId = it->second;
	}
	else
	{
		m_faultedCars[id] = info;		
		if (!m_healthyCars.empty())
		{
			auto it = m_healthyCars.begin();
			nextCarId = it->second;
		}
	}
	return nextCarId;
}

void DispatchClient::DeleteCar(const QString& carId)
{
	CarData::deleteInstance(carId);
	auto it = std::find_if(m_healthyCars.begin(), m_healthyCars.end(),
		[=](std::pair<std::string, std::string> element) ->bool
		{
			return element.second == carId.toStdString();
		}
	);
	if (it != m_healthyCars.end())
		m_healthyCars.erase(it);
	m_faultedCars.remove(carId.toStdString());
}

std::string DispatchClient::UpdateCar(const DispatchCarInfo& carInfo)
{
	//返回该车辆插入的位置下一个车的id，如果为空，则该车为最后一个
	//如果返回自身id，则表示位置不动	
	auto id = carInfo.id.toStdString();
	auto info = carInfo.info.toStdString();
	std::string nextCarId = id;
	auto it = std::find_if(m_healthyCars.begin(), m_healthyCars.end(),
		[=](std::pair<std::string, std::string> element) ->bool
		{
			return element.second == id;
		}
	);
	if (carInfo.state == CarState::Fault)
	{
		if (it != m_healthyCars.end())
		{
			m_healthyCars.erase(it);
		}
			
		if (!m_faultedCars.contains(id))
		{
			m_faultedCars[id] = info;
			if (!m_healthyCars.empty())
			{
				auto it = m_healthyCars.begin();
				nextCarId = it->second;
			}
			else
			{
				nextCarId.clear();
			}
		}		
	}
	else
	{
		if (it == m_healthyCars.end())
		{
			m_healthyCars.insert(std::pair<std::string, std::string>(info, id));
			auto it = m_healthyCars.upper_bound(info);
			if (it != m_healthyCars.end())
				nextCarId = it->second;
			else
				nextCarId.clear();
		}
		if (m_faultedCars.contains(id))
		{
			m_faultedCars.remove(id);
		}		
	}
	return nextCarId;
}

void DispatchClient::SetCarAutoDrive(const QString& carId)
{
	QString data = "{\"type\":\"autonomous\"}";
	SendDataToRos(carId, data);
}

void DispatchClient::SetCurCarAutoDrive()
{
	if(m_curCarControlState == CUR_CAR_CONTROL_STATE::CONTROLLING)
		SetCarAutoDrive(m_curVideoCar.id);
}

void DispatchClient::SetRemoteConfig(const QHash<int, double>& carSpeedsConfig, 
	const QString& carId/* = ""*/)
{
	const auto& speeds = carSpeedsConfig;
	QString data = QString(
		"{"
			"\"type\":\"set_remote_config\","
			"\"set_speed\":["
				"{"
					"\"gear\":1,"
					"\"max_speed\":%1"
				"},"
				"{"
					"\"gear\":2,"
					"\"max_speed\":%2"
				"},"
				"{"
					"\"gear\":3,"
					"\"max_speed\":%3"
				"},"
				"{"
					"\"gear\":4,"
					"\"max_speed\":%4"
				"},"
				"{"
					"\"gear\":5,"
					"\"max_speed\":%5"
				"},"
				"{"
					"\"gear\":6,"
					"\"max_speed\":%6"
				"}"
			"]"
		"}"
	).
		arg(speeds[1]).arg(speeds[2]).arg(speeds[3]).
		arg(speeds[4]).arg(speeds[5]).arg(-speeds[6]);

	SendDataToRos(carId, data);
}

void DispatchClient::SetObstacle(const QString& carId, bool enable)
{
	QString data = QString("{\"type\":\"set_obs_stop\", \"obs_stop\":%1}").
		arg((int)(enable ? ObstacleState::Open : ObstacleState::Close));
	SendDataToRos(carId, data);
}

void DispatchClient::SetCurObstacle(bool enable)
{
	if (m_curCarControlState == CUR_CAR_CONTROL_STATE::CONTROLLING)
		SetObstacle(m_curVideoCar.id, enable);
}

void DispatchClient::SetStopPushingVideo(const QString& carId)
{
	QString data = QString("{\"action\":%1,\"car_id\":\"%2\"}").
		arg((int)Action::ActExitPushVideo).
		arg(carId);
	m_webSocket->sendTextMessage(data);
}

void DispatchClient::SetStopPushingVideoLater(const QString& carId)
{
	m_carsOfExitVideoLater.push_back(carId);
}

void DispatchClient::Response(
	Action act,
	const QString& carId, 
	const QString& driverId,
	ErrorCode errorCode
)
{
	QString data = QString(
		"{\"action\":%1, \"car_id\":\"%2\","
		"\"driver_id\":\"%3\", \"error_code\":%4}"
	).
		arg((int)act).
		arg(carId).
		arg(driverId).
		arg((int)errorCode);
	m_webSocket->sendTextMessage(data);
}

ControlData DispatchClient::GetControlData() const
{
	return m_controlData;
}

CUR_CAR_CONTROL_STATE DispatchClient::GetCurCarControlState()
{
	return m_curCarControlState;
}

void DispatchClient::SetCurCarControlState(CUR_CAR_CONTROL_STATE curCarControlState)
{
	m_curCarControlState = curCarControlState;
}

void DispatchClient::OnMessageRecieve(const WebSocketMessage& msg)
{
	switch (msg.msgCode)
	{
	case WebSocketMsgCode::connectSuccess:
		qDebug() << "DispatchClient::OnMessageRecieve::Connected" << "\n";
		emit Connected();
		break;
	case WebSocketMsgCode::connectFail:
		qDebug() << "DispatchClient::OnMessageRecieve::connectFail" << "\n";
		emit ConnectFail();
	case WebSocketMsgCode::connectTimeout:
		qDebug() << "DispatchClient::OnMessageRecieve::connectTimeout" << "\n";
		emit ConnectTimeout();
		break;
	case WebSocketMsgCode::disconnected:
		qDebug() << "DispatchClient::OnMessageRecieve::disconnected" << "\n";
		emit Disconnected();
		break;
	case WebSocketMsgCode::dataRecieved:
	{
		QStringList list = msg.data.split("\n");
		for (auto data : list)
		{
			HandleData(data.toUtf8());
		}				
	}
		break;
	default:
		break;
	}
}

void DispatchClient::OnControlFire(const ControlData& data)
{	
	if (m_controlData.switchDriveMode == 1 && data.switchDriveMode == 0)
		emit SwitchDriveMode();
	if (m_controlData.switchObstacle == 1 && data.switchObstacle == 0)
		emit SwitchObstacle();
	m_controlData = data;

	auto applyEscapeTime = GetMillTimestampSinceEpoch() - m_applyingControlCarTimestamp;
	if ((m_curCarControlState == CUR_CAR_CONTROL_STATE::APPLYING_CONTROL &&
		applyEscapeTime > m_applyingControlCarTimeout))
	{
		//申请当前车控制权
		ApplyControlCar(m_curVideoCar.id);
	}
	else if (m_curCarControlState == CUR_CAR_CONTROL_STATE::CONTROLLING)
	{
		SendCurControlData(data);
	}

	emit ControlDataIncoming(data);
}

void DispatchClient::OnGetServerTimeFail()
{
	ConnectDispatchServer();
}

void DispatchClient::HandleData(const QString& data)
{
	QJsonParseError jsonError;
	QJsonDocument doucment = QJsonDocument::fromJson(data.toUtf8(), &jsonError);
	if (jsonError.error != QJsonParseError::NoError || !doucment.isObject())
		return;

	const QJsonObject& obj = doucment.object();
	auto act = (Action)obj.find("action")->toInt();
	switch (act)
	{
	case Action::ActCarOff:
	{
		auto carId = obj.find("car_id")->toString();
		emit CarOff(carId);
		DeleteCar(carId);
	}
	break;
	case Action::ActCarApplyBeControlled:
	{
		auto carId = obj.find("car_id")->toString();
		emit CarApplyBeControlled(carId);
	}
	break;
	case Action::ActPushVideoNotice:
	{
		auto carId = obj.find("car_id")->toString();
		auto roomId = obj.find("room_id")->toString();
		auto jsonSuccessCameras = obj.find("success_cameras")->toArray();
		auto jsonFailCameras = obj.find("fail_cameras")->toArray();
		auto videoWidth = obj.find("videoWidth")->toInt();
		auto videoHeight = obj.find("videoHeight")->toInt();
		auto videoFPS = obj.find("videoFPS")->toInt();
		
		VEC_STRS vecSuccessCamera;
		VEC_STRS vecFailCamera;
		for (auto jsonCamera : jsonSuccessCameras)
		{
			vecSuccessCamera.append(jsonCamera.toString());
		}
		for (auto jsonCamera : jsonFailCameras)
		{
			vecFailCamera.append(jsonCamera.toString());
		}
		emit PushVideoNotice(
			carId,
			roomId, 
			vecSuccessCamera, 
			vecFailCamera,
			videoWidth,
			videoHeight,
			videoFPS
		);
	}
	break;
	case Action::ActCarOnline:
	{
		DispatchCarInfo carInfo{
			obj.find("info")->toString(),
			obj.find("car_id")->toString(),
			(CarState)obj.find("state")->toInt()
		};
		auto nextCarId = AddCar(carInfo);
		emit CarOnline(carInfo, nextCarId.c_str());		
	}
	break;
	case Action::ActRosDataToDriver:
	case Action::ActRosDataToDrivers:
	{		
		auto carId = obj["car_id"].toString();
		auto dataVal = obj["data"].toObject();
		auto type = dataVal["type"].toString();
		if (type == "set_vehicle_state")
		{		
			auto carData = CarData::getInstance(carId);
			if (carData->m_carInfo.isEmpty())
				return;

			auto strData = QString(QJsonDocument(dataVal).
				toJson(QJsonDocument::Compact));
			auto errCarData = carData->UpdateData(strData);
			if (errCarData == ErrorCarData::ECD_DATA_CHANGE)
			{
				DispatchCarInfo carInfo{
						carData->m_carInfo,
						carId,
						carData->m_carState,
				};
				auto nextCarId = UpdateCar(carInfo);
				if (nextCarId != carId.toStdString())
				{
					emit CarHealthChange(carInfo, nextCarId.c_str());
				}
			}
			emit CarDataChange(carId, *carData);
		}
		//else if (type == "remote")
		//{

		//}
		//else if (type == "autonomous")
		//{

		//}
		else if (type == "set_remote_config")
		{
			auto msgVal = dataVal["message"];
			if (msgVal == QJsonValue::Undefined || !msgVal.isObject())
				return;
			const auto msgObj = msgVal.toObject();
			auto statusVal = msgObj["status"];
			if (statusVal == QJsonValue::Undefined || !statusVal.isDouble())
				return;
			auto status = statusVal.toInteger();
			emit FinishRemoteConfig(status);
		}
	}
	break;
	case Action::ActCarLocalDataUp:
	{
		CarLocalData carLocalDat;
		auto carId = obj["car_id"].toString();
		carLocalDat.netDelay = obj["netDelay"].toInt();
		carLocalDat.cpuPercent = obj["cpu_percent"].toDouble();
		carLocalDat.mbMemoryTotal = obj["mb_memory_total"].toDouble();
		carLocalDat.mbMemoryFree = obj["mb_memory_free"].toDouble();
		carLocalDat.mbDiskTotal = obj["mb_disk_total"].toInt();
		carLocalDat.mbDiskFree = obj["mb_disk_free"].toInt();
		carLocalDat.kUploadNetSpeed = obj["kb_upload_net_speed"].toDouble();
		carLocalDat.kDownNetSpeed = obj["kb_down_net_speed"].toDouble();
		emit CarLocalDataUp(carId, carLocalDat);
	}
	break;
	case Action::ActApplyControlCar:
	{
		auto carId = obj["car_id"].toString();
		auto driverId = obj["driver_id"].toString();
		if (m_curCarControlState == CUR_CAR_CONTROL_STATE::NO_CONTROL || 
			m_curVideoCar.id != carId)
		{
			Response(Action::ActApplyControlResult, carId, driverId, 
				ErrorCode::ErrDriverNoControlCar);
		}
		else
		{
			emit DriverApplyControlCar(carId, driverId);
		}
	}
	break;
	case Action::ActApplyControlResult:
	{
		auto carId = obj["car_id"].toString();
		auto driverId = obj["driver_id"].toString();
		auto errorCode = (ErrorCode)obj["error_code"].toInt();
		if(m_curCarControlState != CUR_CAR_CONTROL_STATE::APPLYING_CONTROL || 
			carId != m_curVideoCar.id)
			break;
		switch (errorCode)
		{
		case ErrorCode::NoError:
		{
			m_curCarControlState = CUR_CAR_CONTROL_STATE::CONTROLLING;
			SetRemoteConfig(Config::getInstance()->m_carSpeedsConfig, carId);
		}		
			break;
		case ErrorCode::ErrRefuseApplyingControl:
		case ErrorCode::ErrCarOffLine:
		default:
			m_curCarControlState = CUR_CAR_CONTROL_STATE::NO_CONTROL;
			break;
		}
		emit ApplyControlCarResult(errorCode);
	}
	break;
	case Action::ActCarUpdateProcess:
	{
		auto carId = obj["car_id"].toString();
		auto writtenSize = obj["written_size"].toInteger();
		auto totalSize = obj["total_size"].toInteger();		
		emit CarUpdateProcess(carId, writtenSize, totalSize);

		CarData::getInstance(carId)->m_carState = CarState::Updating;
	}
		break;
	case Action::ActCarUpdateFail:
	{
		auto carId = obj["car_id"].toString();
		emit CarUpdateFail(carId);
	}
		break;
	default:
		break;
	}
}

void DispatchClient::SendDataToRos(const QString& carId, const QString& data)
{
	auto globalTime = m_initServerTime + (QDateTime::currentMSecsSinceEpoch() - 
		m_serverTimeOffset);
	QString textMsg = QString(
		"{\"action\":%1,\"car_id\":\"%2\",\"data\":%3, \"global_time\":%4}").
		arg((int)Action::ActDriveDataToRos).
		arg(carId).
		arg(data).
		arg(globalTime);
	m_webSocket->sendTextMessage(textMsg);
}

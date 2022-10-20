#include "CarData.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <iostream>

using namespace std;

#define SetCarData(obj, carDataField, key, isDataChage, toDataType, dataType)   \
{                                                                 \
    auto val = obj[key];                                          \
    if(val != QJsonValue::Undefined)                              \
    {                                                             \
        auto data = (dataType)val.toDataType();                   \
        if(carDataField != data)                                  \
        {                                                         \
            carDataField = data;                                  \
            isDataChage = true;                                   \
        }                                                         \
    }                                                             \
}                                                                 \

std::unordered_map<QString, std::unique_ptr<CarData>> CarData::m_instances;

const QHash<CarState, QString> CarData::m_stateText{
		{CarState::AutoDrive, QStringLiteral("自动驾驶")},
		{CarState::RemoteDrive, QStringLiteral("远程驾驶")},
		{CarState::ManualDrive, QStringLiteral("手动驾驶")},
		{CarState::Ide, QStringLiteral("空闲")},
        {CarState::Fault, QStringLiteral("等待驾驶")},
		{CarState::Updating, QStringLiteral("更新中")},
		{CarState::UpdateFail, QStringLiteral("更新失败")},
};

const QHash<SensorState, QString> CarData::m_sensorText{
		{SensorState::Far, QStringLiteral("远障碍")},
		{SensorState::Forbidden, QStringLiteral("禁用")},
		{SensorState::Near, QStringLiteral("近障碍")},
		{SensorState::NoData, QStringLiteral("无数据")},
		{SensorState::NoObstacle, QStringLiteral("无障碍")},
		{SensorState::QuicklyStop, QStringLiteral("紧急停止")},
		{SensorState::TooMuchnearRemoteDrive, QStringLiteral("极近障碍")},
		{SensorState::TooNear, QStringLiteral("过近障碍")},
};

const QHash<RiseFallResult, QString> CarData::m_riseDownResult{
    {RiseFallResult::NoFire, QStringLiteral("未触发")},
    {RiseFallResult::Success, QStringLiteral("成功")},
    {RiseFallResult::Fail, QStringLiteral("失败")}
};

CarData::CarData()
{
    
}

CarData* CarData::getInstance(const QString& carId)
{
    if (m_instances.find(carId) == m_instances.end())
    {
        m_instances[carId].reset(new CarData());
    }
    return m_instances[carId].get();
}

void CarData::deleteInstance(const QString& carId)
{
    m_instances.erase(carId);
}

ErrorCarData CarData::UpdateData(const QString& data)
{
    QJsonParseError jsonError;
    QJsonDocument doucment = QJsonDocument::fromJson(data.toUtf8(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !doucment.isObject())
    {
        cout << "CarData::UpdateData:jsonError.error != QJsonParseError::NoError,data=" 
            << data.toStdString() << endl;
        return ErrorCarData::ECD_JSON_ERROR;
    }

    const QJsonObject& obj = doucment.object();
    auto type = obj["type"];
    if(type == QJsonValue::Undefined)
    {
        cout << "CarData::UpdateData:type == QJsonValue::Undefined,data=" 
            << data.toStdString() << endl;
        return ErrorCarData::ECD_NO_TYPE;
    }

    if(type.toString() != "set_vehicle_state")
    {
        cout << "CarData::UpdateData:type.toString() != \"set_vehicle_state\",data=" 
            << data.toStdString() << endl;
        return ErrorCarData::ECD_NOT_STATE_DATA;
    }

    bool isDataChage = false;
    SetCarData(obj, m_lineSpeed, "linear_speed", isDataChage, toDouble, double);    
	SetCarData(obj, m_lrimuAngular, "lrimu_angular", isDataChage, toDouble, double);
    SetCarData(obj, m_carState, "state", isDataChage, toInt, CarState);
    
    auto obstacleVal = obj["obstacle"];
    if (obstacleVal != QJsonValue::Undefined && obstacleVal.isObject())
    {
        auto obstacleObj = obstacleVal.toObject();
        if (!obstacleObj.isEmpty())
        {
			SetCarData(obstacleObj, m_downLaserState, "avoidance", isDataChage, toInt, SensorState);
			SetCarData(obstacleObj, m_upLaserState, "up", isDataChage, toInt, SensorState);
			SetCarData(obstacleObj, m_frontCameraState, "camera", isDataChage, toInt, SensorState);
        }
    }

    auto positionVal = obj["position"];
	if (positionVal != QJsonValue::Undefined && positionVal.isObject())
	{
		auto positionObj = positionVal.toObject();
		if (!positionObj.isEmpty())
		{
			SetCarData(positionObj, m_carX, "x", isDataChage, toDouble, double);
			SetCarData(positionObj, m_carY, "y", isDataChage, toDouble, double);
            SetCarData(positionObj, m_carYaw, "yaw", isDataChage, toDouble, double);
		}
	}

    SetCarData(obj, m_rise, "jack_up", isDataChage, toInt, RiseFallResult);
    SetCarData(obj, m_fall, "jack_down", isDataChage, toInt, RiseFallResult);
    SetCarData(obj, m_chargeLevel, "charge_level", isDataChage, toInt, int);
    SetCarData(obj, m_obsStop, "obs_stop", isDataChage, toInt, ObstacleState);

    //cout << "CarData::UpdateData:m_data=data,data=" << data.toStdString() << endl;
    m_data = data;

    return isDataChage ? ErrorCarData::ECD_DATA_CHANGE : ErrorCarData::ECD_DATA_NO_CHANGE;
}

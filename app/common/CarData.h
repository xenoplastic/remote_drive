#pragma once

#include "Defines.h"

#include <QString>
#include <memory>
#include <QHash>
#include <unordered_map>

enum class ErrorCarData
{
    ECD_DATA_NO_CHANGE,
    ECD_DATA_CHANGE,
    ECD_NOT_STATE_DATA,
    ECD_NO_TYPE,
    ECD_JSON_ERROR,
};

struct CarData
{
    static CarData* getInstance(const QString& carId);
    static void deleteInstance(const QString& carId);

    ErrorCarData UpdateData(const QString& data);

    double m_lineSpeed = 0;
    double m_lrimuAngular = 0;
    SensorState m_downLaserState;                   //下激光状态
    SensorState m_upLaserState;                     //上激光状态
    SensorState m_frontCameraState;                 //前摄像机状态
    CarState m_carState = CarState::Ide;            //1:远程驾驶状态 2:自动驾驶状态 3:人工驾驶状态 4:空闲
    RiseFallResult m_rise;                          //0:未触发顶升 1:顶升成功 2:顶升失败
    RiseFallResult m_fall;                          //0:未触发下降 1:下降成功 :下降失败
    int m_chargeLevel = 0;                          //当前电量
    ObstacleState m_obsStop;                        //开关避障(1:打开避障 2:关闭避障)
    double m_carX = 0;
    double m_carY = 0;
    double m_carYaw = 0;

    const static QHash<CarState, QString> m_stateText;
    const static QHash<SensorState, QString> m_sensorText;
    const static QHash<RiseFallResult, QString> m_riseDownResult;
    
    QString m_data;

    QString m_carInfo;

private:
    CarData();

    static std::unordered_map<QString, std::unique_ptr<CarData>> m_instances;
};

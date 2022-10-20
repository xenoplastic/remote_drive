#pragma once

#define URL_API_VERSION						"v1.0"

//dispatch服务api
#define URL_DISPATCH_PREFIX					"/api/" URL_API_VERSION "/dispatch"
#define URL_DISPATCH_GET_CARS_INFO			URL_DISPATCH_PREFIX"/get_cars_info"
#define URL_DISPATCH_PULL_VIDEO				URL_DISPATCH_PREFIX"/pull_video"
#define URL_DISPATCH_GET_TIME				URL_DISPATCH_PREFIX"/get_time"

//logistics服务api
#define URL_LOGISTICS_PREFIX				"/api/" URL_API_VERSION "/logistics"
#define URL_LOGISTICS_GET_CAR_VERSION		URL_LOGISTICS_PREFIX"/get_car_version"
#define URL_LOGISTICS_CAR_PACKAGE_URL       URL_LOGISTICS_PREFIX"/static/package/car/remote_drive_car.gz"
#define URL_LOGISTICS_GET_DRIVER_VERSION	URL_LOGISTICS_PREFIX"/get_driver_version"
#define URL_LOGISTICS_DRIVER_PACKAGE_URL    URL_LOGISTICS_PREFIX"/static/package/driver/RemoteDrive_Setup.exe"
#define URL_LOGISTICS_DRIVER_UPDATE_LOG		URL_LOGISTICS_PREFIX"/static/package/driver/update_log.html"

enum class ErrorCode
{
	ErrUnknow = -2,
	ErrDataInvalid = -1,			//请求的数据无效
	NoError,						//成功
	ErrInternal,					//服务器内部错误
	ErrSql,							//数据库操作错误
	ErrBusy,						//数据库繁忙
	ErrNoDriver,					//无驾驶员在线
	ErrCarIsControlled,				//叉车已经被控制或者被分配给某个驾驶端
	ErrCarOffLine,					//叉车不在线
	ErrCarNoExist,					//叉车不存在
	ErrDriverIdInvalid,				//驾驶员id无效
	ErrCarHaveBeState,				//叉车已经处于该状态
	ErrSomeCamFail,					//部分摄像头推流失败
	ErrRtspServerUnreach,			//RTSP服务器无法访问
	ErrRoomIdNoPushVideo,			//该房间号无法用于推流
	ErrRefuseApplyingControl, 		//拒绝驾驶端申请指定车辆控制权
	ErrDriverNoControlCar,			//该驾驶员没有控制该车
};

enum class Action
{
	ActPushVideo = 0, 				//指示车端开始推流
    ActCarApplyBeControlled,		//车申请被控制
	ActCarOff,						//车离线
	ActTransferData,				//中转数据
	ActPushVideoNotice,				//车段推流结果通知
	ActCameraExitNotice,			//指定的摄像头退出推流
    ActExitPushVideo,               //指示车端退出推流
	ActCarOnline,					//车上线
    ActDriveDataToRos,				//驾驶端发送数据到车端ROS系统
    ActRosDataToDrivers,            //车端ROS数据广播在线驾驶端
	ActRosDataToDriver,				//车端ROS数据发送到指定驾驶端
	ActNewDriverUp,       	 		//通知车端有新的驾驶端上线
	ActCarLocalDataUp, 				//车端本地程序数据上报到驾驶端
	ActApplyControlCar,				//驾驶端申请车控制权
	ActApplyControlResult,			//驾驶端申请车控制权结果
	ActCarUpdateProcess,			//车端更新进度
	ActCarUpdateFail, 				//车端更新失败
};

enum class CarState
{
    RemoteDrive = 1,
	AutoDrive,
    ManualDrive,
    Ide,
	Fault,

    Updating = 100,
	UpdateFail,
};

enum class SensorState
{
	NoData = -2,					//传感器无数据
	Forbidden,						//禁用传感器
	NoObstacle,						//无障碍
	Far,
	Near,
	TooNear,
	QuicklyStop,					//极度危险紧急停止
	TooMuchnearRemoteDrive,
};

enum class RiseFallResult
{
	NoFire = 0,						//未触发
	Success,						//成功
	Fail,							//失败
};

enum class DriverCmd
{
	AutoDriver,
	RemoteDriver,
	Stop,
};

enum class ObstacleState
{
	Open = 1,
	Close,
};

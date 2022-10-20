package common

//车端状态
const (
	RemoteDrive = 1 //远程驾驶
	AutoDriver  = 2 //自动驾驶
	ManualDrive = 3 //手动控制
	Idle        = 4 //空闲状态
)

//客户端类型
const (
	Car    = 0 //叉车
	Driver = 1 //驾驶员
)

//错误码
const (
	ErrDataInvalid           = -1 //请求的数据无效
	NoError                  = 0  //成功
	ErrInternal              = 1  //服务器内部错误
	ErrSql                   = 2  //数据库操作错误
	ErrBusy                  = 3  //数据库繁忙
	ErrNoDriver              = 4  //无驾驶员在线
	ErrCarIsControlled       = 5  //叉车已经被控制或者被分配给某个驾驶端
	ErrCarOffLine            = 6  //叉车不在线
	ErrCarNoExist            = 7  //叉车不存在
	ErrDriverIdInvalid       = 8  //驾驶员id无效
	ErrCarHaveBeState        = 9  //叉车已经处于该状态
	ErrSomeCamFail           = 10 //部分摄像头推流失败
	ErrRtspServerUnreach     = 11 //RTSP服务器无法访问
	ErrRoomIdNoPushVideo     = 12 //该房间号无法用于推流
	ErrRefuseApplyingControl = 13 //拒绝驾驶端申请指定车辆控制权
	ErrDriverNoControlCar    = 14 //该驾驶员没有控制该车
)

//action公共枚举(用于websocket)
const (
	ActPushVideo          = 0  //指示车端开始推流
	ActApplyBeControlled  = 1  //车辆请求被控制
	ActCarOffline         = 2  //车辆已经离线
	ActTransferData       = 3  //中转数据
	ActPushVideoNotice    = 4  //车端推流结果通知
	ActCameraExitNotice   = 5  //指定的摄像头退出推流
	ActExitPushVideo      = 6  //指示车端退出推流
	ActCarOnline          = 7  //车上线
	ActDriveDataToRos     = 8  //驾驶端发送数据到车端ROS系统
	ActRosDataToDrivers   = 9  //车端ROS数据广播在线驾驶端
	ActRosDataToDriver    = 10 //车端ROS数据发送到指定驾驶端
	ActNewDriverUp        = 11 //通知车端有新的驾驶端上线
	ActCarLocalDataUp     = 12 //车端本地程序数据上报到驾驶端
	ActApplyControlCar    = 13 //驾驶端申请车控制权
	ActApplyControlResult = 14 //驾驶端申请车控制权结果
	ActCarUpdateProcess   = 15 //车端更新进度
	ActCarUpdateFail      = 16 //车端更新失败
)

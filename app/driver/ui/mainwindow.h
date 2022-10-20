#pragma once

#include <QMainWindow>
#include <QHash>

#include "DispatchClient.h"

class MediaElement;
class QTimer;
class QLabel;
class QListWidgetItem;
class LeftVideoWindow;
class RightVideoWindow;
class SpeedSetWindow;
class UpdateProcessWindow;
struct CarData;
struct Config;
struct VideoData;

enum class CarControlMsg {
	RemoteControlling,
	NoControl,
	ApplyingControl,
	RefuseControl,
	CarOffLine,
	OpeningVideo,
    PlayingVideo,
	FrontCamPushVideoFail,
	MediaInitFail,
	MediaOpenFail,
	MediaReadFail,
	MediaError,
};

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* o, QEvent* e);

protected slots:
    void OnCarDataChange(const QString& carId, const CarData& carData);
    void OnDriverApplyControlCar(const QString& carId, const QString& driverId);

private:
    void ShowSpeedWindow();
    void AddCarItem(const DispatchCarInfo& car, const QString& nextCarId = "");
	void HandleCamera(const VEC_STRS& successCams, const VEC_STRS& failCams,
		const QString& roomId, VideoData* videoData = nullptr);
    void ExitVideo(bool later = false);
    void ConfigRiseFall(QLabel* lab, RiseFallResult res);
    inline void SetCarDataVisible(bool visible);
    void ResetUI();
    void SetCarControlUIVisible(bool visible);
    void UpdateCarData(const CarData& carData);
    void UpdateBackVideoPos();
    void SetCarMsg(CarControlMsg carControlMsg);
    inline void AdjustCarItem();
    inline void SetWarningBorderVisible(bool visible);
    void RequestCarsInfo();
    inline void SetReconnectUI(bool reconnect);
    void PutCarListIntoMainPage();

private:
    Ui::MainWindow *ui;

    QTimer* m_timerReopenVideo;
    int m_reopenCameraPos = 0;

    DispatchClient* m_dispatchClient;
    QHash<QString, QListWidgetItem*> m_carListItems;
    LeftVideoWindow* m_leftVideoWindow;
    RightVideoWindow* m_rightVideoWindow;
    Config* m_config;
    QWidget* m_shadowWidget = nullptr;
    MediaElement* m_backVideo = nullptr;
	bool m_isInitCarDataDisplay = false;
    SpeedSetWindow* m_speedSetWindow = nullptr;
    UpdateProcessWindow* m_updateProcessWindow = nullptr;
    
    //前摄像头视频中红色字体消息文本
	const QHash<CarControlMsg, QString> m_carControlMsg{
		{CarControlMsg::ApplyingControl, QStringLiteral("申请远程控制中")},
		{CarControlMsg::NoControl, QStringLiteral("未远程控制")},
		{CarControlMsg::RefuseControl, QStringLiteral("该车当前驾驶员拒绝退出遥控")},
		{CarControlMsg::RemoteControlling, QStringLiteral("远程控制中")},
		{CarControlMsg::CarOffLine, QStringLiteral("车已经离线")},
        {CarControlMsg::OpeningVideo, QStringLiteral("打开视频中")},
        {CarControlMsg::FrontCamPushVideoFail, QStringLiteral("车端打开前摄像头失败,重新连接中...")},
        {CarControlMsg::MediaOpenFail, QStringLiteral("视频打开失败,重新连接中...")},
		{CarControlMsg::MediaReadFail, QStringLiteral("视频读取失败,重新连接中...")},
        {CarControlMsg::MediaError, QStringLiteral("视频出错")},
        {CarControlMsg::MediaInitFail, QStringLiteral("视频初始化失败")},
	};
    CarControlMsg m_curCarMsgVideoState;
    CarControlMsg m_curCarMsgControlState;
    int m_carNetDelay = 0;
    qint64 m_driverNetDelay = 0;
};

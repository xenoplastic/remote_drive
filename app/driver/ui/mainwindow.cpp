#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "WebSocket.h"
#include "Config.h"
#include "CarData.h"
#include "qoregionlistitem.h"
#include "speedsetwindow.h"
#include "carinfoitem.h"
#include "leftvideowindow.h"
#include "rightvideowindow.h"
#include "messagewidget.h"
#include "CarMedia.h"
#include "updateprocesswindow.h"
#include "Common.h"

#include <QListWidgetItem>
#include <QTimer>
#include <QCheckBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
	, m_dispatchClient(DispatchClient::getInstance())
	, m_leftVideoWindow(new LeftVideoWindow(this))
	, m_rightVideoWindow(new RightVideoWindow(this))
	, m_timerReopenVideo(new QTimer(this))
	, m_backVideo(new MediaElement(this))
	, m_updateProcessWindow(new UpdateProcessWindow())
{
	ui->setupUi(this);
	QWidget::installEventFilter(this);
	m_timerReopenVideo->setSingleShot(true);
	m_timerReopenVideo->setInterval(1000);
	m_timerReopenVideo->callOnTimeout([=]()
		{
			if (m_reopenCameraPos)
				m_dispatchClient->RePullVideo(m_reopenCameraPos);
			m_reopenCameraPos = 0;
		});

	m_config = Config::getInstance();
    
	this->setWindowTitle(QStringLiteral("远程驾驶2.0"));
	showMaximized();

	m_shadowWidget = new QWidget(this, Qt::FramelessWindowHint);
	m_shadowWidget->setObjectName("widget");
	m_shadowWidget->setStyleSheet("#widget {background-color:rgba(10, 10, 10,100);}");
	
	m_speedSetWindow = new SpeedSetWindow(this);

	QImage img(":/images/set_normal.png");
	auto regionListRect = ui->regionList->geometry();
	auto item = new QListWidgetItem(ui->regionList);
	item->setSizeHint(QSize(regionListRect.width(), img.height()));
	
	auto regionWidget = new QOregionListItem(ui->regionList);
	regionWidget->setFixedSize(item->sizeHint());
	ui->regionList->setItemWidget(item, regionWidget);

	ui->carsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	ui->stackedWidget->setCurrentWidget(ui->carsListContainer);

	ConfigRiseFall(ui->riseLabel, RiseFallResult::NoFire);
	ConfigRiseFall(ui->fallLabel, RiseFallResult::NoFire);

	ui->backVideoPos->installEventFilter(this);

	m_updateProcessWindow->Update();

	SetWarningBorderVisible(false);

	ui->carDataContain->setVisible(false);
	ui->showCarRes->setCheckState(m_config->m_showCarSystemInfo ?
		Qt::Checked : Qt::Unchecked);

	ui->showModelCbx->setCheckState(m_config->m_showVideoModel ? 
		Qt::Checked : Qt::Unchecked);

	RequestCarsInfo();

	connect(m_dispatchClient, &DispatchClient::Connected, this, [this]
		{
			if (ui->stackedWidget->currentWidget() != ui->carsListContainer)
			{
				ui->stackedWidget->setCurrentWidget(ui->carsListContainer);
				PutCarListIntoMainPage();
			}
		}
	);

	connect(m_dispatchClient, &DispatchClient::Disconnected, this, [this]
		{	
			//重连:
			//	1、检测当前页面
			//	2、如果处于视频页面，则调用StopDrive
			//	3、显示网络重连页面
			if (ui->stackedWidget->currentWidget() == ui->mainVideoContainer)
			{
				ExitVideo();
			}

			SetReconnectUI(true);
		}
	);

	connect(m_dispatchClient, &DispatchClient::ConnectTimeout, this, [this]
		{
			SetReconnectUI(true);
		}
	);

	connect(m_dispatchClient, &DispatchClient::ConnectFail, this, [this]
		{
			SetReconnectUI(true);
		}
	);

	//重新打开:
	//	1、清除所有车列表项
	//	2、调用RequestCarsInfo
	//	3、置灰重连按钮，显示重连label
	//	4、如果重连成功，切换到车列表页面
	//	5、如果获取车列表错误，则enable重连按钮,显示网络错误label
	//	6、如果websocket连接失败，则enable重连按钮,显示网络错误label
	connect(ui->reconnectBtn, &QPushButton::clicked, this, [this] 
		{
			for (auto item : m_carListItems)
			{
				ui->carsList->itemWidget(item)->deleteLater();
				delete item;
			}
			m_carListItems.clear();
			RequestCarsInfo();

			SetReconnectUI(false);
		}
	);

	connect(m_dispatchClient, &DispatchClient::CarOnline, this, [this]
		(const DispatchCarInfo& car, const QString& nextCarId)
		{
			AddCarItem(car, nextCarId);
		}
	);

	connect(m_dispatchClient, &DispatchClient::CarHealthChange, this, [this]
	(const DispatchCarInfo& car, const QString& nextCarId)
		{
			auto item = m_carListItems.find(car.id);
			if (item == m_carListItems.end())
				return;
					
			auto RelocateCarItem = [=] {
				auto item = m_carListItems.take(car.id);
				ui->carsList->itemWidget(item)->deleteLater();
				delete item;
				AddCarItem(car, nextCarId);
			};
			if (nextCarId.isEmpty())
			{
				int row = ui->carsList->row(item.value());
				if (row != ui->carsList->count() - 1)
				{
					RelocateCarItem();
				}			
			}
			else
			{
				RelocateCarItem();
			}
		}
	);

	connect(m_dispatchClient, &DispatchClient::CarOff, this, [this](const QString& carId)
		{
			auto item = m_carListItems.take(carId);
			ui->carsList->itemWidget(item)->deleteLater();
			delete item;
			if (carId == m_dispatchClient->GetCurCarInfo().id)
			{
				SetCarMsg(CarControlMsg::CarOffLine);
				ExitVideo();
			}
		}
	);

	connect(m_dispatchClient, &DispatchClient::PushVideoNotice, this, [this](
		const QString& carId, const QString& roomId, const VEC_STRS& successCams, 
		const VEC_STRS& failCams, int videoWidth, int videoHeight, int videoFPS
		)
		{
			auto curCarInfo = m_dispatchClient->GetCurCarInfo();
			if (curCarInfo.id == carId)
			{
				VideoData* pVideoData = nullptr;
				if (videoWidth && videoHeight && videoFPS)
				{
					VideoData videoData;
					videoData.width = videoWidth;
					videoData.height = videoHeight;
					videoData.frameRate = videoFPS;
					videoData.pixFmt = AV_PIX_FMT_YUV420P;
					pVideoData = &videoData;
				}
				HandleCamera(successCams, failCams, roomId, pVideoData);
			}
		}
	);

	connect(m_dispatchClient, &DispatchClient::CarDataChange, this, 
		&MainWindow::OnCarDataChange);

	connect(regionWidget, &QOregionListItem::buttonClick, this, [this]
		{
			ShowSpeedWindow();
		}
	);

	connect(m_dispatchClient, &DispatchClient::ControlDataIncoming, this, [this]
	(const ControlData& data)
		{
			if (data.gear == 6)
			{
				//倒挡时交换前后摄像头
				if (ui->frontVideo->Render() != m_backVideo || 
					m_backVideo->Render() != ui->frontVideo)
				{
					ui->frontVideo->SwapRender(m_backVideo);
				}
			}
			else
			{
				if(ui->frontVideo->Render() != ui->frontVideo)
					ui->frontVideo->SetRender(ui->frontVideo);
				if (m_backVideo->Render() != m_backVideo)
					m_backVideo->SetRender(m_backVideo);
			}
			ui->gearLabel->setText(QString("%1").arg(data.gear));
		}
	);

	connect(m_dispatchClient, &DispatchClient::NetDelayChange, this, [this] 
	(qint64 netDely)
		{
			m_driverNetDelay = netDely;
			if (m_dispatchClient->GetCurCarControlState() == CUR_CAR_CONTROL_STATE::CONTROLLING)
			{
				if (m_carNetDelay >= m_config->m_netWarningDelay ||
					m_driverNetDelay >= m_config->m_netWarningDelay)
				{
					SetWarningBorderVisible(true);
				}
				else
				{
					SetWarningBorderVisible(false);
				}
			}
			ui->driverDelayLabel->setText(QString("%1ms").arg(netDely));
		}
	);

	connect(m_dispatchClient, &DispatchClient::CarUpdateProcess, this, [this]
	(const QString& carId, qint64 writeSize, qint64 totalSize)
		{
			if (!m_carListItems.contains(carId))
				return;

			QString text;
			if (totalSize > 0)
			{
				double percent = (double)writeSize / totalSize * 100;
				text = QString("%1(%2%)").
					arg(CarData::m_stateText[CarState::Updating]).
					arg((int)percent);
			}
			else
			{
				text = CarData::m_stateText[CarState::Updating];
			}
			auto item = m_carListItems[carId];
			//auto carInfo = item->data(Qt::UserRole).value<DispatchCarInfo>();
			//carInfo.state = CarState::Updating;
			//item->setData(Qt::UserRole, QVariant::fromValue(carInfo));

			auto carDataItem = static_cast<CarInfoItem*>(ui->carsList->itemWidget(item));
			carDataItem->SetStateText(text);
		}
	);

	connect(m_dispatchClient, &DispatchClient::CarUpdateFail, this, [this]
	(const QString& carId)
		{
			auto item = m_carListItems[carId];
			//auto carInfo = item->data(Qt::UserRole).value<DispatchCarInfo>();
			//carInfo.state = CarState::UpdateFail;
			//item->setData(Qt::UserRole, QVariant::fromValue(carInfo));

			auto carDataItem = static_cast<CarInfoItem*>(ui->carsList->itemWidget(item));
			carDataItem->SetState(CarState::UpdateFail);
		}
	);

	connect(m_dispatchClient, &DispatchClient::SwitchDriveMode, this, [this]
		{
			if (ui->startRemoteDriveBtn->isVisible())
				emit ui->startRemoteDriveBtn->clicked();
			else if (ui->exitRemoteDriveBtn->isVisible())
				emit ui->exitRemoteDriveBtn->clicked();
		}
	);

	connect(m_dispatchClient, &DispatchClient::SwitchObstacle, this, [this]
		{
			if(ui->obstacleSwitch->isVisible())
				emit ui->obstacleSwitch->clicked();
		}
	);

	connect(ui->exitRemoteDriveBtn, &QPushButton::clicked, this, [this]
		{
			m_dispatchClient->ExitControlCar();
			SetCarMsg(CarControlMsg::NoControl);
			SetCarControlUIVisible(false);
			ui->startRemoteDriveBtn->setVisible(true);
		}
	);

	connect(ui->startRemoteDriveBtn, &QPushButton::clicked, this, [this]
		{
			m_dispatchClient->ApplyControlCar(m_dispatchClient->GetCurCarInfo().id);
		}
	);

	connect(ui->closeVideoBtn, &QPushButton::clicked, this, [this]
		{
			ExitVideo();
			ui->stackedWidget->setCurrentWidget(ui->carsListContainer);			

			//车列表放到主窗口中
			PutCarListIntoMainPage();
		}
	);

	connect(ui->obstacleSwitch, &QPushButton::clicked, this, [this]
		{
			auto curCarInfo = m_dispatchClient->GetCurCarInfo();
			auto curCarData = CarData::getInstance(curCarInfo.id);
			m_dispatchClient->SetCurObstacle(
				curCarData->m_obsStop == ObstacleState::Open ? false : true);
		}
	);

	connect(m_speedSetWindow, &SpeedSetWindow::windowClose, this, [=]
		{
			m_shadowWidget->hide();
			m_speedSetWindow->hide();
		}
	);

	connect(m_dispatchClient, &DispatchClient::DriverApplyControlCar, 
		this, &MainWindow::OnDriverApplyControlCar);

	connect(m_dispatchClient, &DispatchClient::ApplyControlCarResult, this, [this]
	(ErrorCode errorCode)
		{
			//提示申请当前车控制结果
			switch (errorCode)
			{
			case ErrorCode::ErrRefuseApplyingControl:
				SetCarMsg(CarControlMsg::RefuseControl);
				break;
			case ErrorCode::NoError:
				SetCarControlUIVisible(true);
				ui->startRemoteDriveBtn->setVisible(false);
				SetCarMsg(CarControlMsg::RemoteControlling);
				break;
			case ErrorCode::ErrCarOffLine:
				SetCarMsg(CarControlMsg::CarOffLine);
				break;
			default:
				SetCarMsg(CarControlMsg::NoControl);
				break;
			}
		}
	);

	connect(m_dispatchClient, &DispatchClient::ApplyingControlCar, this, [this] 
		{
			SetCarMsg(CarControlMsg::ApplyingControl);
		}
	);

	connect(ui->frontVideo, &MediaElement::mediaOpenSuccess, this, [this]
		{		
			ui->frontVideo->ShowModel(m_config->m_showVideoModel);
			m_dispatchClient->StartLogitech();
			SetCarDataVisible(true);
			if (!ui->exitRemoteDriveBtn->isVisible())
			{
				SetCarControlUIVisible(false);
				ui->startRemoteDriveBtn->setVisible(true);
			}
			auto carData = CarData::getInstance(m_dispatchClient->GetCurCarInfo().id);
			if (!carData->m_data.isEmpty())
			{
				UpdateCarData(*carData);
			}
		}
	);

	connect(ui->frontVideo, &MediaElement::mediaMessage, this, [this]
		(MediaMessage mediaMsg, const std::string& mediaPath)
		{
			switch (mediaMsg)
			{
			case MediaMessage::InitFail:
				SetCarMsg(CarControlMsg::MediaInitFail);
				break;
			case MediaMessage::OpenFail:
				SetCarMsg(CarControlMsg::MediaOpenFail);	
				break;
			case MediaMessage::ReadFail:
				SetCarMsg(CarControlMsg::MediaReadFail);	
				break;
			default:
				SetCarMsg(CarControlMsg::MediaError);
				break;
			}

			if (mediaMsg == MediaMessage::OpenFail || mediaMsg == MediaMessage::ReadFail)
			{
				//ui->startRemoteDriveBtn->setVisible(false);
				//SetCarControlUIVisible(false);
				if (m_dispatchClient->IsPullingVideo() &&
					ui->frontVideo->GetMediaPath() == mediaPath)
				{
					if (m_timerReopenVideo->isActive())
					{
						m_reopenCameraPos |= FRONT_CAMERA;
					}
					else
					{
						m_reopenCameraPos = FRONT_CAMERA;
						m_timerReopenVideo->start();
					}				
				}
			}
		}
	);

	connect(m_leftVideoWindow->GetMediaElement(), &MediaElement::mediaMessage, this, [this]
	(MediaMessage mediaMsg, const std::string& mediaPath) 
		{
			if (mediaMsg == MediaMessage::OpenFail || mediaMsg == MediaMessage::ReadFail)
			{
				if (m_dispatchClient->IsPullingVideo() && m_leftVideoWindow->isVisible() &&
					m_leftVideoWindow->GetMediaElement()->GetMediaPath() == mediaPath)
				{
					if (m_timerReopenVideo->isActive())
					{
						m_reopenCameraPos |= LEFT_CAMERA;
					}
					else
					{
						m_reopenCameraPos = LEFT_CAMERA;
						m_timerReopenVideo->start();
					}
					m_leftVideoWindow->ShowReopenText();
				}
			}			
		}
	);

	connect(m_rightVideoWindow->GetMediaElement(), &MediaElement::mediaMessage, this, [this]
	(MediaMessage mediaMsg, const std::string& mediaPath)
		{
			if (mediaMsg == MediaMessage::OpenFail || mediaMsg == MediaMessage::ReadFail)
			{
				if (m_dispatchClient->IsPullingVideo() && m_rightVideoWindow->isVisible() &&
					m_rightVideoWindow->GetMediaElement()->GetMediaPath() == mediaPath)
				{
					if (m_timerReopenVideo->isActive())
					{
						m_reopenCameraPos |= RIGHT_CAMERA;
					}
					else
					{
						m_reopenCameraPos = RIGHT_CAMERA;
						m_timerReopenVideo->start();
					}
					m_rightVideoWindow->ShowReopenText();
				}
			}
		}
	);

	connect(m_backVideo, &MediaElement::mediaMessage, this, [this]
	(MediaMessage mediaMsg, const std::string& mediaPath)
		{
			if (mediaMsg == MediaMessage::OpenFail || mediaMsg == MediaMessage::ReadFail)
			{
				if (m_dispatchClient->IsPullingVideo() &&
					m_backVideo->GetMediaPath() == mediaPath)
				{
					if (m_timerReopenVideo->isActive())
					{
						m_reopenCameraPos |= BACK_CAMERA;
					}
					else
					{
						m_reopenCameraPos = BACK_CAMERA;
						m_timerReopenVideo->start();
					}
				}
			}
		}
	);

	connect(m_dispatchClient, &DispatchClient::CarLocalDataUp, this, [this]
	(const QString& carId, const CarLocalData& carLocalData)
		{
			if (m_dispatchClient->GetCurCarInfo().id == carId)
			{
				ui->cpuLabel->setText(QString("%1%").arg(QString::number(carLocalData.cpuPercent, 'f', 2)));
				ui->diskTotalLabel->setText(QString("%1MB").arg(carLocalData.mbDiskTotal));
				ui->diskFreeLabel->setText(QString("%1MB").arg(carLocalData.mbDiskFree));
				ui->memTotalLabel->setText(QString("%1MB").arg((uint64_t)carLocalData.mbMemoryTotal));
				ui->memFreeLabel->setText(QString("%1MB").arg((uint64_t)carLocalData.mbMemoryFree));
				ui->carDelayLabel->setText(QString("%1ms").arg(carLocalData.netDelay));
				m_carNetDelay = carLocalData.netDelay;
				ui->upNetSpeedLabel->setText(QString("%1KB").arg(QString::number(carLocalData.kUploadNetSpeed, 'f', 2)));
				ui->downNetSpeedLabel->setText(QString("%1KB").arg(QString::number(carLocalData.kDownNetSpeed, 'f', 2)));

				if (m_dispatchClient->GetCurCarControlState() == CUR_CAR_CONTROL_STATE::CONTROLLING)
				{
					auto& netWarningDelay = m_config->m_netWarningDelay;
					if (m_carNetDelay >= netWarningDelay || m_driverNetDelay >= netWarningDelay)
					{
						SetWarningBorderVisible(true);
					}
					else
					{
						SetWarningBorderVisible(false);
					}
				}
			}
		}
	);

	connect(ui->leftVideoBtn, &QPushButton::clicked, this, [this]
		{
			if (!m_leftVideoWindow->isVisible())
			{
				m_leftVideoWindow->show();
				m_leftVideoWindow->ReopenVideo();
			}
			else
			{
				m_leftVideoWindow->activateWindow();
			}
		}
	);
	connect(ui->rightVideoBtn, &QPushButton::clicked, this, [this]
		{
			if (!m_rightVideoWindow->isVisible())
			{
				m_rightVideoWindow->show();
				m_rightVideoWindow->ReopenVideo();
			}
			else
			{
				m_rightVideoWindow->activateWindow();
			}
		}
	);
	connect(m_rightVideoWindow, &RightVideoWindow::closeWindow, this, [this]
		{
			m_reopenCameraPos &= ~RIGHT_CAMERA;
		}
	);
	connect(m_leftVideoWindow, &LeftVideoWindow::closeWindow, this, [this]
		{
			m_reopenCameraPos &= ~LEFT_CAMERA;
		}
	);
	connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, [this](int)
		{
			auto size = ui->carsList->geometry();
			for (auto it : m_carListItems)
			{
				ui->carsList->itemWidget(it)->setFixedWidth(size.width());
			}

			if (ui->stackedWidget->currentWidget() == ui->mainVideoContainer)
			{				
				UpdateBackVideoPos();
				m_backVideo->raise();
				m_backVideo->show();
				m_backVideo->update();
			}
			else
			{
				m_backVideo->hide();
			}
		}
	);

	connect(ui->showModelCbx, &QCheckBox::stateChanged, this, [this] (int state)
		{
			bool visible = (state == Qt::Checked);
			ui->frontVideo->ShowModel(visible);
			m_config->SetVideoModelVisible(visible);
		}
	);

	connect(ui->showCarRes, &QCheckBox::stateChanged, this, [this](int state)
		{
			bool visible = (state == Qt::Checked);
			m_config->SetShowCarSystemInfo(visible);
			ui->carDataContain->setVisible(visible);			
		}
	);
}

MainWindow::~MainWindow()
{
	delete m_leftVideoWindow;
	delete m_rightVideoWindow;
	delete m_updateProcessWindow;
    delete ui;
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
	AdjustCarItem();

	if (m_shadowWidget)
	{
		m_shadowWidget->resize(this->size());

		if (m_speedSetWindow)
		{
			auto size = m_speedSetWindow->size();
			auto rc = this->geometry();
			m_speedSetWindow->move(
				rc.width() / 2 - size.width() / 2,
				rc.height() / 2 - size.height() / 2
			);
		}
	}
		
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	m_leftVideoWindow->close();
	m_rightVideoWindow->close();
}

bool MainWindow::eventFilter(QObject* target, QEvent* event)
{
	if (target == this)
	{
		if (event->type() == QEvent::WindowDeactivate)
		{
			qDebug() << "MainWindow::eventFilter:WindowDeactivate\n";
			if (m_dispatchClient->IsPullingVideo())
			{
				qDebug() << "MainWindow::eventFilter:IsPullingVideo\n";
				m_dispatchClient->StopLogitech();
			}
		}
		else if (event->type() == QEvent::WindowActivate)
		{
			qDebug() << "MainWindow::eventFilter:WindowActivate\n";
			if (m_dispatchClient->IsPullingVideo())
			{
				qDebug() << "MainWindow::eventFilter:IsPullingVideo\n";
				m_dispatchClient->StartLogitech();
			}
		}
	}
	else if (target == ui->backVideoPos)
	{
		if (event->type() == QEvent::Resize || event->type() == QEvent::Move)
		{
			UpdateBackVideoPos();
		}
	}

	return QMainWindow::eventFilter(target, event);
}

void MainWindow::OnCarDataChange(const QString& carId, const CarData& carData)
{
	if (m_dispatchClient->GetCurCarInfo().id == carId)
	{
		UpdateCarData(carData);
	}	

	if (m_carListItems.contains(carId))
	{
		auto item = m_carListItems[carId];
		//auto carInfo = item->data(Qt::UserRole).value<DispatchCarInfo>();
		//carInfo.state = carData.m_carState;
		//item->setData(Qt::UserRole, QVariant::fromValue(carInfo));

		auto carDataItem =  static_cast<CarInfoItem*>(ui->carsList->itemWidget(item));
		carDataItem->SetState(carData.m_carState);
	}
}

void MainWindow::OnDriverApplyControlCar(const QString& carId, const QString& driverId)
{
	//auto item = m_carListItems[carId];
	//auto carInfo = item->data(Qt::UserRole).value<DispatchCarInfo>();
	auto carData = CarData::getInstance(carId);
	QString text = QString(QStringLiteral("%1申请被控制")).arg(carData->m_carInfo);
	MessageWidget* messageWidget = new MessageWidget(
		text, 
		[=](bool ok)
		{
			if (m_dispatchClient->GetCurCarInfo().id != carId ||
				m_dispatchClient->GetCurCarControlState() != CUR_CAR_CONTROL_STATE::CONTROLLING)
			{
				m_dispatchClient->Response(
					Action::ActApplyControlResult,
					carId, driverId,
					ErrorCode::ErrDriverNoControlCar
				);
			}
			else
			{
				if (ok)
				{
					SetCarControlUIVisible(false);
					ui->startRemoteDriveBtn->setVisible(true);
					m_dispatchClient->SetCurCarControlState(CUR_CAR_CONTROL_STATE::NO_CONTROL);
					m_dispatchClient->Response(
						Action::ActApplyControlResult,
						carId, driverId,
						ErrorCode::NoError
					);
					SetCarMsg(CarControlMsg::NoControl);
				}
				else
				{
					m_dispatchClient->Response(
						Action::ActApplyControlResult,
						carId, driverId,
						ErrorCode::ErrRefuseApplyingControl
					);
				}
			}
		},
		this
	);
	auto rectMain = this->geometry();
	messageWidget->move(rectMain.width() - messageWidget->width(), 
		rectMain.height() - messageWidget->height());
	messageWidget->show();
}

void MainWindow::ShowSpeedWindow()
{
	m_shadowWidget->resize(this->size());
	m_shadowWidget->show();

	auto size = m_speedSetWindow->size();
	auto rc = this->geometry();
	m_speedSetWindow->move(
		rc.width() / 2 - size.width() / 2,
		rc.height() / 2 - size.height() / 2
	);
	m_speedSetWindow->show();
}

void MainWindow::AddCarItem(const DispatchCarInfo& car, const QString& nextCarId/* = ""*/)
{
	QImage img(":/images/open_video_normal.png");
	auto carsListRect = ui->carsList->geometry();
	auto itemSize = QSize(carsListRect.width(), img.height() + 10);

	QListWidgetItem* item = new QListWidgetItem();
	if (nextCarId.isEmpty())
	{
		ui->carsList->addItem(item);
	}
	else
	{
		auto nextItem = m_carListItems.find(nextCarId);
		if (nextItem == m_carListItems.end())
		{
			ui->carsList->addItem(item);
		}
		else
		{
			int row = ui->carsList->row(nextItem.value());
			ui->carsList->insertItem(row, item);
		}
	}
	item->setSizeHint(itemSize);

	auto carItem = new CarInfoItem(ui->carsList);
	carItem->setFixedHeight(itemSize.height());
	carItem->setFixedWidth(itemSize.width());
	carItem->SetInfo(car.info);
	carItem->SetState(car.state);
	ui->carsList->setItemWidget(item, carItem);
	carItem->EnableOpenVideoButton(car.state != CarState::Updating);
	
	m_carListItems[car.id] = item;

	connect(carItem, &CarInfoItem::openVideo, this, [car, this]
		{
			//如果打开的是当前已经在看的车则什么也不做
			if (ui->stackedWidget->currentWidget() == ui->mainVideoContainer && 
				m_dispatchClient->GetCurCarInfo().id == car.id)
				return;

			ResetUI();
			ui->carInfoLabel->setText(car.info);

			//如果已经处于视频画面,则退出之前的视频，再打开新的画面
			if (ui->stackedWidget->currentWidget() == ui->mainVideoContainer)
			{		
				ExitVideo(true);
			}
			else
			{
				ui->stackedWidget->setCurrentWidget(ui->mainVideoContainer);
				m_reopenCameraPos = 0;
			}								

			m_dispatchClient->PullVideo(
				car,
				LEFT_CAMERA | RIGHT_CAMERA | FRONT_CAMERA | BACK_CAMERA,
				[=](
					ErrorCode dataErr,
					HttpErrorCode httpErr,
					const VEC_STRS& successCams,
					const VEC_STRS& failCams,
					const QString& roomId
					)
				{
					auto curCarInfo = m_dispatchClient->GetCurCarInfo();
					if (curCarInfo.id == car.id)
					{
						if (dataErr == ErrorCode::ErrCarOffLine)
						{
							SetCarMsg(CarControlMsg::CarOffLine);
							return;
						}
						HandleCamera(successCams, failCams, roomId);
					}
				}
			);
		}
	);
}

void MainWindow::HandleCamera(const VEC_STRS& successCams, const VEC_STRS& failCams,
	const QString& roomId, VideoData* videoData/* = nullptr*/)
{
	bool isActive = this->isActiveWindow();
	if (successCams.contains("front_camera"))
	{
		QString mediaUrl = GetPullMediaUrl(roomId, "front_camera", m_config->m_mediaAddr,
			m_config->m_mediaTransProtocol);
		m_curCarMsgVideoState = CarControlMsg::PlayingVideo;
		SetCarMsg(m_curCarMsgControlState);
		ui->frontVideo->StartMedia(
			mediaUrl, 
			m_config->m_pullVideoProtocol, 
			m_config->m_openVideoTimeout,
			m_config->m_readVideoTimeout,
			videoData
		);
	}

	for (auto camName : successCams)
	{
		QString mediaUrl = GetPullMediaUrl(roomId, camName, m_config->m_mediaAddr,
			m_config->m_mediaTransProtocol);
		qDebug() << "ShowCamera:rtspPath" << mediaUrl << "\n";
		if (camName == "left_camera")
		{
			m_leftVideoWindow->show();
			m_leftVideoWindow->OpenVideo(mediaUrl, videoData);
			ui->leftVideoBtn->show();
		}
		else if (camName == "right_camera")
		{
			m_rightVideoWindow->show();
			m_rightVideoWindow->OpenVideo(mediaUrl, videoData);
			ui->rightVideoBtn->show();		

			//车列表放到右摄像头窗口中
			auto rightContainer = m_rightVideoWindow->GetRightContainer();			
			if (!rightContainer->children().contains(ui->carsList))
			{	
				auto rightContainerLayout = rightContainer->layout();
				ui->carsList->setMaximumWidth(rightContainer->width());
				rightContainerLayout->addWidget(ui->carsList);				

				AdjustCarItem();
			}		
		}
		else if (camName == "back_camera")
		{
			m_backVideo->StartMedia(
				mediaUrl,
				m_config->m_pullVideoProtocol,
				m_config->m_openVideoTimeout,
				m_config->m_readVideoTimeout,
				videoData
			);
		}
	}
	if (failCams.contains("front_camera"))
	{
		SetCarMsg(CarControlMsg::FrontCamPushVideoFail);
	}
	if (!failCams.isEmpty())
	{
		if (failCams.contains("front_camera"))
			m_reopenCameraPos |= FRONT_CAMERA;
		if (failCams.contains("back_camera"))
			m_reopenCameraPos |= BACK_CAMERA;
		if (failCams.contains("left_camera"))
			m_reopenCameraPos |= LEFT_CAMERA;
		if (failCams.contains("right_camera"))
			m_reopenCameraPos |= RIGHT_CAMERA;
		if (m_reopenCameraPos && !m_timerReopenVideo->isActive())
			m_timerReopenVideo->start();
	}
	if (isActive)
		this->activateWindow();
}

void MainWindow::ExitVideo(bool later/* = false*/)
{
	m_timerReopenVideo->stop();
	m_dispatchClient->ExitVideo(later);

	m_leftVideoWindow->StopVideo();
	m_leftVideoWindow->hide();	
	ui->frontVideo->StopMedia();
	m_backVideo->StopMedia();
	m_rightVideoWindow->StopVideo();
	m_rightVideoWindow->hide();
	m_reopenCameraPos = 0;
}

void MainWindow::ConfigRiseFall(QLabel* lab, RiseFallResult res)
{
	lab->setText(CarData::m_riseDownResult[res]);
	switch (res)
	{
	case RiseFallResult::NoFire:
		lab->setProperty("stateColor", "blue");
		break;
	case RiseFallResult::Success:
		lab->setProperty("stateColor", "green");
		break;
	case RiseFallResult::Fail:
		lab->setProperty("stateColor", "red");
		break;
	}
	lab->style()->unpolish(lab);
	lab->style()->polish(lab);
}

inline void MainWindow::SetCarDataVisible(bool visible)
{
	auto children = ui->carDataLeftTopContainer->children();
	for (auto child : children)
	{
		((QWidget*)child)->setVisible(visible);
	}
	children = ui->carDataRightTopContainer->children();
	for (auto child : children)
	{
		((QWidget*)child)->setVisible(visible);
	}
	children = ui->frontVideoBottomContainer->children();
	for (auto child : children)
	{
		((QWidget*)child)->setVisible(visible);
	}
	if (m_config->m_showCarSystemInfo)
	{
		ui->carDataContain->setVisible(visible);
	}
}

void MainWindow::ResetUI()
{
	ui->leftVideoBtn->setVisible(false);
	ui->rightVideoBtn->setVisible(false);
	ui->startRemoteDriveBtn->setVisible(false);
	ui->exitRemoteDriveBtn->setVisible(false);
	ui->obstacleSwitch->setVisible(false);
	ui->batteryLabel->setText("*");
	ui->carStateLabel->setText("*");
	ui->speedLabel->setText("*");
	ui->carDelayLabel->setText("*");
	ui->driverDelayLabel->setText("*");
	ui->gearLabel->setText("*");
	ui->frontCamLabel->setText("*");
	ui->upLaserLabel->setText("*");

	ui->cpuLabel->setText("*");	
	ui->diskTotalLabel->setText("*");
	ui->diskFreeLabel->setText("*");
	ui->upNetSpeedLabel->setText("*");
	ui->downNetSpeedLabel->setText("*");
	ui->memTotalLabel->setText("*");
	ui->memFreeLabel->setText("*");

	ui->downLaserLabel->setText("*");
	ui->obstacleLabel->setText("*");
	ui->riseLabel->setText("*");
	ui->fallLabel->setText("*");

	ui->frontVideo->ShowModel(false);
	ui->frontVideo->SetSteeringWheel(0.f);

	SetCarMsg(CarControlMsg::OpeningVideo);
	m_curCarMsgControlState = CarControlMsg::NoControl;
	SetCarControlUIVisible(false);
	SetCarDataVisible(false);

	ConfigRiseFall(ui->riseLabel, RiseFallResult::NoFire);
	ConfigRiseFall(ui->fallLabel, RiseFallResult::NoFire);

	SetWarningBorderVisible(false);

	SetWidgetCssProperty(ui->carStateLabel, "stateColor", "white");
	SetWidgetCssProperty(ui->frontCamLabel, "stateColor", "white");
	SetWidgetCssProperty(ui->obstacleLabel, "stateColor", "white");
	SetWidgetCssProperty(ui->upLaserLabel, "stateColor", "white");
	SetWidgetCssProperty(ui->downLaserLabel, "stateColor", "white");
}

void MainWindow::SetCarControlUIVisible(bool visible)
{
	ui->obstacleSwitch->setVisible(visible);
	ui->exitRemoteDriveBtn->setVisible(visible);
}

void MainWindow::UpdateCarData(const CarData& carData)
{
	ui->batteryLabel->setText(QString("%1%").arg(carData.m_chargeLevel));
	ui->carStateLabel->setText(
		carData.m_stateText[carData.m_carState]
	);

	auto controlData = m_dispatchClient->GetControlData();
	ui->frontVideo->SetSteeringWheel(carData.m_lrimuAngular / 
		(controlData.gear != 6 ? -90.f : 90.f));
	ui->speedLabel->setText(QString("%1m/s").arg(carData.m_lineSpeed));
	ui->upLaserLabel->setText(carData.m_sensorText[carData.m_upLaserState]);
	ui->downLaserLabel->setText(carData.m_sensorText[carData.m_downLaserState]);
	ui->frontCamLabel->setText(carData.m_sensorText[carData.m_frontCameraState]);
	ui->obstacleLabel->setText(QString(carData.m_obsStop == ObstacleState::Open ?
		QStringLiteral("打开") : QStringLiteral("关闭")
	));

	SetWidgetCssProperty(ui->obstacleLabel, "stateColor",
		carData.m_obsStop == ObstacleState::Open ? "green" : "yellow");

	auto configSensorColor = [this](SensorState sensorState, QWidget* widget)
	{
		switch (sensorState)
		{
		case SensorState::NoData:
		case SensorState::Forbidden:
			SetWidgetCssProperty(widget, "stateColor", "red");
			break;
		case SensorState::NoObstacle:						//无障碍
			SetWidgetCssProperty(widget, "stateColor", "green");
			break;
		case SensorState::Far:
		case SensorState::Near:
		case SensorState::TooNear:
		case SensorState::QuicklyStop:					//极度危险紧急停止
		case SensorState::TooMuchnearRemoteDrive:
			SetWidgetCssProperty(widget, "stateColor", "yellow");
			break;
		}
	};

	configSensorColor(carData.m_frontCameraState, ui->frontCamLabel);
	configSensorColor(carData.m_upLaserState, ui->upLaserLabel);
	configSensorColor(carData.m_downLaserState, ui->downLaserLabel);

	switch (carData.m_carState)
	{
	case CarState::AutoDrive:
		SetWidgetCssProperty(ui->carStateLabel, "stateColor", "green");
		break;
	case CarState::Fault:
	case CarState::ManualDrive:
		SetWidgetCssProperty(ui->carStateLabel, "stateColor", "red");
		break;
	case CarState::RemoteDrive:
		SetWidgetCssProperty(ui->carStateLabel, "stateColor", "yellow");
		break;
	}
	
	ConfigRiseFall(ui->riseLabel, carData.m_rise);
	ConfigRiseFall(ui->fallLabel, carData.m_fall);

	if (m_rightVideoWindow->isVisible())
	{
		m_rightVideoWindow->SetCarPos(carData.m_carX, carData.m_carY);
	}
}

void MainWindow::UpdateBackVideoPos()
{
	auto pos = ui->backVideoPos->mapTo(this, QPoint(0, 0));
	m_backVideo->move(pos);
	m_backVideo->resize(ui->backVideoPos->size());
}

void MainWindow::SetCarMsg(CarControlMsg carControlMsg)
{
	if (carControlMsg == CarControlMsg::CarOffLine)
	{
		ui->carControlMsgLabel->setText(m_carControlMsg[carControlMsg]);
		return;
	}

	switch (carControlMsg)
	{
	case CarControlMsg::RemoteControlling:
	case CarControlMsg::NoControl:
	case CarControlMsg::ApplyingControl:
	case CarControlMsg::RefuseControl:
		m_curCarMsgControlState = carControlMsg;
		if (m_curCarMsgVideoState == CarControlMsg::PlayingVideo)
			ui->carControlMsgLabel->setText(m_carControlMsg[carControlMsg]);
		break;
	case CarControlMsg::OpeningVideo:
	case CarControlMsg::PlayingVideo:
	case CarControlMsg::FrontCamPushVideoFail:
	case CarControlMsg::MediaInitFail:
	case CarControlMsg::MediaOpenFail:
	case CarControlMsg::MediaReadFail:
	case CarControlMsg::MediaError:
		m_curCarMsgVideoState = carControlMsg;
		ui->carControlMsgLabel->setText(m_carControlMsg[carControlMsg]);
		break;
	}
}

void MainWindow::AdjustCarItem()
{
	auto size = ui->carsList->geometry();
	for (auto it : m_carListItems)
	{
		ui->carsList->itemWidget(it)->setFixedWidth(size.width());
	}
}

void MainWindow::SetWarningBorderVisible(bool visible)
{
	qDebug() << "MainWindow::SetWarningBorderVisible:visible=" << visible << "\n";
	SetWidgetCssProperty(ui->frontVideoClientContainer, "borderColor", 
		visible ? "red" : "none");
	//ui->frontVideoClientContainer->setProperty(
	//	"borderColor", visible ? "red" : "none");
	//ui->frontVideoClientContainer->style()->unpolish(
	//	ui->frontVideoClientContainer);
	//ui->frontVideoClientContainer->style()->polish(
	//	ui->frontVideoClientContainer);
}

void MainWindow::RequestCarsInfo()
{
	m_dispatchClient->PullCarsInfo([=](const MAP_ID_CARSINFO& healthyCars,
		const HASH_STR_STR& faultedCars, HttpErrorCode httpErrorCode)
		{
			if (httpErrorCode != HttpErrorCode::Success)
			{
				SetReconnectUI(true);
				return;
			}

			for (auto it = faultedCars.begin(); it != faultedCars.end(); ++it)
			{
				DispatchCarInfo carInfo;
				carInfo.id = it.key().c_str();
				carInfo.info = it.value().c_str();
				carInfo.state = CarData::getInstance(carInfo.id)->m_carState;
				AddCarItem(carInfo);
			}
			for (auto& it : healthyCars)
			{
				DispatchCarInfo carInfo;
				carInfo.id = it.second.c_str();;
				carInfo.info = it.first.c_str();
				carInfo.state = CarData::getInstance(carInfo.id)->m_carState;
				AddCarItem(carInfo);
			}
		}
	);
}

void MainWindow::SetReconnectUI(bool reconnect)
{
	ui->reconnectBtn->setEnabled(reconnect);
	ui->netErrLabel->setVisible(reconnect);
	ui->netReconnectLabel->setVisible(!reconnect);
	if(reconnect)
		ui->stackedWidget->setCurrentWidget(ui->netErrorContainer);
}

void MainWindow::PutCarListIntoMainPage()
{
	if (ui->carsListContainer->children().contains(ui->carsList))
		return;

	auto layout = ui->carsListContainer->layout();
	int chilidTotalWidth = 0;
	auto carsListContainerChilder = ui->carsListContainer->children();
	auto margins = layout->contentsMargins();
	auto space = layout->spacing();
	for (auto child : carsListContainerChilder)
	{
		auto widget = dynamic_cast<QWidget*>(child);
		if (widget)
		{
			chilidTotalWidth += space;
			chilidTotalWidth += widget->geometry().width();
		}
	}
	chilidTotalWidth += margins.left();
	chilidTotalWidth += margins.right();

	layout->addWidget(ui->carsList);

	ui->carsList->setMaximumWidth(QWIDGETSIZE_MAX);

	auto carListWidth = ui->carsListContainer->width() - chilidTotalWidth;
	for (auto it : m_carListItems)
	{
		ui->carsList->itemWidget(it)->setFixedWidth(carListWidth);
	}	
}
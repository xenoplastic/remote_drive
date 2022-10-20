#include "updateprocesswindow.h"
#include "ui_updateprocesswindow.h"
#include "UpdateDetector.h"
#include "mainwindow.h"
#include "common.h"
#include "Config.h"
#include "BaseFunc.h"
#include "res/version.h"
#include "DriverDefines.h"

#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QDesktopServices>

UpdateProcessWindow::UpdateProcessWindow(QWidget *parent) :
    QDialog(parent),
    m_updateDetector(new UpdateDetector(this)),
    ui(new Ui::UpdateProcessWindow)
{
    ui->setupUi(this);
	ui->stackedWidget->setCurrentWidget(ui->promptPage);

	connect(ui->btnUpdate, &QPushButton::clicked, this, [this]
		{			
            ui->stackedWidget->setCurrentWidget(ui->updatePage);

			auto mainWindow = GetFirstWindowPtr<MainWindow>();
			mainWindow->hide();

			//开始下载
			DownloadNewVersion();
		}
	);
	connect(ui->btnStop, &QPushButton::clicked, this, [this]
		{
			this->close();
		}
	);
	connect(ui->btnUpdateLog, &QPushButton::clicked, this, []
		{
			QString url = "http://" + Config::getInstance()->m_logisticsAddress +
				URL_LOGISTICS_DRIVER_UPDATE_LOG;
			QDesktopServices::openUrl(url);
		}
	);
}

UpdateProcessWindow::~UpdateProcessWindow()
{
    delete ui;
}

void UpdateProcessWindow::Update()
{
	//更新:
	//1、后台检测更新
	//2、发现有更新包时，弹窗提示用户更新
	//3、用户选择更新后,隐藏主窗口
	//4、下载新的安装包，下载完毕后，执行安装包程序，并退出当前主程序
	connect(m_updateDetector, &UpdateDetector::UpdateFinish, this,
		[this](UpdateResult updateResult)
		{
			switch (updateResult) {
			case UpdateResult::UpdateError:
			case UpdateResult::IsUpToDate:
				break;
			case UpdateResult::HaveNewerVersion:
				//提醒更新
				this->show();
				break;
			default:
				break;
			}
		},
		Qt::QueuedConnection);

	QString version = QString::fromUtf16((const ushort*)VERSION_FILEVERSION);
	QString url = "http://" + Config::getInstance()->m_logisticsAddress + 
		URL_LOGISTICS_GET_DRIVER_VERSION;
	m_updateDetector->CheckUpdate(url, version);
}

void UpdateProcessWindow::closeEvent(QCloseEvent* event)
{
	m_updateDetector->Stop();
	auto mainWindow = GetFirstWindowPtr<MainWindow>();
	if(mainWindow->isHidden())
		mainWindow->show();
}

void UpdateProcessWindow::DownloadNewVersion()
{
	QString location = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	QString strdownloadDir = QString("%1/%2/download").arg(location, CONFIG_DIR);
	QDir downloadDir;
	if (!downloadDir.exists(strdownloadDir))
	{
		downloadDir.mkpath(strdownloadDir);
	}
	auto updateTimestamp = GetMillTimestampSinceEpoch();
	QString downPath = strdownloadDir + "/RemoteDrive_Setup.exe";
	QString url = "http://" + Config::getInstance()->m_logisticsAddress + 
		URL_LOGISTICS_DRIVER_PACKAGE_URL;
	m_updateDetector->Download(url, downPath,
		[=](qint64 bytesReceived, qint64 writeSize,
			qint64 totalSize, HttpErrorCode resCode) mutable
		{
			switch (resCode)
			{
			case HttpErrorCode::Downloading:
			{
				if (!totalSize)
					return;

				auto curTimestamp = GetMillTimestampSinceEpoch();
				auto updateEscapeTime = curTimestamp - updateTimestamp;
				if (updateEscapeTime <= 1000)
				{
					return;
				}
				updateTimestamp = curTimestamp;
				int percent = (int)((double)writeSize / totalSize * 100);
				ui->progressBar->setValue(percent);
			}
			break;
			case HttpErrorCode::Success:
				//关闭当前程序
				qApp->quit();
				//启动安装包程序
				QProcess::startDetached(downPath);
				break;
			case HttpErrorCode::Fail:
			case HttpErrorCode::OpenFileFial:
			{
				//提醒用户
			}
			break;
			default:
				break;
			}
		});
}

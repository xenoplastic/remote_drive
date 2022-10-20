#include "rightvideowindow.h"
#include "ui_rightvideowindow.h"
#include "Config.h"

RightVideoWindow::RightVideoWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RightVideoWindow)
{
	setWindowFlags(
		this->windowFlags() |
		Qt::WindowMinimizeButtonHint |
		Qt::WindowMaximizeButtonHint
	);
    ui->setupUi(this);
    this->setWindowTitle(QStringLiteral("右摄像头"));
    ui->msgLabel->hide();
}

RightVideoWindow::~RightVideoWindow()
{
    delete ui;
}

void RightVideoWindow::OpenVideo(const QString& videoPath, VideoData* videoData/* = nullptr*/)
{
    auto config = Config::getInstance();
    ui->msgLabel->hide();
    ui->openGLWidget->StartMedia(
        videoPath, 
        config->m_pullVideoProtocol,
        config->m_openVideoTimeout,
        config->m_readVideoTimeout,
        videoData
    );
    m_rtspUrl = videoPath;   
}

void RightVideoWindow::StopVideo()
{
    ui->msgLabel->hide();
    ui->openGLWidget->StopMedia();   
}

void RightVideoWindow::ReopenVideo()
{   
    
    ui->msgLabel->hide();
    if (!m_rtspUrl.isEmpty())
    {
        auto config = Config::getInstance();
		ui->openGLWidget->StartMedia(
			m_rtspUrl,
			config->m_pullVideoProtocol,
			config->m_openVideoTimeout,
			config->m_readVideoTimeout
		);
    }		
}

MediaElement* RightVideoWindow::GetMediaElement()
{
    return ui->openGLWidget;
}

void RightVideoWindow::ShowReopenText()
{
    ui->msgLabel->show();
}

bool RightVideoWindow::IsReopenTextShow() const
{
    return ui->msgLabel->isVisible();
}

QWidget* RightVideoWindow::GetRightContainer() const
{
    return ui->rightContainer;
}

void RightVideoWindow::SetCarPos(double x2d, double y2d)
{
	auto config = Config::getInstance();
	double& originX = config->m_mapOriginX;
	double& originY = config->m_mapOriginY;
	double& resolution = config->m_mapResolution;
	double x = (x2d - originX) / resolution;
	double y = (y2d - originY) / resolution;
    ui->mapCanvas->SetCarPos(x, y);
}

void RightVideoWindow::closeEvent(QCloseEvent* event)
{
    ui->openGLWidget->StopMedia();
    emit closeWindow();
}

void RightVideoWindow::showEvent(QShowEvent* event)
{
    ui->msgLabel->hide();
}

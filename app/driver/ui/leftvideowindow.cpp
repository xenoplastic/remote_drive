#include "leftvideowindow.h"
#include "ui_leftvideowindow.h"
#include "Config.h"

#include <QCloseEvent>

LeftVideoWindow::LeftVideoWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LeftVideoWindow)
{
    setWindowFlags(
        this->windowFlags() | 
        Qt::WindowMinimizeButtonHint | 
        Qt::WindowMaximizeButtonHint
    );
    ui->setupUi(this);
    this->setWindowTitle(QStringLiteral("左摄像头"));
    ui->msgLabel->hide();
}

LeftVideoWindow::~LeftVideoWindow()
{
    delete ui;
}

void LeftVideoWindow::OpenVideo(const QString& videoPath, VideoData* videoData/* = nullptr*/)
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

void LeftVideoWindow::StopVideo()
{
    ui->msgLabel->hide();
    ui->openGLWidget->StopMedia();
}

void LeftVideoWindow::ReopenVideo()
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

CarMedia* LeftVideoWindow::GetMediaElement()
{
    return ui->openGLWidget;
}

void LeftVideoWindow::ShowReopenText()
{
    ui->msgLabel->show();
}

bool LeftVideoWindow::IsReopenTextShow() const
{
    return ui->msgLabel->isVisible();
}

void LeftVideoWindow::closeEvent(QCloseEvent* event)
{
    ui->openGLWidget->StopMedia();
    emit closeWindow();
}

void LeftVideoWindow::showEvent(QShowEvent* event)
{
    ui->msgLabel->hide();
}

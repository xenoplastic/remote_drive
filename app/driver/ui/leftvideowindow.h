#pragma once

#include <QDialog>
#include <QString>

#include "CarMedia.h"

namespace Ui {
class LeftVideoWindow;
}

class LeftVideoWindow : public QDialog
{
    Q_OBJECT
public:
    explicit LeftVideoWindow(QWidget *parent = nullptr);
    ~LeftVideoWindow();

    void OpenVideo(const QString& videoPath, VideoData* videoData = nullptr);
    void StopVideo();
    void ReopenVideo();
    CarMedia* GetMediaElement();
    void ShowReopenText();
    bool IsReopenTextShow() const;

signals:
    void closeWindow();

protected:
	void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    Ui::LeftVideoWindow *ui;
    QString m_rtspUrl;
};

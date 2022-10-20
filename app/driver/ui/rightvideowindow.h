#pragma once

#include <QDialog>
#include <QString>

#include "MediaElement.h"

namespace Ui {
class RightVideoWindow;
}

class RightVideoWindow : public QDialog
{
    Q_OBJECT
public:
    explicit RightVideoWindow(QWidget *parent = nullptr);
    ~RightVideoWindow();

	void OpenVideo(const QString& videoPath, VideoData* videoData = nullptr);
	void StopVideo();
    void ReopenVideo();
    MediaElement* GetMediaElement();
    void ShowReopenText();
    bool IsReopenTextShow() const;
    QWidget* GetRightContainer() const;
    void SetCarPos(double x2d, double y2d);

signals:
	void closeWindow();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    Ui::RightVideoWindow *ui;
    QString m_rtspUrl;
};

#pragma once

#include <QDialog>

namespace Ui {
class UpdateProcessWindow;
}

class UpdateDetector;

class UpdateProcessWindow : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateProcessWindow( QWidget *parent = nullptr);
    ~UpdateProcessWindow();

    void Update();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void DownloadNewVersion();

private:
    Ui::UpdateProcessWindow *ui;
    UpdateDetector* m_updateDetector = nullptr;
};

#pragma once

#include <QWidget>

namespace Ui {
class SpeedSetWindow;
}

class SpeedSetWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SpeedSetWindow(QWidget *parent = nullptr);
    ~SpeedSetWindow();

signals:
	void windowClose();

private:
    Ui::SpeedSetWindow *ui;
};

#pragma once

#include <QWidget>

#include "defines.h"

namespace Ui {
class CarInfoItem;
}

class CarInfoItem : public QWidget
{
    Q_OBJECT

public:
    explicit CarInfoItem(QWidget *parent = nullptr);
    ~CarInfoItem();

    void SetInfo(const QString& info);
    void SetState(const CarState& state);
    void EnableOpenVideoButton(bool enable);
    void SetStateText(const QString& text);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
	void openVideo();

public:
    Ui::CarInfoItem *ui;
};


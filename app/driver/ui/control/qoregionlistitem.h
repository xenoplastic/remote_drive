#pragma once

#include <QWidget>

namespace Ui {
class QOregionListItem;
}

class QOregionListItem : public QWidget
{
    Q_OBJECT

public:
    explicit QOregionListItem(QWidget *parent = nullptr);
    ~QOregionListItem();

signals:
    void buttonClick();

private:
    Ui::QOregionListItem *ui;
};

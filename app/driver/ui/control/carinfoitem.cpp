#include "carinfoitem.h"
#include "ui_carinfoitem.h"
#include "CarData.h"

#include <QImage>
#include <QStyle>

CarInfoItem::CarInfoItem(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CarInfoItem)
{
    ui->setupUi(this);

    QImage img(":/images/open_video_normal.png");
    ui->openVideoBtn->setFixedSize(img.size());

    connect(ui->openVideoBtn, &QPushButton::clicked, this, &CarInfoItem::openVideo);

    installEventFilter(this);

    ui->body->setStyleSheet("background-color: rgb(252, 252,252);");
}

CarInfoItem::~CarInfoItem()
{
    delete ui;
}

void CarInfoItem::SetInfo(const QString& info)
{
    ui->carInfoLabel->setText(info);
}

void CarInfoItem::SetState(const CarState& state)
{
    ui->carStateLabel->setText(CarData::m_stateText[state]);
    if (state == CarState::Fault)
    {
        ui->carStateLabel->setProperty("stateColor", "red");
    }
    else
    {
        ui->carStateLabel->setProperty("stateColor", "blue");
    }
	ui->carStateLabel->style()->unpolish(ui->carStateLabel);
	ui->carStateLabel->style()->polish(ui->carStateLabel);
}

void CarInfoItem::EnableOpenVideoButton(bool enable)
{
    ui->openVideoBtn->setEnabled(enable);
}

void CarInfoItem::SetStateText(const QString& text)
{
    ui->carStateLabel->setText(text);
}

bool CarInfoItem::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() == QEvent::Enter)
	{
        ui->body->setStyleSheet("background-color: rgb(240, 240,255);");        
	}
	else if (event->type() == QEvent::Leave)
	{
        ui->body->setStyleSheet("background-color: rgb(252, 252,252);");
	}

    return QWidget::eventFilter(watched, event);
}

#include "qoregionlistitem.h"
#include "ui_qoregionlistitem.h"
#include "Config.h"

#include <QMessageBox>
#include <QPushButton>
#include <QIcon>

QOregionListItem::QOregionListItem(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QOregionListItem)
{
    ui->setupUi(this);
	connect(ui->pushButton, &QPushButton::clicked, this, &QOregionListItem::buttonClick);

    auto area = Config::getInstance()->m_area;
    ui->areaLabel->setText(area);
}

QOregionListItem::~QOregionListItem()
{
    delete ui;
}

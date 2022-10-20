#include "speedsetwindow.h"
#include "ui_speedsetwindow.h"
#include "Config.h"

SpeedSetWindow::SpeedSetWindow(QWidget *parent) :
	QWidget(parent),
    ui(new Ui::SpeedSetWindow)
{
    ui->setupUi(this);
    ui->body->resize(this->size());
	connect(ui->closeBtn, &QPushButton::clicked, this, 
        &SpeedSetWindow::windowClose);
    connect(ui->okBtn, &QPushButton::clicked, this, [this]
        {  
            auto config = Config::getInstance();
			config->SetSpeeds(QHash<int, double>{
				{ 1, ui->oneSpeedEdit->value() },
				{ 2, ui->twoSpeedEdit->value() },
				{ 3, ui->threeSpeedEdit->value() },
				{ 4, ui->fourSpeedEdit->value() },
				{ 5, ui->fiveSpeedEdit->value() },
				{ 6, ui->sixSpeedEdit->value() },
			});
            emit windowClose();
        }
    );

    auto config = Config::getInstance();
    ui->oneSpeedEdit->setValue(config->m_carSpeedsConfig[1]);
    ui->twoSpeedEdit->setValue(config->m_carSpeedsConfig[2]);
    ui->threeSpeedEdit->setValue(config->m_carSpeedsConfig[3]);
    ui->fourSpeedEdit->setValue(config->m_carSpeedsConfig[4]);
    ui->fiveSpeedEdit->setValue(config->m_carSpeedsConfig[5]);
    ui->sixSpeedEdit->setValue(config->m_carSpeedsConfig[6]);
}

SpeedSetWindow::~SpeedSetWindow()
{
    delete ui;
}


#include "messagewidget.h"
#include "ui_messagewidget.h"

MessageWidget::MessageWidget(
    const QString& text, 
    MSG_CALLBACK callback, 
    QWidget *parent
) :
    QWidget(parent),
    ui(new Ui::MessageWidget)
{
    ui->setupUi(this);
    ui->textLabel->setText(text);
    ui->widget->setStyleSheet(QString::fromUtf8("#widget{border:3px solid red}"));

    connect(ui->okBtn, &QPushButton::clicked, this, [this, callback]
        {
            callback(true);
            delete this;
        }
    );
	connect(ui->noBtn, &QPushButton::clicked, this, [this, callback]
		{
            callback(false);
			delete this;
		}
	);

	setWindowFlags(Qt::FramelessWindowHint);
	setAttribute(Qt::WA_TranslucentBackground, true);
}

MessageWidget::~MessageWidget()
{
    delete ui;
}

#pragma once

#include <QWidget>
#include <functional>

namespace Ui {
class MessageWidget;
}

using MSG_CALLBACK = std::function<void(bool ok)>;

class MessageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MessageWidget(
        const QString& text, 
        MSG_CALLBACK callback, 
        QWidget *parent = nullptr
    );
    ~MessageWidget();

private:
    Ui::MessageWidget *ui;
};

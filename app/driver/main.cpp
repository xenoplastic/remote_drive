#include "mainwindow.h"
#include "Common.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/images/favicon.ico"));
    MainWindow w;
    w.show();
    auto exitCode = app.exec();

    //罗技SDK有BUG，导致进程无法退出，使用Win32 API强制退出
    ForceExit(exitCode);

    return 0;
}

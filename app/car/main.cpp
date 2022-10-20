#include <QCoreApplication>
#include <iostream>

#include "DispatchClient.h"

#define VERSION "1.0.1.16"

int main(int argc, char *argv[])
{
    std::cout << "current version:" << VERSION << std::endl;
    QCoreApplication a(argc, argv);

    DispatchClient dispatchCLient;
    dispatchCLient.StartWork();

    return a.exec();
}

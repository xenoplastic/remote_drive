#pragma once

#include <QString>
#include <QApplication>

#ifdef linux
#include <string.h>
#define memcpy_s(dst, dstSize, src, srcSize) memcpy((dst), (src), (srcSize))
#endif

class QWidget;

QString ExeCmd(const char* cmd);

/*
*find usb number through commond "ll /sys/class/video4linux"
*show info camera device through commond "udevadm info -q path -n /dev/video0"
*/
QString GetDevicePath(const char* usbNum);

#ifdef WIN32
void ForceExit(unsigned exitCode);
#endif

//获取指定类型的第一个窗口指针
template <typename T>
T* GetFirstWindowPtr()
{
    foreach(QWidget * w, qApp->topLevelWidgets())
        if (T* windowPtr = qobject_cast<T*>(w))
            return windowPtr;
    return nullptr;
}

void SetWidgetCssProperty(QWidget* widget, const char* name, const QVariant& value);

//应该由车端把视频url发给驾驶端
QString GetPushMediaUrl(const QString& roomId, const QString& cameraName, const QString& addr,
    const QString& protocol);
QString GetPullMediaUrl(const QString& roomId, const QString& cameraName, const QString& addr,
    const QString& protocol);
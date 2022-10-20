#include "BusinessFun.h"

#include <Config.h>
#include "Common.h"

QString GetCameraDevicePath(const QString& cameraName)
{
    auto config = Config::getInstance();
    QString cameraUsbNum;
    if(cameraName == "left_camera")
    {
        cameraUsbNum = config->m_leftCameraPath;
    }
    else if(cameraName == "front_camera")
    {
        cameraUsbNum = config->m_frontCameraPath;
    }
    else if(cameraName == "right_camera")
    {
        cameraUsbNum = config->m_rightCameraPath;
    }
	else if (cameraName == "back_camera")
	{
        cameraUsbNum = config->m_backCameraPath;
	}
    if(cameraUsbNum.contains("rtsp"))
        return cameraUsbNum;
    return GetDevicePath(cameraUsbNum.toStdString().c_str());
}

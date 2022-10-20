#include "Common.h"

#include <QProcess>

#ifdef WIN32
#include <windows.h>
#endif

QString ExeCmd(const char* cmd)
{
    QProcess proc;
    proc.startCommand(cmd);
    proc.waitForFinished();
    return QString::fromLocal8Bit(proc.readAllStandardOutput());
}

QString GetDevicePath(const char* usbNum)
{
    QString cmdPrefix = "udevadm info -q path -n /dev/";
    QString cmdGetAllCameras = "ls /sys/class/video4linux/";
    QString outStr;
    QString cmd;

    //get all device path of camera
    outStr = ExeCmd(cmdGetAllCameras.toStdString().c_str());
    auto allCameras =  outStr.split("\n");

    //find which device path matchs usbNum
    for(auto& cam : allCameras)
    {
        cmd = cmdPrefix + cam;
        outStr = ExeCmd(cmd.toStdString().c_str());
        if(outStr.contains(usbNum))
        {
            return "/dev/" + cam;
        }
    }
    return "";
}

#ifdef WIN32
void ForceExit(unsigned exitCode)
{
	HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, GetCurrentProcessId());
	if (hProcess)
	{
		TerminateProcess(hProcess, exitCode);
		CloseHandle(hProcess);
	}
}
#endif

#include <QWidget>
#include <QStyle>
#include <QVariant>

void SetWidgetCssProperty(QWidget* widget, const char* name, const QVariant& value)
{
	widget->setProperty(name, value);
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
}

QString GetPushMediaUrl(const QString& roomId, const QString& cameraName, const QString& addr,
    const QString& protocol)
{
    if (protocol == "srt")
    {
        //srt://[your.sls.ip]:4455?streamid=uplive.sls.com/live/test
//		return QString("srt://%1?streamid=#!::r=live/livestream,roomid=%2,pos=%3,m=publish").
//			arg(addr, roomId, cameraName);
        return QString("srt://%1?streamid=uplive.sls.com/live/%2/%3").
                    arg(addr, roomId, cameraName);
    }
    else if(protocol == "rtsp")
    {
        return QString("rtsp://%1/%2/%3").arg(addr, roomId, cameraName);
    }
    return "";
}

QString GetPullMediaUrl(const QString& roomId, const QString& cameraName, const QString& addr,
    const QString& protocol)
{
	if (protocol == "srt")
	{
        //srs服务url(srs推流出现IO错误)
		//return QString("srt://%1?streamid=#!::r=live/livestream,roomid=%2,pos=%3,m=request").
		//	arg(addr, roomId, cameraName);
        
		//srt-live-server服务url
		return QString("srt://%1?streamid=live.sls.com/live/%2/%3").
			arg(addr, roomId, cameraName);
	}
	else if (protocol == "rtsp")
	{
		return QString("rtsp://%1/%2/%3").arg(addr, roomId, cameraName);
	}
	return "";
}

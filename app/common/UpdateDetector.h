#pragma once

#include "HttpClient.h"

#include <QObject>
#include <QString>

enum class UpdateResult
{
    IsUpToDate,
    HaveNewerVersion,
    UpdateError
};

//1、函数A检查本地版本与远程版本，通过信号传递UpdateResult
//2、函数B执行下载更新包步骤，函数B：包url，下载目的地址，下载过程回调
class UpdateDetector : public HttpClient
{
    Q_OBJECT
public:
    explicit UpdateDetector(QObject *parent = nullptr);

    void CheckUpdate(const QString urlVersion, const QString& localVersion);

    bool Download(
        const QString& url,
        const QString& saveFilePath,
        DOWNLOAD_CALLBACK callback
    ) override;

    QString GetServerCarVersion() const;

signals:
    void UpdateFinish(UpdateResult);

private:
    QString m_serverCarVersion;
};

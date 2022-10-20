#include "UpdateDetector.h"
#include "Defines.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

using namespace std;

UpdateDetector::UpdateDetector(QObject *parent)
    : HttpClient{parent}
{

}

void UpdateDetector::CheckUpdate(const QString urlVersion, const QString& localVersion)
{
    cout << "CheckUpdate:urlVersion=" << urlVersion.toStdString() << endl;
    this->Get(urlVersion, [=](const QByteArray& data, HttpErrorCode resCode){
        if(resCode != HttpErrorCode::Success)
        {
            cout << "CheckUpdate:resCode != HttpErrorCode" << endl;
            emit UpdateFinish(UpdateResult::UpdateError);
            return;
        }

        QJsonParseError jsonError;
        QJsonDocument doucment = QJsonDocument::fromJson(data, &jsonError);
        if (jsonError.error == QJsonParseError::NoError && doucment.isObject())
        {
            const QJsonObject& obj = doucment.object();
            auto errroCode = (ErrorCode)obj["error_code"].toInt();
            if(errroCode == ErrorCode::NoError)
            {
                auto newVersion = obj["version"].toString();
                newVersion.replace(" ", "");
                newVersion.replace("\n", "");
                m_serverCarVersion = newVersion;
                emit UpdateFinish(m_serverCarVersion == localVersion ?
                    UpdateResult::IsUpToDate : UpdateResult::HaveNewerVersion);
            }
            else
            {
                cout << "CheckUpdate:errroCode != ErrorCode::NoError" << endl;
                emit UpdateFinish(UpdateResult::UpdateError);
            }
            return;
        }
        cout << "CheckUpdate:jsonError.error=" << jsonError.error << endl;
        emit UpdateFinish(UpdateResult::UpdateError);
    });
}

bool UpdateDetector::Download(
        const QString& url,
        const QString& saveFilePath,
        DOWNLOAD_CALLBACK callback
        )
{
    QFile file;
    file.setFileName(saveFilePath);
    if(file.exists())
    {
        if(!file.remove())
            m_downloadCallback(0, 0, 0, HttpErrorCode::OpenFileFial);
    }
    return HttpClient::Download(url, saveFilePath, callback);
}

QString UpdateDetector::GetServerCarVersion() const
{
    return m_serverCarVersion;
}

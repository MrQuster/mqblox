#pragma once
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

namespace mqblox {

class RobloxApiClient : public QObject {
    Q_OBJECT
public:
    explicit RobloxApiClient(QObject *parent = nullptr);
    void fetchUserProfileAnd3D(qulonglong userId, const QString &roblosecurity);

signals:
    void profileUpdated(qulonglong uid, const QString &uname, const QString &disp);
    void robuxUpdated(const QString &rbx);
    void networkError(const QString &err);

private:
    QNetworkAccessManager *m_nam;
};

} // namespace mqblox

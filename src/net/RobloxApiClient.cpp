#include "net/RobloxApiClient.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace mqblox {

RobloxApiClient::RobloxApiClient(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)) {}

void RobloxApiClient::fetchUserProfileAnd3D(qulonglong userId, const QString &roblosecurity) {
    if (roblosecurity.isEmpty()) return;

    QNetworkRequest reqAuth(QUrl("https://users.roblox.com/v1/users/authenticated"));
    reqAuth.setRawHeader("Cookie", QString(".ROBLOSECURITY=%1").arg(roblosecurity).toUtf8());
    auto *repAuth = m_nam->get(reqAuth);
    connect(repAuth, &QNetworkReply::finished, this, [this, repAuth, roblosecurity]() {
        repAuth->deleteLater();
        if (repAuth->error() != QNetworkReply::NoError) return;
        QJsonObject obj = QJsonDocument::fromJson(repAuth->readAll()).object();
        qulonglong uid = obj.value("id").toVariant().toULongLong();
        QString name = obj.value("name").toString();
        QString disp = obj.value("displayName").toString(name);
        if (uid > 0) {
            emit profileUpdated(uid, name, disp);
            QNetworkRequest reqRbx(QUrl(QString("https://economy.roblox.com/v1/users/%1/currency").arg(uid)));
            reqRbx.setRawHeader("Cookie", QString(".ROBLOSECURITY=%1").arg(roblosecurity).toUtf8());
            auto *repRbx = m_nam->get(reqRbx);
            connect(repRbx, &QNetworkReply::finished, this, [this, repRbx]() {
                repRbx->deleteLater();
                if (repRbx->error() != QNetworkReply::NoError) return;
                QJsonObject rObj = QJsonDocument::fromJson(repRbx->readAll()).object();
                if (rObj.contains("robux")) {
                    emit robuxUpdated(QString::number(rObj.value("robux").toVariant().toLongLong()));
                }
            });
        }
    });
    (void)userId;
}

} // namespace mqblox

#include "core/SecretStore.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>

namespace mqblox {

static QByteArray deriveKey() {
    QByteArray seed = QSysInfo::machineUniqueId();
    if (seed.isEmpty()) seed = "mqblox_local_vault_key_v26";
    return QCryptographicHash::hash(seed + ":mqblox_salt", QCryptographicHash::Sha256);
}

static QByteArray xorCrypt(const QByteArray &data) {
    QByteArray key = deriveKey();
    QByteArray out = data;
    for (qsizetype i = 0; i < out.size(); ++i) {
        out[i] = out[i] ^ key[i % key.size()];
    }
    return out;
}

static QString vaultFilePath() {
    QString dir = QDir::homePath() + "/.local/share/mqblox";
    QDir().mkpath(dir);
    return dir + "/vault_v13.bin";
}

bool SecretStore::storeToken(const QString &accountId, const QString &token) {
    QFile f(vaultFilePath());
    QJsonObject obj;
    if (f.open(QIODevice::ReadOnly)) {
        obj = QJsonDocument::fromJson(xorCrypt(QByteArray::fromBase64(f.readAll()))).object();
        f.close();
    }
    obj[accountId] = token;
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    f.write(xorCrypt(QJsonDocument(obj).toJson(QJsonDocument::Compact)).toBase64());
    f.close();
    return true;
}

QString SecretStore::loadToken(const QString &accountId) {
    QFile f(vaultFilePath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    QJsonObject obj = QJsonDocument::fromJson(xorCrypt(QByteArray::fromBase64(f.readAll()))).object();
    return obj.value(accountId).toString();
}

} // namespace mqblox

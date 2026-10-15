#pragma once
#include "core/Types.hpp"
#include <QString>
#include <QVector>

namespace mqblox {

class SettingsManager {
public:
    SettingsManager();

    void loadAll();
    bool saveAll(QString *errMsg = nullptr);

    AppSettings &settings() { return m_settings; }
    const AppSettings &settings() const { return m_settings; }

    QVector<AccountProfile> &accounts() { return m_accounts; }
    const QVector<AccountProfile> &accounts() const { return m_accounts; }

    QString activeAccountId() const { return m_activeAccountId; }
    void setActiveAccountId(const QString &id) { m_activeAccountId = id; }
    AccountProfile *activeAccount();

    QString mqDir() const;
    QString accountsDir() const;
    QString soberBaseDir() const;

    bool exportPresetToFile(const QString &filePath) const;
    bool importPresetFromFile(const QString &filePath, QString *errMsg = nullptr);

    static QString parsePlaceUri(const QString &input);

private:
    bool writeSoberConfigJson(QString *errMsg) const;

    AppSettings m_settings;
    QVector<AccountProfile> m_accounts;
    QString m_activeAccountId = "default";
};

} // namespace mqblox

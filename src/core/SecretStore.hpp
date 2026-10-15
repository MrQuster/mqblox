#pragma once
#include <QString>

namespace mqblox {
class SecretStore {
public:
    static bool storeToken(const QString &accountId, const QString &token);
    static QString loadToken(const QString &accountId);
};
} // namespace mqblox

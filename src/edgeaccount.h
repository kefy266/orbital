#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

class QNetworkAccessManager;
class QTcpServer;
class QTcpSocket;

// Optional EdgeAccount sign-in (OpenID Connect + PKCE, loopback redirect per RFC 8252).
// The EdgeAccount access token is traded once at orbital.oedge.xyz for a long-lived
// Orbital session token (models are no longer served through it).
// The session lives in settings.json under "edgeAccount".
class EdgeAccount : public QObject
{
    Q_OBJECT
public:
    explicit EdgeAccount(QNetworkAccessManager *nam, QObject *parent = nullptr);

    static QString issuer();
    static QString cloudUrl();   // https://orbital.oedge.xyz
    static QString token();      // Orbital session token, empty when signed out
    // Developer accounts get ORBITAL Code (ORBITAL Cloud models in Kod, no daily quota).
    // Cached in settings so the provider is there at startup; refreshed from /api/ben.
    static bool developer();
    static int orbitalLevel();   // ORBITAL Web models: 1 Fast, 2 up to Pro, 3 all
    void refreshEntitlements();

    bool signedIn() const { return !token().isEmpty(); }
    QString email() const;
    QString name() const;
    bool busy() const { return m_busy; }

    void signIn();
    void signOut();
    void cancel();

Q_SIGNALS:
    void changed();
    void entitlementsChanged();
    void finished(bool ok, const QString &message);

private:
    void onDiscovery(const QString &authEndpoint, const QString &tokenEndpoint);
    void onConnection();
    void exchangeCode(const QString &code);
    void openCloudSession(const QString &accessToken);
    void fail(const QString &message);
    void reset();

    QNetworkAccessManager *m_nam;
    QPointer<QTcpServer> m_server;
    QTimer m_timeout;
    bool m_busy = false;
    QString m_tokenEndpoint;
    QString m_redirectUri;
    QString m_verifier;
    QString m_state;
};

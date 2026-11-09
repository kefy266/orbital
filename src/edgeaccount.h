#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

class QNetworkAccessManager;
class QTcpServer;
class QTcpSocket;

// Optional EdgeAccount sign-in (OpenID Connect + PKCE, loopback redirect per RFC 8252).
// The EdgeAccount access token is traded once at ORBITAL Cloud for a long-lived
// ORBITAL Cloud session token; with it, the "orbital" provider (Config::providers)
// serves models through orbital.oedge.xyz without a provider API key.
// The session lives in settings.json under "edgeAccount".
class EdgeAccount : public QObject
{
    Q_OBJECT
public:
    explicit EdgeAccount(QNetworkAccessManager *nam, QObject *parent = nullptr);

    static QString issuer();
    static QString cloudUrl();   // https://orbital.oedge.xyz
    static QString token();      // ORBITAL Cloud session token, empty when signed out

    bool signedIn() const { return !token().isEmpty(); }
    QString email() const;
    QString name() const;
    bool busy() const { return m_busy; }

    void signIn();
    void signOut();
    void cancel();

Q_SIGNALS:
    void changed();
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

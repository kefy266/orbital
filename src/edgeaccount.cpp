#include "edgeaccount.h"

#include "paths.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

static const char kClientId[] = "orbital-masaustu";

static QString randomUrlSafe(int bytes)
{
    QByteArray raw(bytes, Qt::Uninitialized);
    QRandomGenerator::system()->generate(raw.begin(), raw.end());
    return QString::fromLatin1(raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

static QJsonObject stored()
{
    return Paths::loadSettings().value(QStringLiteral("edgeAccount")).toObject();
}

EdgeAccount::EdgeAccount(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(5 * 60 * 1000);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail(tr("Giriş zaman aşımına uğradı.")); });
}

QString EdgeAccount::issuer()
{
    const QString env = qEnvironmentVariable("ORBITAL_EDGEACCOUNT_URL");
    return env.isEmpty() ? QStringLiteral("https://hesap.oedge.xyz") : env;
}

QString EdgeAccount::cloudUrl()
{
    const QString env = qEnvironmentVariable("ORBITAL_CLOUD_URL");
    return env.isEmpty() ? QStringLiteral("https://orbital.oedge.xyz") : env;
}

QString EdgeAccount::token()
{
    return stored().value(QStringLiteral("token")).toString();
}

QString EdgeAccount::email() const
{
    return stored().value(QStringLiteral("email")).toString();
}

QString EdgeAccount::name() const
{
    return stored().value(QStringLiteral("name")).toString();
}

void EdgeAccount::signIn()
{
    if (m_busy)
        return;
    m_busy = true;
    Q_EMIT changed();
    m_timeout.start();

    QNetworkRequest req(QUrl(issuer() + QStringLiteral("/.well-known/openid-configuration")));
    req.setTransferTimeout(15000);
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (!m_busy)
            return;
        const QJsonObject d = QJsonDocument::fromJson(reply->readAll()).object();
        const QString auth = d.value(QStringLiteral("authorization_endpoint")).toString();
        const QString tok = d.value(QStringLiteral("token_endpoint")).toString();
        if (reply->error() != QNetworkReply::NoError || auth.isEmpty() || tok.isEmpty()) {
            fail(tr("EdgeAccount'a ulaşılamadı. İnternet bağlantını kontrol et."));
            return;
        }
        onDiscovery(auth, tok);
    });
}

void EdgeAccount::onDiscovery(const QString &authEndpoint, const QString &tokenEndpoint)
{
    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        fail(tr("Yerel giriş dönüşü açılamadı: %1").arg(m_server->errorString()));
        return;
    }
    connect(m_server, &QTcpServer::newConnection, this, &EdgeAccount::onConnection);

    m_tokenEndpoint = tokenEndpoint;
    m_redirectUri = QStringLiteral("http://127.0.0.1:%1/geri").arg(m_server->serverPort());
    m_verifier = randomUrlSafe(48);
    m_state = randomUrlSafe(16);
    const QByteArray challenge = QCryptographicHash::hash(m_verifier.toLatin1(), QCryptographicHash::Sha256)
                                     .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

    QUrl url(authEndpoint);
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(kClientId));
    q.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    q.addQueryItem(QStringLiteral("scope"), QStringLiteral("openid email profile"));
    q.addQueryItem(QStringLiteral("redirect_uri"), m_redirectUri);
    q.addQueryItem(QStringLiteral("state"), m_state);
    q.addQueryItem(QStringLiteral("code_challenge"), QString::fromLatin1(challenge));
    q.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    url.setQuery(q);
    if (!QDesktopServices::openUrl(url))
        fail(tr("Tarayıcı açılamadı."));
}

void EdgeAccount::onConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        socket->setParent(this); // outlives the server, which closes as soon as the code arrives
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            QByteArray head = socket->property("head").toByteArray() + socket->readAll();
            socket->setProperty("head", head);
            if (!head.contains("\r\n\r\n") && head.size() < 16384)
                return;
            const QList<QByteArray> line = head.left(head.indexOf("\r\n")).split(' ');
            const QUrl target(QStringLiteral("http://127.0.0.1") + QString::fromLatin1(line.value(1)));
            const QUrlQuery q(target);
            const bool ours = line.value(0) == "GET" && target.path() == QLatin1String("/geri");
            const QString error = q.queryItemValue(QStringLiteral("error"));
            const QString code = q.queryItemValue(QStringLiteral("code"));
            const bool ok = ours && error.isEmpty() && !code.isEmpty()
                            && q.queryItemValue(QStringLiteral("state")) == m_state;

            const QByteArray page = QStringLiteral(
                "<!doctype html><html lang=\"tr\"><head><meta charset=\"utf-8\"><title>ORBITAL</title>"
                "<style>body{margin:0;height:100vh;display:grid;place-items:center;font-family:system-ui,sans-serif;"
                "background:#0a0a0a;color:#ededed;text-align:center}p{color:#8a8a8a}</style></head><body><div>"
                "<h1>%1</h1><p>%2</p></div></body></html>")
                .arg(ok ? tr("Giriş tamamlandı") : tr("Giriş tamamlanamadı"),
                     ok ? tr("Bu sekmeyi kapatıp ORBITAL'e dönebilirsin.") : tr("ORBITAL'e dönüp tekrar dene."))
                .toUtf8();
            socket->write("HTTP/1.1 " + QByteArray(ours ? "200 OK" : "404 Not Found")
                          + "\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\n"
                          + "Content-Length: " + QByteArray::number(page.size()) + "\r\n\r\n" + page);
            socket->disconnectFromHost();
            if (!ours)
                return; // e.g. the browser asking for /favicon.ico
            if (m_server)
                m_server->close();
            if (ok)
                exchangeCode(code);
            else
                fail(error == QLatin1String("access_denied") ? tr("Giriş reddedildi.") : tr("Giriş tamamlanamadı."));
        });
    }
}

void EdgeAccount::exchangeCode(const QString &code)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    form.addQueryItem(QStringLiteral("code"), code);
    form.addQueryItem(QStringLiteral("redirect_uri"), m_redirectUri);
    form.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(kClientId));
    form.addQueryItem(QStringLiteral("code_verifier"), m_verifier);
    QNetworkRequest req{QUrl(m_tokenEndpoint)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    req.setTransferTimeout(15000);
    QNetworkReply *reply = m_nam->post(req, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (!m_busy)
            return;
        const QString access = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("access_token")).toString();
        if (access.isEmpty()) {
            fail(tr("EdgeAccount girişi doğrulanamadı."));
            return;
        }
        openCloudSession(access);
    });
}

void EdgeAccount::openCloudSession(const QString &accessToken)
{
    QNetworkRequest req(QUrl(cloudUrl() + QStringLiteral("/api/oturum")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(20000);
    QNetworkReply *reply = m_nam->post(req, QJsonDocument(QJsonObject{{QStringLiteral("access_token"), accessToken}}).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (!m_busy)
            return;
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const QString token = o.value(QStringLiteral("token")).toString();
        if (token.isEmpty()) {
            const QString msg = o.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
            fail(msg.isEmpty() ? tr("Oturum sunucusuna ulaşılamadı.") : msg);
            return;
        }
        QJsonObject s = Paths::loadSettings();
        s.insert(QStringLiteral("edgeAccount"), QJsonObject{{QStringLiteral("token"), token},
                                                            {QStringLiteral("email"), o.value(QStringLiteral("email"))},
                                                            {QStringLiteral("name"), o.value(QStringLiteral("name"))}});
        Paths::saveSettings(s);
        reset();
        Q_EMIT changed();
        Q_EMIT finished(true, tr("EdgeAccount ile giriş yapıldı: %1").arg(email()));
    });
}

void EdgeAccount::signOut()
{
    cancel();
    const QString t = token();
    if (!t.isEmpty()) {
        QNetworkRequest req(QUrl(cloudUrl() + QStringLiteral("/api/oturum")));
        req.setRawHeader("Authorization", "Bearer " + t.toUtf8());
        req.setTransferTimeout(10000);
        QNetworkReply *reply = m_nam->deleteResource(req);
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    }
    QJsonObject s = Paths::loadSettings();
    s.remove(QStringLiteral("edgeAccount"));
    Paths::saveSettings(s);
    Q_EMIT changed();
}

void EdgeAccount::cancel()
{
    if (!m_busy)
        return;
    reset();
    Q_EMIT changed();
}

void EdgeAccount::fail(const QString &message)
{
    if (!m_busy)
        return;
    reset();
    Q_EMIT changed();
    Q_EMIT finished(false, message);
}

void EdgeAccount::reset()
{
    m_busy = false;
    m_timeout.stop();
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
    }
    m_verifier.clear();
    m_state.clear();
}

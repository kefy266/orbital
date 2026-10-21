#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <functional>

class QNetworkAccessManager;
class QProcess;

// Minimal Model Context Protocol client: stdio ("local") and streamable HTTP
// ("remote") transports, enough for initialize, tools/list and tools/call.
class McpClient : public QObject
{
    Q_OBJECT
public:
    using Reply = std::function<void(const QJsonObject &result, const QString &error)>;

    McpClient(const QString &name, const QJsonObject &config, QNetworkAccessManager *nam, QObject *parent = nullptr);
    ~McpClient() override;

    QString name() const { return m_name; }
    QString status() const { return m_status; }
    QJsonArray tools() const { return m_tools; }

    void start();
    void callTool(const QString &tool, const QJsonObject &arguments, std::function<void(const QString &, bool)> done);

Q_SIGNALS:
    void ready();
    void statusChanged(const QString &status);

private:
    void request(const QString &method, const QJsonObject &params, Reply reply);
    void notify(const QString &method, const QJsonObject &params = {});
    void send(const QJsonObject &message);
    void handleMessage(const QJsonObject &message);
    void setStatus(const QString &status);

    QString m_name;
    QJsonObject m_config;
    QNetworkAccessManager *m_nam;
    QPointer<QProcess> m_proc;
    QByteArray m_buffer;
    QByteArray m_sessionId;
    int m_nextId = 1;
    QHash<int, Reply> m_pending;
    QJsonArray m_tools;
    QString m_status = QStringLiteral("başlatılıyor");
};

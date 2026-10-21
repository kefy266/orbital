#include "mcpclient.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QTimer>

McpClient::McpClient(const QString &name, const QJsonObject &config, QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_config(config)
    , m_nam(nam)
{
}

McpClient::~McpClient()
{
    if (m_proc) {
        m_proc->disconnect(this);
        m_proc->kill();
        m_proc->waitForFinished(1000);
    }
}

void McpClient::setStatus(const QString &status)
{
    m_status = status;
    Q_EMIT statusChanged(status);
}

void McpClient::start()
{
    const bool remote = m_config.value(QStringLiteral("type")).toString() == QLatin1String("remote")
                        || m_config.contains(QStringLiteral("url"));
    if (!remote) {
        QStringList cmd;
        for (const QJsonValue &v : m_config.value(QStringLiteral("command")).toArray())
            cmd << v.toString();
        if (cmd.isEmpty()) {
            setStatus(tr("hata: komut yok"));
            return;
        }
        m_proc = new QProcess(this);
        if (m_config.contains(QStringLiteral("cwd")))
            m_proc->setWorkingDirectory(m_config.value(QStringLiteral("cwd")).toString());
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        const QJsonObject extra = m_config.value(QStringLiteral("environment")).toObject();
        for (auto it = extra.begin(); it != extra.end(); ++it)
            env.insert(it.key(), it.value().toString());
        m_proc->setProcessEnvironment(env);
        connect(m_proc, &QProcess::readyReadStandardOutput, this, [this] {
            m_buffer += m_proc->readAllStandardOutput();
            int nl;
            while ((nl = m_buffer.indexOf('\n')) >= 0) {
                const QByteArray line = m_buffer.left(nl).trimmed();
                m_buffer.remove(0, nl + 1);
                if (!line.isEmpty())
                    handleMessage(QJsonDocument::fromJson(line).object());
            }
        });
        connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart)
                setStatus(tr("hata: başlatılamadı"));
        });
        connect(m_proc, &QProcess::finished, this, [this] {
            setStatus(tr("kapandı"));
            for (const Reply &r : std::as_const(m_pending))
                r({}, tr("MCP sunucusu kapandı"));
            m_pending.clear();
        });
        m_proc->start(cmd.takeFirst(), cmd);
    }

    const QJsonObject init{{QStringLiteral("protocolVersion"), QStringLiteral("2025-06-18")},
                           {QStringLiteral("capabilities"), QJsonObject{}},
                           {QStringLiteral("clientInfo"), QJsonObject{{QStringLiteral("name"), QStringLiteral("orbital")},
                                                                      {QStringLiteral("version"), QStringLiteral("0.1.0")}}}};
    request(QStringLiteral("initialize"), init, [this](const QJsonObject &, const QString &error) {
        if (!error.isEmpty()) {
            setStatus(tr("hata: %1").arg(error));
            return;
        }
        notify(QStringLiteral("notifications/initialized"));
        request(QStringLiteral("tools/list"), {}, [this](const QJsonObject &result, const QString &error) {
            if (!error.isEmpty()) {
                setStatus(tr("hata: %1").arg(error));
                return;
            }
            m_tools = result.value(QStringLiteral("tools")).toArray();
            setStatus(tr("bağlı · %1 araç").arg(m_tools.size()));
            Q_EMIT ready();
        });
    });
}

void McpClient::callTool(const QString &tool, const QJsonObject &arguments, std::function<void(const QString &, bool)> done)
{
    request(QStringLiteral("tools/call"),
            QJsonObject{{QStringLiteral("name"), tool}, {QStringLiteral("arguments"), arguments}},
            [done](const QJsonObject &result, const QString &error) {
                if (!error.isEmpty()) {
                    done(error, true);
                    return;
                }
                QStringList parts;
                for (const QJsonValue &v : result.value(QStringLiteral("content")).toArray()) {
                    const QJsonObject c = v.toObject();
                    if (c.value(QStringLiteral("type")).toString() == QLatin1String("text"))
                        parts << c.value(QStringLiteral("text")).toString();
                    else
                        parts << QStringLiteral("[%1]").arg(c.value(QStringLiteral("type")).toString());
                }
                done(parts.join(QLatin1Char('\n')), result.value(QStringLiteral("isError")).toBool());
            });
}

void McpClient::request(const QString &method, const QJsonObject &params, Reply reply)
{
    const int id = m_nextId++;
    m_pending.insert(id, std::move(reply));
    QJsonObject msg{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), id},
                    {QStringLiteral("method"), method}};
    if (!params.isEmpty())
        msg.insert(QStringLiteral("params"), params);
    send(msg);
    QTimer::singleShot(method == QLatin1String("tools/call") ? 300000 : 30000, this, [this, id] {
        if (const Reply r = m_pending.take(id))
            r({}, tr("MCP isteği zaman aşımına uğradı"));
    });
}

void McpClient::notify(const QString &method, const QJsonObject &params)
{
    QJsonObject msg{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("method"), method}};
    if (!params.isEmpty())
        msg.insert(QStringLiteral("params"), params);
    send(msg);
}

void McpClient::send(const QJsonObject &message)
{
    const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
    if (m_proc) {
        m_proc->write(payload + '\n');
        return;
    }
    QNetworkRequest req(QUrl(m_config.value(QStringLiteral("url")).toString()));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json, text/event-stream");
    if (!m_sessionId.isEmpty())
        req.setRawHeader("Mcp-Session-Id", m_sessionId);
    const QJsonObject headers = m_config.value(QStringLiteral("headers")).toObject();
    for (auto it = headers.begin(); it != headers.end(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
    QNetworkReply *reply = m_nam->post(req, payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply, message] {
        reply->deleteLater();
        if (reply->hasRawHeader("Mcp-Session-Id"))
            m_sessionId = reply->rawHeader("Mcp-Session-Id");
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError && message.contains(QStringLiteral("id"))) {
            if (const Reply r = m_pending.take(message.value(QStringLiteral("id")).toInt()))
                r({}, reply->errorString());
            return;
        }
        if (reply->header(QNetworkRequest::ContentTypeHeader).toString().contains(QStringLiteral("event-stream"))) {
            for (const QByteArray &line : body.split('\n'))
                if (line.startsWith("data:"))
                    handleMessage(QJsonDocument::fromJson(line.mid(5).trimmed()).object());
        } else if (!body.trimmed().isEmpty()) {
            handleMessage(QJsonDocument::fromJson(body).object());
        }
    });
}

void McpClient::handleMessage(const QJsonObject &message)
{
    if (!message.contains(QStringLiteral("id")) || message.contains(QStringLiteral("method")))
        return; // server notifications / requests are not needed for tools
    const Reply r = m_pending.take(message.value(QStringLiteral("id")).toInt());
    if (!r)
        return;
    if (message.contains(QStringLiteral("error")))
        r({}, message.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString());
    else
        r(message.value(QStringLiteral("result")).toObject(), {});
}

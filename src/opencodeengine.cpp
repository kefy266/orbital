#include "opencodeengine.h"

#include "config.h"
#include "llm.h"
#include "paths.h"
#include "pluginmanager.h"
#include "skillmanager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QProcess>
#include <QRandomGenerator>
#include <QSysInfo>
#include <QTcpServer>
#include <QTimer>
#include <algorithm>
#include <memory>

#ifdef Q_OS_LINUX
#include <csignal>
#include <sys/prctl.h>
#endif

static QByteArray toJson(const QJsonValue &v)
{
    if (v.isArray())
        return QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact);
    return QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact);
}

static QString platformTag()
{
    const QString arch = QSysInfo::currentCpuArchitecture().contains(QStringLiteral("arm")) ? QStringLiteral("arm64")
                                                                                           : QStringLiteral("x64");
#if defined(Q_OS_WIN)
    return QStringLiteral("windows-") + arch;
#elif defined(Q_OS_MACOS)
    return QStringLiteral("darwin-") + arch;
#else
    return QStringLiteral("linux-") + arch;
#endif
}

OpencodeEngine::OpencodeEngine(SkillManager *skills, PluginManager *plugins, QObject *parent)
    : Engine(parent)
    , m_skills(skills)
    , m_plugins(plugins)
{
}

OpencodeEngine::~OpencodeEngine()
{
    stop();
}

QString OpencodeEngine::version()
{
    const QString v = Config::raw().value(QStringLiteral("opencode")).toObject().value(QStringLiteral("version")).toString().trimmed();
    return v.isEmpty() ? QString::fromLatin1(kDefaultVersion) : v;
}

QString OpencodeEngine::binaryPath()
{
    const QString custom = Config::raw().value(QStringLiteral("opencode")).toObject().value(QStringLiteral("binary")).toString();
    if (!custom.isEmpty())
        return custom;
#if defined(Q_OS_WIN)
    const QString exe = QStringLiteral("opencode.exe");
#else
    const QString exe = QStringLiteral("opencode");
#endif
    return Paths::dataDir() + QStringLiteral("/engine/opencode-%1/package/bin/").arg(version()) + exe;
}

QString OpencodeEngine::homeDir()
{
    QDir dir(Paths::dataDir() + QStringLiteral("/opencode-home"));
    dir.mkpath(QStringLiteral("."));
    return dir.absolutePath();
}

// ---------------------------------------------------------------- lifecycle

void OpencodeEngine::start(const QString &directory)
{
    stop();
    m_stopping = false;
    m_directory = directory;
    if (QFile::exists(binaryPath()))
        launch();
    else
        install();
}

void OpencodeEngine::install()
{
    const QString ver = version();
    const QString tag = platformTag();
    const QUrl url(QStringLiteral("https://registry.npmjs.org/opencode-%1/-/opencode-%1-%2.tgz").arg(tag, ver));
    const QString targetDir = Paths::dataDir() + QStringLiteral("/engine/opencode-%1").arg(ver);
    QDir().mkpath(targetDir);
    Q_EMIT statusMessage(tr("İlk kurulum yapılıyor…"));

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, [this, ver](qint64 got, qint64 total) {
        if (total > 0)
            Q_EMIT statusMessage(tr("İlk kurulum yapılıyor… %1%").arg(got * 100 / total));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, targetDir] {
        reply->deleteLater();
        if (m_stopping)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT failed(tr("Bulut modelleri hazırlanamadı. İnternet bağlantını kontrol edip tekrar dene."));
            return;
        }
        const QString archive = targetDir + QStringLiteral("/opencode.tgz");
        QFile f(archive);
        if (!f.open(QIODevice::WriteOnly)) {
            Q_EMIT failed(tr("Arşiv yazılamadı: %1").arg(f.errorString()));
            return;
        }
        f.write(reply->readAll());
        f.close();
        Q_EMIT statusMessage(tr("Kuruluyor…"));
        auto *tar = new QProcess(this);
        connect(tar, &QProcess::finished, this, [this, tar, archive](int code) {
            tar->deleteLater();
            QFile::remove(archive);
            if (code != 0 || !QFile::exists(binaryPath())) {
                Q_EMIT failed(tr("Orbital Cloud kurulamadı: %1").arg(QString::fromUtf8(tar->readAllStandardError())));
                return;
            }
            QFile::setPermissions(binaryPath(), QFile::permissions(binaryPath()) | QFile::ExeOwner | QFile::ExeUser);
            launch();
        });
        tar->start(QStringLiteral("tar"), {QStringLiteral("xzf"), archive, QStringLiteral("-C"), targetDir});
    });
}

void OpencodeEngine::buildOverlay(std::function<void(const QByteArray &)> done)
{
    // Providers: translate Orbital's provider list into opencode's format.
    const QList<ProviderConfig> providers = Config::providers();
    auto providerJson = std::make_shared<QJsonObject>();
    auto remaining = std::make_shared<int>(providers.size() + 1);

    auto finish = [this, providerJson, done] {
        QJsonObject overlay{{QStringLiteral("$schema"), QStringLiteral("https://opencode.ai/config.json")},
                            {QStringLiteral("autoupdate"), false},
                            {QStringLiteral("share"), QStringLiteral("disabled")}};
        if (!providerJson->isEmpty())
            overlay.insert(QStringLiteral("provider"), *providerJson);
        const QString model = Config::defaultModel();
        if (providerJson->contains(model.section(QLatin1Char('/'), 0, 0)))
            overlay.insert(QStringLiteral("model"), model);

        QJsonArray skillPaths;
        for (const Skill &s : m_skills->skills())
            if (s.enabled)
                skillPaths << s.dir;
        if (!skillPaths.isEmpty())
            overlay.insert(QStringLiteral("skills"), QJsonObject{{QStringLiteral("paths"), skillPaths}});

        QJsonObject commands;
        const QJsonObject pluginCommands = m_plugins->commands();
        for (auto it = pluginCommands.begin(); it != pluginCommands.end(); ++it) {
            const QJsonObject src = it.value().toObject();
            QJsonObject cmd{{QStringLiteral("template"), src.value(QStringLiteral("template"))}};
            for (const char *key : {"description", "agent", "model", "subtask"})
                if (src.contains(QLatin1String(key)))
                    cmd.insert(QLatin1String(key), src.value(QLatin1String(key)));
            commands.insert(it.key(), cmd);
        }
        if (!commands.isEmpty())
            overlay.insert(QStringLiteral("command"), commands);

        QJsonObject mcp;
        const QJsonObject servers = m_plugins->mcpServers();
        for (auto it = servers.begin(); it != servers.end(); ++it) {
            const QJsonObject s = it.value().toObject();
            QJsonObject out{{QStringLiteral("type"), s.value(QStringLiteral("type"))}, {QStringLiteral("enabled"), true}};
            for (const char *key : {"command", "environment", "url", "headers", "cwd"})
                if (s.contains(QLatin1String(key)))
                    out.insert(QLatin1String(key), s.value(QLatin1String(key)));
            mcp.insert(it.key(), out);
        }
        if (!mcp.isEmpty())
            overlay.insert(QStringLiteral("mcp"), mcp);

        const QStringList instructions = m_plugins->instructionFiles();
        if (!instructions.isEmpty())
            overlay.insert(QStringLiteral("instructions"), QJsonArray::fromStringList(instructions));
        // Orbital's per-tool permission settings, in the engine's vocabulary.
        QJsonObject perms = Config::raw().value(QStringLiteral("permissions")).toObject();
        if (m_plugins) {
            const QJsonObject pluginPerms = m_plugins->permissions();
            for (auto it = pluginPerms.begin(); it != pluginPerms.end(); ++it)
                perms.insert(it.key(), it.value());
        }
        if (!perms.isEmpty()) {
            const auto policy = [&perms](const QString &tool) {
                const QString v = perms.value(tool).toString(perms.value(QStringLiteral("*")).toString(QStringLiteral("ask")));
                return v == QLatin1String("allow") || v == QLatin1String("deny") ? v : QStringLiteral("ask");
            };
            const auto strictest = [](const QString &a, const QString &b) {
                if (a == QLatin1String("deny") || b == QLatin1String("deny"))
                    return QStringLiteral("deny");
                if (a == QLatin1String("ask") || b == QLatin1String("ask"))
                    return QStringLiteral("ask");
                return QStringLiteral("allow");
            };
            QJsonObject permission;
            for (const char *tool : {"read", "list", "glob", "grep", "skill", "bash"})
                permission.insert(QLatin1String(tool), policy(QLatin1String(tool)));
            permission.insert(QStringLiteral("edit"), strictest(policy(QStringLiteral("write")), policy(QStringLiteral("edit"))));
            permission.insert(QStringLiteral("webfetch"), policy(QStringLiteral("*")));
            permission.insert(QStringLiteral("websearch"), policy(QStringLiteral("*")));
            overlay.insert(QStringLiteral("permission"), permission);
        }

        const QStringList plugins = m_plugins->opencodePlugins();
        if (!plugins.isEmpty())
            overlay.insert(QStringLiteral("plugin"), QJsonArray::fromStringList(plugins));
        done(QJsonDocument(overlay).toJson(QJsonDocument::Compact));
    };
    auto tick = [remaining, finish] {
        if (--*remaining == 0)
            finish();
    };

    for (const ProviderConfig &p : providers) {
        const QString key = p.resolvedApiKey();
        if (p.type == QLatin1String("anthropic")) {
            if (!key.isEmpty()) {
                QJsonObject entry{{QStringLiteral("options"), QJsonObject{{QStringLiteral("apiKey"), key}}}};
                if (p.id != QLatin1String("anthropic")) {
                    entry.insert(QStringLiteral("npm"), QStringLiteral("@ai-sdk/anthropic"));
                    entry.insert(QStringLiteral("name"), p.name);
                    QJsonObject models;
                    for (const QString &m : p.models)
                        models.insert(m, QJsonObject{{QStringLiteral("name"), m}});
                    entry.insert(QStringLiteral("models"), models);
                }
                providerJson->insert(p.id, entry);
            }
            tick();
            continue;
        }
        fetchModels(&m_nam, p, [p, key, providerJson, tick](const QStringList &list) {
            if (!list.isEmpty()) {
                QJsonObject models;
                for (const QString &m : list)
                    models.insert(m, QJsonObject{{QStringLiteral("name"), m}, {QStringLiteral("tool_call"), true}});
                QJsonObject options{{QStringLiteral("baseURL"), p.baseUrl}};
                if (!key.isEmpty())
                    options.insert(QStringLiteral("apiKey"), key);
                providerJson->insert(p.id, QJsonObject{{QStringLiteral("name"), p.name},
                                                       {QStringLiteral("npm"), QStringLiteral("@ai-sdk/openai-compatible")},
                                                       {QStringLiteral("options"), options},
                                                       {QStringLiteral("models"), models}});
            }
            tick();
        });
    }
    tick();
}

void OpencodeEngine::launch()
{
    Q_EMIT statusMessage(tr("Hazırlanıyor…"));
    buildOverlay([this](const QByteArray &overlay) {
        if (m_stopping)
            return;
        QTcpServer probe;
        probe.listen(QHostAddress::LocalHost, 0);
        const int port = probe.serverPort();
        probe.close();

        QByteArray password;
        for (int i = 0; i < 32; ++i)
            password += "abcdefghijklmnopqrstuvwxyz0123456789"[QRandomGenerator::global()->bounded(36)];
        m_baseUrl = QStringLiteral("http://127.0.0.1:%1").arg(port);
        m_auth = "Basic " + QByteArray("opencode:" + password).toBase64();

        const QString home = homeDir();
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        // Isolate from any opencode the user has installed: own config, data, state and cache.
        env.insert(QStringLiteral("XDG_CONFIG_HOME"), home + QStringLiteral("/config"));
        env.insert(QStringLiteral("XDG_DATA_HOME"), home + QStringLiteral("/data"));
        env.insert(QStringLiteral("XDG_STATE_HOME"), home + QStringLiteral("/state"));
        env.insert(QStringLiteral("XDG_CACHE_HOME"), home + QStringLiteral("/cache"));
        env.insert(QStringLiteral("OPENCODE_DISABLE_PROJECT_CONFIG"), QStringLiteral("1"));
        env.insert(QStringLiteral("OPENCODE_DISABLE_AUTOUPDATE"), QStringLiteral("1"));
        env.insert(QStringLiteral("OPENCODE_SERVER_PASSWORD"), QString::fromLatin1(password));
        env.insert(QStringLiteral("OPENCODE_CONFIG_CONTENT"), QString::fromUtf8(overlay));
        env.remove(QStringLiteral("OPENCODE_CONFIG"));
        env.remove(QStringLiteral("OPENCODE_CONFIG_DIR"));

        m_stdout.clear();
        m_proc = new QProcess(this);
        m_proc->setProcessChannelMode(QProcess::MergedChannels);
        m_proc->setWorkingDirectory(m_directory);
        m_proc->setProcessEnvironment(env);
#ifdef Q_OS_LINUX
        // If Orbital dies without a clean shutdown, take the engine down with it.
        m_proc->setChildProcessModifier([] { ::prctl(PR_SET_PDEATHSIG, SIGTERM); });
#endif
        connect(m_proc, &QProcess::readyRead, this, [this] {
            m_stdout += m_proc->readAll();
            if (!m_ready && m_stdout.contains("listening on")) {
                m_ready = true;
                onServerReady();
            }
            if (m_stdout.size() > 65536)
                m_stdout = m_stdout.right(16384);
        });
        connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart)
                Q_EMIT failed(tr("Orbital Cloud başlatılamadı: %1").arg(binaryPath()));
        });
        connect(m_proc, &QProcess::finished, this, [this](int code) {
            if (m_stopping)
                return;
            m_ready = false;
            Q_EMIT failed(tr("Orbital Cloud durdu (kod %1).\n%2").arg(code).arg(QString::fromUtf8(m_stdout.right(1500))));
        });
        m_proc->start(binaryPath(), {QStringLiteral("serve"), QStringLiteral("--hostname"), QStringLiteral("127.0.0.1"),
                                     QStringLiteral("--port"), QString::number(port)});
    });
}

void OpencodeEngine::onServerReady()
{
    call("GET", QStringLiteral("/agent"), {}, [this](const QJsonValue &data, const QString &) {
        m_agents.clear();
        for (const QJsonValue &v : data.toArray()) {
            const QJsonObject a = v.toObject();
            const QString mode = a.value(QStringLiteral("mode")).toString();
            if (a.value(QStringLiteral("hidden")).toBool() || mode == QLatin1String("subagent"))
                continue;
            m_agents << AgentInfo{a.value(QStringLiteral("name")).toString(), a.value(QStringLiteral("description")).toString()};
        }
        startEvents();
        Q_EMIT statusMessage(QString());
        Q_EMIT ready();
    });
}

void OpencodeEngine::stop()
{
    m_stopping = true;
    m_ready = false;
    if (m_events) {
        m_events->disconnect(this);
        m_events->abort();
        m_events->deleteLater();
    }
    if (m_proc) {
        m_proc->disconnect(this);
        m_proc->terminate();
        if (!m_proc->waitForFinished(3000))
            m_proc->kill();
        m_proc->deleteLater();
    }
    m_userMessages.clear();
    m_reasoningParts.clear();
    m_partSession.clear();
}

// ---------------------------------------------------------------- HTTP

QNetworkRequest OpencodeEngine::request(const QString &path) const
{
    QNetworkRequest req(QUrl(m_baseUrl + path));
    req.setRawHeader("Authorization", m_auth);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(0);
    return req;
}

void OpencodeEngine::call(const QByteArray &verb, const QString &path, const QJsonValue &body, Callback cb)
{
    if (!m_ready) {
        if (cb)
            cb({}, tr("Motor hazır değil."));
        return;
    }
    QNetworkReply *reply = m_nam.sendCustomRequest(request(path), verb, body.isNull() || body.isUndefined() ? QByteArray() : toJson(body));
    connect(reply, &QNetworkReply::finished, this, [reply, cb] {
        reply->deleteLater();
        const QByteArray raw = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        QString error;
        if (reply->error() != QNetworkReply::NoError) {
            const QJsonObject o = doc.object();
            error = o.value(QStringLiteral("message")).toString(
                o.value(QStringLiteral("data")).toObject().value(QStringLiteral("message")).toString());
            if (error.isEmpty())
                error = QString::fromUtf8(raw).left(300);
            if (error.isEmpty())
                error = reply->errorString();
        }
        if (cb)
            cb(doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object()), error);
    });
}

void OpencodeEngine::startEvents()
{
    if (m_stopping)
        return;
    QNetworkRequest req = request(QStringLiteral("/event"));
    req.setRawHeader("Accept", "text/event-stream");
    m_eventBuffer.clear();
    m_events = m_nam.get(req);
    connect(m_events, &QNetworkReply::readyRead, this, [this] {
        m_eventBuffer += m_events->readAll();
        parseEvents();
    });
    connect(m_events, &QNetworkReply::finished, this, [this] {
        if (m_events)
            m_events->deleteLater();
        if (!m_stopping && m_ready)
            QTimer::singleShot(1000, this, &OpencodeEngine::startEvents);
    });
}

void OpencodeEngine::parseEvents()
{
    m_eventBuffer.replace("\r\n", "\n");
    int sep;
    while ((sep = m_eventBuffer.indexOf("\n\n")) >= 0) {
        const QByteArray chunk = m_eventBuffer.left(sep);
        m_eventBuffer.remove(0, sep + 2);
        QByteArray payload;
        for (const QByteArray &line : chunk.split('\n'))
            if (line.startsWith("data:"))
                payload += line.mid(5).trimmed();
        if (payload.isEmpty())
            continue;
        const QJsonObject e = QJsonDocument::fromJson(payload).object();
        QJsonObject props = e.value(QStringLiteral("properties")).toObject();
        if (props.isEmpty() && e.contains(QStringLiteral("data")))
            props = e.value(QStringLiteral("data")).toObject();
        handleEvent(e.value(QStringLiteral("type")).toString(), props);
    }
}

static QString errorText(const QJsonObject &error)
{
    const QString msg = error.value(QStringLiteral("data")).toObject().value(QStringLiteral("message")).toString();
    return msg.isEmpty() ? error.value(QStringLiteral("name")).toString() : msg;
}

void OpencodeEngine::handleEvent(const QString &type, const QJsonObject &p)
{
    const QString sid = p.value(QStringLiteral("sessionID")).toString();
    if (type == QLatin1String("message.updated")) {
        const QJsonObject info = p.value(QStringLiteral("info")).toObject();
        const QString msgSid = info.value(QStringLiteral("sessionID")).toString();
        if (info.value(QStringLiteral("role")).toString() == QLatin1String("user")) {
            m_userMessages.insert(info.value(QStringLiteral("id")).toString());
            return;
        }
        if (info.contains(QStringLiteral("error")) && info.value(QStringLiteral("error")).toObject().value(QStringLiteral("name")).toString()
                                                         != QLatin1String("MessageAbortedError"))
            Q_EMIT errorOccurred(msgSid, errorText(info.value(QStringLiteral("error")).toObject()));
        if (info.value(QStringLiteral("time")).toObject().contains(QStringLiteral("completed"))) {
            const QJsonObject t = info.value(QStringLiteral("tokens")).toObject();
            Q_EMIT usageUpdated(msgSid, t.value(QStringLiteral("input")).toInt() + t.value(QStringLiteral("cache")).toObject().value(QStringLiteral("read")).toInt(),
                              t.value(QStringLiteral("output")).toInt(), info.value(QStringLiteral("cost")).toDouble());
        }
    } else if (type == QLatin1String("message.part.updated")) {
        const QJsonObject part = p.value(QStringLiteral("part")).toObject();
        const QString partSid = part.value(QStringLiteral("sessionID")).toString();
        if (m_userMessages.contains(part.value(QStringLiteral("messageID")).toString()))
            return;
        const QString kind = part.value(QStringLiteral("type")).toString();
        const QString id = part.value(QStringLiteral("id")).toString();
        if (kind == QLatin1String("text") && !part.value(QStringLiteral("synthetic")).toBool()) {
            Q_EMIT textSet(partSid, id, part.value(QStringLiteral("text")).toString());
        } else if (kind == QLatin1String("reasoning")) {
            m_reasoningParts.insert(id);
        } else if (kind == QLatin1String("tool")) {
            const QJsonObject state = part.value(QStringLiteral("state")).toObject();
            const QString status = state.value(QStringLiteral("status")).toString();
            QString output = state.value(QStringLiteral("output")).toString();
            if (status == QLatin1String("error"))
                output = state.value(QStringLiteral("error")).toString();
            Q_EMIT toolUpdate(partSid, part.value(QStringLiteral("callID")).toString(), part.value(QStringLiteral("tool")).toString(),
                            state.value(QStringLiteral("input")).toObject(), status, output);
        }
    } else if (type == QLatin1String("message.part.delta")) {
        if (m_userMessages.contains(p.value(QStringLiteral("messageID")).toString()))
            return;
        if (p.value(QStringLiteral("field")).toString() != QLatin1String("text"))
            return;
        const QString partId = p.value(QStringLiteral("partID")).toString();
        const QString delta = p.value(QStringLiteral("delta")).toString();
        if (m_reasoningParts.contains(partId))
            Q_EMIT reasoningDelta(sid, partId, delta);
        else
            Q_EMIT textDelta(sid, partId, delta);
    } else if (type == QLatin1String("session.status")) {
        Q_EMIT busyChanged(sid, p.value(QStringLiteral("status")).toObject().value(QStringLiteral("type")).toString() != QLatin1String("idle"));
    } else if (type == QLatin1String("session.idle")) {
        Q_EMIT busyChanged(sid, false);
    } else if (type == QLatin1String("session.error")) {
        const QJsonObject err = p.value(QStringLiteral("error")).toObject();
        if (err.value(QStringLiteral("name")).toString() != QLatin1String("MessageAbortedError"))
            Q_EMIT errorOccurred(sid, errorText(err));
    } else if (type == QLatin1String("permission.asked")) {
        QStringList detail;
        for (const QJsonValue &v : p.value(QStringLiteral("patterns")).toArray())
            detail << v.toString();
        Q_EMIT permissionRequested(PermissionRequest{sid, p.value(QStringLiteral("id")).toString(),
                                                   p.value(QStringLiteral("permission")).toString(), detail.join(QStringLiteral(", "))});
    } else if (type == QLatin1String("question.asked")) {
        const QString qid = p.value(QStringLiteral("id")).toString();
        const QJsonArray questions = p.value(QStringLiteral("questions")).toArray();
        Q_EMIT questionRequested(QuestionRequest{sid, qid, questions});
    } else if (type == QLatin1String("question.replied") || type == QLatin1String("question.rejected")) {
        const QString qid = p.contains(QStringLiteral("requestID")) ? p.value(QStringLiteral("requestID")).toString()
                                                                    : p.value(QStringLiteral("id")).toString();
        Q_EMIT questionDismissed(qid);
    } else if (type == QLatin1String("session.created") || type == QLatin1String("session.deleted")
               || (type == QLatin1String("session.updated"))) {
        Q_EMIT sessionsChanged();
    }
}

// ---------------------------------------------------------------- Engine API

void OpencodeEngine::listSessions(std::function<void(const QList<SessionInfo> &)> done)
{
    call("GET", QStringLiteral("/session"), {}, [done](const QJsonValue &data, const QString &) {
        QList<SessionInfo> out;
        for (const QJsonValue &v : data.toArray()) {
            const QJsonObject s = v.toObject();
            if (s.contains(QStringLiteral("parentID")))
                continue;
            out << SessionInfo{s.value(QStringLiteral("id")).toString(), s.value(QStringLiteral("title")).toString(),
                               qint64(s.value(QStringLiteral("time")).toObject().value(QStringLiteral("updated")).toDouble()),
                               s.value(QStringLiteral("directory")).toString()};
        }
        std::sort(out.begin(), out.end(), [](const SessionInfo &a, const SessionInfo &b) { return a.updated > b.updated; });
        done(out);
    });
}

void OpencodeEngine::createSession(std::function<void(const QString &, const QString &)> done)
{
    call("POST", QStringLiteral("/session"), QJsonObject{}, [done](const QJsonValue &data, const QString &error) {
        done(data.toObject().value(QStringLiteral("id")).toString(), error);
    });
}

void OpencodeEngine::deleteSession(const QString &id, Done done)
{
    call("DELETE", QStringLiteral("/session/") + id, {}, [done](const QJsonValue &, const QString &error) { done(error); });
}

void OpencodeEngine::renameSession(const QString &id, const QString &title, Done done)
{
    call("PATCH", QStringLiteral("/session/") + id, QJsonObject{{QStringLiteral("title"), title}},
         [this, done](const QJsonValue &, const QString &error) {
             if (error.isEmpty())
                 Q_EMIT sessionsChanged();
             if (done)
                 done(error);
         });
}

void OpencodeEngine::loadHistory(const QString &id, std::function<void(const QJsonArray &)> done)
{
    call("GET", QStringLiteral("/session/%1/message").arg(id), {}, [done](const QJsonValue &data, const QString &) {
        QJsonArray out;
        for (const QJsonValue &v : data.toArray()) {
            const QJsonObject info = v.toObject().value(QStringLiteral("info")).toObject();
            const QJsonArray parts = v.toObject().value(QStringLiteral("parts")).toArray();
            if (info.value(QStringLiteral("role")).toString() == QLatin1String("user")) {
                QStringList texts;
                for (const QJsonValue &pv : parts) {
                    const QJsonObject part = pv.toObject();
                    if (part.value(QStringLiteral("type")).toString() == QLatin1String("text") && !part.value(QStringLiteral("synthetic")).toBool())
                        texts << part.value(QStringLiteral("text")).toString();
                }
                out << QJsonObject{{QStringLiteral("role"), QStringLiteral("user")}, {QStringLiteral("text"), texts.join(QLatin1Char('\n'))}};
                continue;
            }
            QJsonArray items;
            for (const QJsonValue &pv : parts) {
                const QJsonObject part = pv.toObject();
                const QString kind = part.value(QStringLiteral("type")).toString();
                if (kind == QLatin1String("text") || kind == QLatin1String("reasoning")) {
                    items << QJsonObject{{QStringLiteral("type"), kind}, {QStringLiteral("text"), part.value(QStringLiteral("text"))}};
                } else if (kind == QLatin1String("tool")) {
                    const QJsonObject state = part.value(QStringLiteral("state")).toObject();
                    items << QJsonObject{{QStringLiteral("type"), QStringLiteral("tool")},
                                         {QStringLiteral("id"), part.value(QStringLiteral("callID"))},
                                         {QStringLiteral("name"), part.value(QStringLiteral("tool"))},
                                         {QStringLiteral("input"), state.value(QStringLiteral("input"))},
                                         {QStringLiteral("status"), state.value(QStringLiteral("status"))},
                                         {QStringLiteral("output"), state.contains(QStringLiteral("error")) ? state.value(QStringLiteral("error"))
                                                                                                          : state.value(QStringLiteral("output"))}};
                }
            }
            QJsonObject item{{QStringLiteral("role"), QStringLiteral("assistant")}, {QStringLiteral("parts"), items}};
            const QJsonObject err = info.value(QStringLiteral("error")).toObject();
            if (!err.isEmpty() && err.value(QStringLiteral("name")).toString() != QLatin1String("MessageAbortedError"))
                item.insert(QStringLiteral("error"), errorText(err));
            out << item;
        }
        done(out);
    });
}

void OpencodeEngine::listModels(std::function<void(const QList<ModelChoice> &)> done)
{
    call("GET", QStringLiteral("/config/providers"), {}, [done](const QJsonValue &data, const QString &) {
        QList<ModelChoice> out;
        for (const QJsonValue &v : data.toObject().value(QStringLiteral("providers")).toArray()) {
            const QJsonObject p = v.toObject();
            const QString pid = p.value(QStringLiteral("id")).toString();
            const QString pname = p.value(QStringLiteral("name")).toString(pid);
            const QJsonObject models = p.value(QStringLiteral("models")).toObject();
            for (auto it = models.begin(); it != models.end(); ++it) {
                const QString name = it.value().toObject().value(QStringLiteral("name")).toString(it.key());
                out << ModelChoice{pid, pname, it.key(), QStringLiteral("%1 · %2").arg(pname, name), name};
            }
        }
        done(out);
    });
}

void OpencodeEngine::listCommands(std::function<void(const QList<CommandDef> &)> done)
{
    call("GET", QStringLiteral("/command"), {}, [this, done](const QJsonValue &data, const QString &) {
        const QJsonObject pluginCommands = m_plugins->commands();
        QList<CommandDef> out;
        for (const QJsonValue &v : data.toArray()) {
            const QJsonObject c = v.toObject();
            const QString name = c.value(QStringLiteral("name")).toString();
            if (c.value(QStringLiteral("source")).toString() == QLatin1String("skill"))
                continue; // skills are offered by Orbital's own skill list
            out << CommandDef{name, c.value(QStringLiteral("description")).toString(),
                              pluginCommands.contains(name) ? QStringLiteral("plugin") : QStringLiteral("engine")};
        }
        done(out);
    });
}

static QJsonObject modelObject(const QString &ref)
{
    const int slash = ref.indexOf(QLatin1Char('/'));
    if (slash < 0)
        return {};
    return QJsonObject{{QStringLiteral("providerID"), ref.left(slash)}, {QStringLiteral("modelID"), ref.mid(slash + 1)}};
}

void OpencodeEngine::send(const QString &sessionId, const QString &text, const QStringList &skills, const QString &modelRef,
                          const QString &agent)
{
    QJsonArray parts;
    for (const QString &name : skills) {
        if (const Skill *s = m_skills->find(name))
            parts << QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                                 {QStringLiteral("synthetic"), true},
                                 {QStringLiteral("text"), QStringLiteral("<skill name=\"%1\">\n%2\n</skill>").arg(s->name, s->body)}};
    }
    parts << QJsonObject{{QStringLiteral("type"), QStringLiteral("text")}, {QStringLiteral("text"), text}};
    QJsonObject body{{QStringLiteral("parts"), parts}};
    const QJsonObject model = modelObject(modelRef);
    if (!model.isEmpty())
        body.insert(QStringLiteral("model"), model);
    if (!agent.isEmpty())
        body.insert(QStringLiteral("agent"), agent);
    Q_EMIT busyChanged(sessionId, true);
    call("POST", QStringLiteral("/session/%1/prompt_async").arg(sessionId), body, [this, sessionId](const QJsonValue &, const QString &error) {
        if (!error.isEmpty()) {
            Q_EMIT errorOccurred(sessionId, error);
            Q_EMIT busyChanged(sessionId, false);
        }
    });
}

void OpencodeEngine::runCommand(const QString &sessionId, const QString &command, const QString &arguments,
                                const QString &modelRef, const QString &agent)
{
    QJsonObject body{{QStringLiteral("command"), command}, {QStringLiteral("arguments"), arguments}};
    if (!modelRef.isEmpty())
        body.insert(QStringLiteral("model"), modelRef);
    if (!agent.isEmpty())
        body.insert(QStringLiteral("agent"), agent);
    Q_EMIT busyChanged(sessionId, true);
    call("POST", QStringLiteral("/session/%1/command").arg(sessionId), body, [this, sessionId](const QJsonValue &, const QString &error) {
        if (!error.isEmpty()) {
            Q_EMIT errorOccurred(sessionId, error);
            Q_EMIT busyChanged(sessionId, false);
        }
    });
}

void OpencodeEngine::abort(const QString &sessionId)
{
    call("POST", QStringLiteral("/session/%1/abort").arg(sessionId), QJsonObject{}, {});
}

void OpencodeEngine::replyPermission(const PermissionRequest &request, const QString &reply)
{
    call("POST", QStringLiteral("/permission/%1/reply").arg(request.id), QJsonObject{{QStringLiteral("reply"), reply}}, {});
}

void OpencodeEngine::replyQuestion(const QString &requestId, const QJsonArray &answers)
{
    call("POST", QStringLiteral("/question/%1/reply").arg(requestId), QJsonObject{{QStringLiteral("answers"), answers}}, {});
}

void OpencodeEngine::rejectQuestion(const QString &requestId)
{
    call("POST", QStringLiteral("/question/%1/reject").arg(requestId), QJsonObject{}, {});
}

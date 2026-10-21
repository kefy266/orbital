#include "nativeengine.h"

#include "config.h"
#include "llm.h"
#include "paths.h"
#include "pluginmanager.h"
#include "skillmanager.h"
#include "tools.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QRegularExpression>
#include <QSysInfo>
#include <QUuid>
#include <algorithm>
#include <memory>

NativeEngine::NativeEngine(SkillManager *skills, PluginManager *plugins, QObject *parent)
    : Engine(parent)
    , m_skills(skills)
    , m_plugins(plugins)
    , m_nam(new QNetworkAccessManager(this))
    , m_tools(new ToolRegistry(skills, plugins, m_nam, this))
{
}

void NativeEngine::start(const QString &directory)
{
    m_directory = directory;
    m_tools->reloadExtensions();
    Q_EMIT ready();
}

void NativeEngine::stop()
{
    const QStringList ids = m_runs.keys();
    for (const QString &id : ids)
        abort(id);
    m_tools->abortRunning();
}

// ---------------------------------------------------------------- sessions

QString NativeEngine::sessionPath(const QString &id) const
{
    QDir dir(Paths::dataDir() + QStringLiteral("/sessions"));
    dir.mkpath(QStringLiteral("."));
    return dir.filePath(id + QStringLiteral(".json"));
}

QJsonObject NativeEngine::loadSession(const QString &id) const
{
    QFile f(sessionPath(id));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

void NativeEngine::saveSession(QJsonObject session) const
{
    session.insert(QStringLiteral("updated"), QDateTime::currentMSecsSinceEpoch());
    QFile f(sessionPath(session.value(QStringLiteral("id")).toString()));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(session).toJson(QJsonDocument::Compact));
}

void NativeEngine::listSessions(std::function<void(const QList<SessionInfo> &)> done)
{
    QList<SessionInfo> out;
    QDir dir(Paths::dataDir() + QStringLiteral("/sessions"));
    for (const QFileInfo &fi : dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files)) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject s = QJsonDocument::fromJson(f.readAll()).object();
        if (s.value(QStringLiteral("directory")).toString() != m_directory)
            continue;
        out << SessionInfo{s.value(QStringLiteral("id")).toString(), s.value(QStringLiteral("title")).toString(),
                           qint64(s.value(QStringLiteral("updated")).toDouble())};
    }
    std::sort(out.begin(), out.end(), [](const SessionInfo &a, const SessionInfo &b) { return a.updated > b.updated; });
    done(out);
}

void NativeEngine::createSession(std::function<void(const QString &, const QString &)> done)
{
    const QString id = QStringLiteral("orb_") + QUuid::createUuid().toString(QUuid::Id128).left(20);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    saveSession(QJsonObject{{QStringLiteral("id"), id},
                            {QStringLiteral("title"), QString()},
                            {QStringLiteral("directory"), m_directory},
                            {QStringLiteral("created"), now},
                            {QStringLiteral("messages"), QJsonArray{}}});
    Q_EMIT sessionsChanged();
    done(id, {});
}

void NativeEngine::deleteSession(const QString &id, Done done)
{
    abort(id);
    QFile::remove(sessionPath(id));
    Q_EMIT sessionsChanged();
    done({});
}

void NativeEngine::loadHistory(const QString &id, std::function<void(const QJsonArray &)> done)
{
    const QJsonArray messages = loadSession(id).value(QStringLiteral("messages")).toArray();
    QHash<QString, QJsonObject> results;
    for (const QJsonValue &v : messages) {
        const QJsonObject m = v.toObject();
        if (m.value(QStringLiteral("role")).toString() == QLatin1String("tool"))
            results.insert(m.value(QStringLiteral("tool_call_id")).toString(), m);
    }
    QJsonArray out;
    for (const QJsonValue &v : messages) {
        const QJsonObject m = v.toObject();
        const QString role = m.value(QStringLiteral("role")).toString();
        if (role == QLatin1String("user")) {
            out << QJsonObject{{QStringLiteral("role"), role},
                               {QStringLiteral("text"), m.value(QStringLiteral("display")).toString(m.value(QStringLiteral("content")).toString())},
                               {QStringLiteral("skills"), m.value(QStringLiteral("skills"))}};
        } else if (role == QLatin1String("assistant")) {
            QJsonArray parts;
            if (!m.value(QStringLiteral("reasoning")).toString().isEmpty())
                parts << QJsonObject{{QStringLiteral("type"), QStringLiteral("reasoning")}, {QStringLiteral("text"), m.value(QStringLiteral("reasoning"))}};
            if (!m.value(QStringLiteral("content")).toString().isEmpty())
                parts << QJsonObject{{QStringLiteral("type"), QStringLiteral("text")}, {QStringLiteral("text"), m.value(QStringLiteral("content"))}};
            for (const QJsonValue &c : m.value(QStringLiteral("tool_calls")).toArray()) {
                const QJsonObject call = c.toObject();
                const QJsonObject res = results.value(call.value(QStringLiteral("id")).toString());
                parts << QJsonObject{{QStringLiteral("type"), QStringLiteral("tool")},
                                     {QStringLiteral("id"), call.value(QStringLiteral("id"))},
                                     {QStringLiteral("name"), call.value(QStringLiteral("name"))},
                                     {QStringLiteral("input"), call.value(QStringLiteral("arguments"))},
                                     {QStringLiteral("status"), res.isEmpty() ? QStringLiteral("error")
                                                              : res.value(QStringLiteral("is_error")).toBool() ? QStringLiteral("error")
                                                                                                               : QStringLiteral("completed")},
                                     {QStringLiteral("output"), res.value(QStringLiteral("content"))}};
            }
            QJsonObject item{{QStringLiteral("role"), role}, {QStringLiteral("parts"), parts}};
            if (m.contains(QStringLiteral("error")))
                item.insert(QStringLiteral("error"), m.value(QStringLiteral("error")));
            out << item;
        } else if (role == QLatin1String("notice")) {
            out << QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")},
                               {QStringLiteral("parts"), QJsonArray{}},
                               {QStringLiteral("error"), m.value(QStringLiteral("content"))}};
        }
    }
    done(out);
}

// ---------------------------------------------------------------- catalog

void NativeEngine::listModels(std::function<void(const QList<ModelChoice> &)> done)
{
    const QList<ProviderConfig> providers = Config::providers();
    auto results = std::make_shared<QList<ModelChoice>>();
    auto remaining = std::make_shared<int>(providers.size());
    if (providers.isEmpty()) {
        done({});
        return;
    }
    for (const ProviderConfig &p : providers) {
        fetchModels(m_nam, p, [p, results, remaining, done](const QStringList &models) {
            for (const QString &m : models)
                results->append(ModelChoice{p.id, p.name, m, QStringLiteral("%1 · %2").arg(p.name, m), m});
            if (--*remaining == 0)
                done(*results);
        });
    }
}

QList<AgentInfo> NativeEngine::agents() const
{
    return {AgentInfo{QStringLiteral("build"), tr("Tüm araçlar: dosya yazar, düzenler, komut çalıştırır")},
            AgentInfo{QStringLiteral("plan"), tr("Salt okunur: inceler ve plan çıkarır")}};
}

static QJsonObject builtinCommands()
{
    return QJsonObject{
        {QStringLiteral("init"), QJsonObject{
             {QStringLiteral("description"), QObject::tr("Proje için AGENTS.md oluştur")},
             {QStringLiteral("template"), QStringLiteral("Explore this repository and create or update AGENTS.md at its root: "
                                                         "build/test commands, architecture overview, code style rules and "
                                                         "gotchas a coding agent should know. Keep it compact. $ARGUMENTS")}}},
        {QStringLiteral("review"), QJsonObject{
             {QStringLiteral("description"), QObject::tr("Commit edilmemiş değişiklikleri incele")},
             {QStringLiteral("template"), QStringLiteral("Review the uncommitted changes (use `git diff` and `git status`). "
                                                         "Report bugs, risky changes and missing tests, most severe first. $ARGUMENTS")}}},
    };
}

void NativeEngine::listCommands(std::function<void(const QList<CommandDef> &)> done)
{
    QList<CommandDef> out;
    const QJsonObject builtin = builtinCommands();
    for (auto it = builtin.begin(); it != builtin.end(); ++it)
        out << CommandDef{it.key(), it.value().toObject().value(QStringLiteral("description")).toString(), QStringLiteral("engine")};
    const QJsonObject plugin = m_plugins->commands();
    for (auto it = plugin.begin(); it != plugin.end(); ++it)
        out << CommandDef{it.key(), it.value().toObject().value(QStringLiteral("description")).toString(), QStringLiteral("plugin")};
    done(out);
}

// ---------------------------------------------------------------- prompting

QString NativeEngine::systemPrompt(const QString &agent) const
{
    QString s = QStringLiteral(
        "You are Orbital, an AI coding assistant running on the user's computer. You help with software engineering "
        "tasks using the tools provided. Be concise and direct. Prefer doing the work with tools over describing it. "
        "Read files before editing them. When a task is done, summarize what changed in a sentence or two.\n\n");
    if (agent == QLatin1String("plan"))
        s += QStringLiteral("You are in PLAN mode: you may only read and search. Investigate and produce a clear, "
                            "step-by-step plan; do not attempt to modify files.\n\n");
    s += QStringLiteral("<env>\nworking directory: %1\nplatform: %2\ndate: %3\n</env>\n")
             .arg(m_directory, QSysInfo::prettyProductName(), QDate::currentDate().toString(Qt::ISODate));

    for (const QString &name : {QStringLiteral("AGENTS.md"), QStringLiteral("ORBITAL.md"), QStringLiteral("CLAUDE.md")}) {
        QFile f(QDir(m_directory).filePath(name));
        if (f.open(QIODevice::ReadOnly)) {
            s += QStringLiteral("\n<project_instructions file=\"%1\">\n%2\n</project_instructions>\n")
                     .arg(name, QString::fromUtf8(f.read(40000)));
            break;
        }
    }
    for (const QString &path : m_plugins->instructionFiles()) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            s += QStringLiteral("\n<instructions source=\"%1\">\n%2\n</instructions>\n").arg(QFileInfo(path).fileName(),
                                                                                           QString::fromUtf8(f.read(40000)));
    }

    QStringList catalog;
    for (const Skill &sk : m_skills->skills())
        if (sk.enabled)
            catalog << QStringLiteral("- %1: %2").arg(sk.name, sk.description);
    if (!catalog.isEmpty())
        s += QStringLiteral("\n<available_skills>\nSkills are expert instruction packs. When a task matches a skill's "
                            "description, call the `skill` tool with its name first and follow what it says.\n%1\n</available_skills>\n")
                 .arg(catalog.join(QLatin1Char('\n')));
    return s;
}

NativeEngine::Run *NativeEngine::liveRun(const QString &sessionId, quint64 serial) const
{
    Run *run = m_runs.value(sessionId);
    return run && run->serial == serial && !run->aborted ? run : nullptr;
}

void NativeEngine::send(const QString &sessionId, const QString &text, const QStringList &skills, const QString &modelRef,
                        const QString &agent)
{
    if (m_runs.contains(sessionId))
        return;
    QJsonObject session = loadSession(sessionId);
    if (session.isEmpty()) {
        Q_EMIT errorOccurred(sessionId, tr("Oturum bulunamadı."));
        return;
    }

    QString content;
    for (const QString &name : skills) {
        if (const Skill *s = m_skills->find(name))
            content += QStringLiteral("<skill name=\"%1\">\n%2\n</skill>\n\n").arg(s->name, s->body);
    }
    content += text;

    QJsonArray messages = session.value(QStringLiteral("messages")).toArray();
    messages << QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                            {QStringLiteral("content"), content},
                            {QStringLiteral("display"), text},
                            {QStringLiteral("skills"), QJsonArray::fromStringList(skills)}};
    session.insert(QStringLiteral("messages"), messages);
    if (session.value(QStringLiteral("title")).toString().isEmpty()) {
        session.insert(QStringLiteral("title"), text.simplified().left(60));
        saveSession(session);
        Q_EMIT sessionsChanged();
    } else {
        saveSession(session);
    }

    auto *run = new Run;
    run->serial = ++m_serial;
    run->sessionId = sessionId;
    run->modelRef = modelRef;
    run->agent = agent.isEmpty() ? QStringLiteral("build") : agent;
    run->session = session;
    m_runs.insert(sessionId, run);
    Q_EMIT busyChanged(sessionId, true);
    step(run);
}

void NativeEngine::runCommand(const QString &sessionId, const QString &command, const QString &arguments,
                              const QString &modelRef, const QString &agent)
{
    QJsonObject def = m_plugins->commands().value(command).toObject();
    if (def.isEmpty())
        def = builtinCommands().value(command).toObject();
    if (def.isEmpty()) {
        Q_EMIT errorOccurred(sessionId, tr("Bilinmeyen komut: /%1").arg(command));
        return;
    }
    QString text = def.value(QStringLiteral("template")).toString();
    const QStringList positional = arguments.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (int i = positional.size(); i >= 1; --i)
        text.replace(QStringLiteral("$%1").arg(i), positional.at(i - 1));
    text.replace(QStringLiteral("$ARGUMENTS"), arguments);
    text.remove(QRegularExpression(QStringLiteral("\\$\\d+")));
    send(sessionId, text.trimmed(), {}, modelRef, def.value(QStringLiteral("agent")).toString(agent));
}

void NativeEngine::step(Run *run)
{
    if (run->step >= Config::maxSteps()) {
        finishRun(run, tr("En fazla adım sayısına (%1) ulaşıldı.").arg(Config::maxSteps()));
        return;
    }
    const int slash = run->modelRef.indexOf(QLatin1Char('/'));
    ProviderConfig provider;
    if (slash < 0 || !Config::provider(run->modelRef.left(slash), &provider)) {
        finishRun(run, tr("Model seçilmedi veya sağlayıcı bulunamadı: %1").arg(run->modelRef));
        return;
    }

    LlmRequest req;
    req.provider = provider;
    req.model = run->modelRef.mid(slash + 1);
    req.system = systemPrompt(run->agent);
    for (const QJsonValue &v : run->session.value(QStringLiteral("messages")).toArray())
        if (v.toObject().value(QStringLiteral("role")).toString() != QLatin1String("notice"))
            req.messages << v;
    req.tools = m_tools->definitions(run->agent == QLatin1String("plan"));

    const QString sid = run->sessionId;
    const quint64 serial = run->serial;
    const QString key = QStringLiteral("%1:%2").arg(serial).arg(run->step);
    ++run->step;

    auto *stream = new LlmStream(m_nam, this);
    run->stream = stream;
    connect(stream, &LlmStream::textDelta, this, [this, sid, key](const QString &d) { Q_EMIT textDelta(sid, QStringLiteral("t") + key, d); });
    connect(stream, &LlmStream::reasoningDelta, this, [this, sid, key](const QString &d) { Q_EMIT reasoningDelta(sid, QStringLiteral("r") + key, d); });
    connect(stream, &LlmStream::toolCallStarted, this, [this, sid](const QString &id, const QString &name) {
        Q_EMIT toolUpdate(sid, id, name, {}, QStringLiteral("pending"), {});
    });
    connect(stream, &LlmStream::failed, this, [this, sid, serial, stream](const QString &error) {
        stream->deleteLater();
        if (Run *r = liveRun(sid, serial))
            finishRun(r, error);
    });
    connect(stream, &LlmStream::finished, this, [this, sid, serial, stream, key](QJsonObject assistant, const QJsonObject &usage) {
        stream->deleteLater();
        Run *r = liveRun(sid, serial);
        if (!r)
            return;
        if (recoverTextToolCall(&assistant))
            Q_EMIT textSet(sid, QStringLiteral("t") + key, QString());

        QJsonObject u = r->session.value(QStringLiteral("usage")).toObject();
        u.insert(QStringLiteral("input"), usage.value(QStringLiteral("input")).toInt());
        u.insert(QStringLiteral("output"), u.value(QStringLiteral("output")).toInt() + usage.value(QStringLiteral("output")).toInt());
        r->session.insert(QStringLiteral("usage"), u);
        Q_EMIT usageUpdated(sid, u.value(QStringLiteral("input")).toInt(), u.value(QStringLiteral("output")).toInt(), 0);

        QJsonArray messages = r->session.value(QStringLiteral("messages")).toArray();
        messages << assistant;
        r->session.insert(QStringLiteral("messages"), messages);
        saveSession(r->session);

        r->pendingCalls = assistant.value(QStringLiteral("tool_calls")).toArray();
        r->callIndex = 0;
        if (r->pendingCalls.isEmpty())
            finishRun(r);
        else
            runNextTool(r);
    });
    stream->start(req);
}

// Small local models often print tool calls as JSON text ({"name":..,"arguments":..},
// one or several, maybe inside a ```json fence) instead of using the tool-call channel.
// When the whole reply is made of such calls, turn them into real ones.
bool NativeEngine::recoverTextToolCall(QJsonObject *assistant) const
{
    if (assistant->contains(QStringLiteral("tool_calls")))
        return false;
    QString text = assistant->value(QStringLiteral("content")).toString();
    text.remove(QRegularExpression(QStringLiteral("```(?:json)?")));
    text = text.trimmed();
    if (!text.startsWith(QLatin1Char('{')))
        return false;

    QJsonArray calls;
    int i = 0;
    while (i < text.size()) {
        if (text.at(i).isSpace() || text.at(i) == QLatin1Char(',')) {
            ++i;
            continue;
        }
        if (text.at(i) != QLatin1Char('{'))
            return false;
        // Find the matching brace, honouring strings.
        int depth = 0;
        bool inString = false;
        int end = -1;
        for (int j = i; j < text.size(); ++j) {
            const QChar c = text.at(j);
            if (inString) {
                if (c == QLatin1Char('\\'))
                    ++j;
                else if (c == QLatin1Char('"'))
                    inString = false;
            } else if (c == QLatin1Char('"')) {
                inString = true;
            } else if (c == QLatin1Char('{')) {
                ++depth;
            } else if (c == QLatin1Char('}') && --depth == 0) {
                end = j;
                break;
            }
        }
        if (end < 0)
            return false;
        const QJsonObject obj = QJsonDocument::fromJson(text.mid(i, end - i + 1).toUtf8()).object();
        const QString name = obj.value(QStringLiteral("name")).toString();
        QJsonValue args = obj.value(QStringLiteral("arguments"));
        if (args.isUndefined())
            args = obj.value(QStringLiteral("parameters"));
        if (args.isString())
            args = QJsonDocument::fromJson(args.toString().toUtf8()).object();
        if (name.isEmpty() || !args.isObject() || !m_tools->exists(name))
            return false;
        calls << QJsonObject{{QStringLiteral("id"), QStringLiteral("call_") + QUuid::createUuid().toString(QUuid::Id128).left(12)},
                             {QStringLiteral("name"), name},
                             {QStringLiteral("arguments"), args.toObject()}};
        i = end + 1;
    }
    if (calls.isEmpty())
        return false;
    assistant->insert(QStringLiteral("content"), QString());
    assistant->insert(QStringLiteral("tool_calls"), calls);
    return true;
}

void NativeEngine::runNextTool(Run *run)
{
    if (run->callIndex >= run->pendingCalls.size()) {
        step(run);
        return;
    }
    const QJsonObject call = run->pendingCalls.at(run->callIndex).toObject();
    const QString name = call.value(QStringLiteral("name")).toString();
    const QJsonObject args = call.value(QStringLiteral("arguments")).toObject();
    Q_EMIT toolUpdate(run->sessionId, call.value(QStringLiteral("id")).toString(), name, args, QStringLiteral("running"), {});

    QString policy = Config::permission(name);
    if (m_tools->isReadOnly(name) && !Config::raw().value(QStringLiteral("permissions")).toObject().contains(name))
        policy = QStringLiteral("allow");
    if (m_alwaysAllowed.value(run->sessionId).contains(name))
        policy = QStringLiteral("allow");

    if (policy == QLatin1String("deny")) {
        QJsonArray messages = run->session.value(QStringLiteral("messages")).toArray();
        messages << QJsonObject{{QStringLiteral("role"), QStringLiteral("tool")},
                                {QStringLiteral("tool_call_id"), call.value(QStringLiteral("id"))},
                                {QStringLiteral("name"), name},
                                {QStringLiteral("content"), QStringLiteral("Permission denied by configuration.")},
                                {QStringLiteral("is_error"), true}};
        run->session.insert(QStringLiteral("messages"), messages);
        Q_EMIT toolUpdate(run->sessionId, call.value(QStringLiteral("id")).toString(), name, args, QStringLiteral("error"),
                        tr("Yapılandırma tarafından reddedildi."));
        ++run->callIndex;
        runNextTool(run);
        return;
    }
    if (policy == QLatin1String("ask")) {
        run->permissionId = QStringLiteral("perm_%1_%2").arg(run->serial).arg(run->callIndex);
        Q_EMIT permissionRequested(PermissionRequest{run->sessionId, run->permissionId, name, ToolRegistry::describe(name, args)});
        return;
    }
    executeTool(run, call);
}

void NativeEngine::replyPermission(const PermissionRequest &request, const QString &reply)
{
    Run *run = m_runs.value(request.sessionId);
    if (!run || run->permissionId != request.id)
        return;
    run->permissionId.clear();
    const QJsonObject call = run->pendingCalls.at(run->callIndex).toObject();
    const QString name = call.value(QStringLiteral("name")).toString();
    if (reply == QLatin1String("reject")) {
        // Stop the turn: record the rejection for every remaining call so the history stays valid.
        QJsonArray messages = run->session.value(QStringLiteral("messages")).toArray();
        for (int i = run->callIndex; i < run->pendingCalls.size(); ++i) {
            const QJsonObject c = run->pendingCalls.at(i).toObject();
            messages << QJsonObject{{QStringLiteral("role"), QStringLiteral("tool")},
                                    {QStringLiteral("tool_call_id"), c.value(QStringLiteral("id"))},
                                    {QStringLiteral("name"), c.value(QStringLiteral("name"))},
                                    {QStringLiteral("content"), QStringLiteral("The user rejected this tool call. Stop and ask how to proceed.")},
                                    {QStringLiteral("is_error"), true}};
            Q_EMIT toolUpdate(run->sessionId, c.value(QStringLiteral("id")).toString(), c.value(QStringLiteral("name")).toString(),
                            c.value(QStringLiteral("arguments")).toObject(), QStringLiteral("error"), tr("Kullanıcı reddetti."));
        }
        run->session.insert(QStringLiteral("messages"), messages);
        finishRun(run);
        return;
    }
    if (reply == QLatin1String("always"))
        m_alwaysAllowed[run->sessionId].insert(name);
    executeTool(run, call);
}

void NativeEngine::replyQuestion(const QString &, const QJsonArray &)
{
}

void NativeEngine::rejectQuestion(const QString &)
{
}

void NativeEngine::executeTool(Run *run, const QJsonObject &call)
{
    const QString sid = run->sessionId;
    const quint64 serial = run->serial;
    const QString id = call.value(QStringLiteral("id")).toString();
    const QString name = call.value(QStringLiteral("name")).toString();
    const QJsonObject args = call.value(QStringLiteral("arguments")).toObject();
    m_tools->execute(name, args, m_directory, [this, sid, serial, id, name, args](const QString &output, bool isError) {
        Run *r = liveRun(sid, serial);
        if (!r)
            return;
        QJsonArray messages = r->session.value(QStringLiteral("messages")).toArray();
        messages << QJsonObject{{QStringLiteral("role"), QStringLiteral("tool")},
                                {QStringLiteral("tool_call_id"), id},
                                {QStringLiteral("name"), name},
                                {QStringLiteral("content"), output},
                                {QStringLiteral("is_error"), isError}};
        r->session.insert(QStringLiteral("messages"), messages);
        saveSession(r->session);
        Q_EMIT toolUpdate(sid, id, name, args, isError ? QStringLiteral("error") : QStringLiteral("completed"), output);
        ++r->callIndex;
        runNextTool(r);
    });
}

void NativeEngine::finishRun(Run *run, const QString &error)
{
    const QString sid = run->sessionId;
    if (!error.isEmpty()) {
        QJsonArray messages = run->session.value(QStringLiteral("messages")).toArray();
        messages << QJsonObject{{QStringLiteral("role"), QStringLiteral("notice")}, {QStringLiteral("content"), error}};
        run->session.insert(QStringLiteral("messages"), messages);
    }
    saveSession(run->session);
    m_runs.remove(sid);
    delete run;
    if (!error.isEmpty())
        Q_EMIT errorOccurred(sid, error);
    Q_EMIT busyChanged(sid, false);
    Q_EMIT sessionsChanged();
}

void NativeEngine::abort(const QString &sessionId)
{
    Run *run = m_runs.value(sessionId);
    if (!run)
        return;
    run->aborted = true;
    if (run->stream)
        run->stream->abort();
    m_tools->abortRunning();
    // Close out any tool calls that never got a result.
    QJsonArray messages = run->session.value(QStringLiteral("messages")).toArray();
    QSet<QString> answered;
    for (const QJsonValue &v : messages)
        if (v.toObject().value(QStringLiteral("role")).toString() == QLatin1String("tool"))
            answered.insert(v.toObject().value(QStringLiteral("tool_call_id")).toString());
    for (const QJsonValue &v : run->pendingCalls) {
        const QJsonObject c = v.toObject();
        if (answered.contains(c.value(QStringLiteral("id")).toString()))
            continue;
        messages << QJsonObject{{QStringLiteral("role"), QStringLiteral("tool")},
                                {QStringLiteral("tool_call_id"), c.value(QStringLiteral("id"))},
                                {QStringLiteral("name"), c.value(QStringLiteral("name"))},
                                {QStringLiteral("content"), QStringLiteral("Interrupted by the user.")},
                                {QStringLiteral("is_error"), true}};
    }
    run->session.insert(QStringLiteral("messages"), messages);
    finishRun(run, tr("Durduruldu."));
}

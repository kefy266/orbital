#include "controller.h"

#include "../config.h"
#include "../nativeengine.h"
#include "../opencodeengine.h"
#include "../paths.h"
#include "../pluginmanager.h"
#include "../skillmanager.h"
#include "../tools.h"
#include "chatmodel.h"
#include "../llm.h"
#include <QNetworkAccessManager>

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QJsonArray>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

static QString prettyModelName(QString name, bool *free);

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_skills(new SkillManager(this))
    , m_plugins(new PluginManager(this))
    , m_chat(new ChatModel(this))
    , m_sessions(new SessionModel(this))
    , m_nam(new QNetworkAccessManager(this))
{
    Config::ensureDefault();
    Paths::installBundledContent();
    reloadExtensions();

    const QJsonObject settings = Paths::loadSettings();
    m_directory = settings.value(QStringLiteral("lastProject")).toString();
    if (m_directory.isEmpty() || !QDir(m_directory).exists())
        m_directory = QDir::homePath();

    m_engines.insert(QStringLiteral("native"), new NativeEngine(m_skills, m_plugins, this));
    m_engines.insert(QStringLiteral("opencode"), new OpencodeEngine(m_skills, m_plugins, this));

    m_sessionRefresh.setSingleShot(true);
    m_sessionRefresh.setInterval(300);
    connect(&m_sessionRefresh, &QTimer::timeout, this, &AppController::refreshSessions);
}

AppController::~AppController()
{
    for (Engine *e : std::as_const(m_engines))
        e->stop();
}

void AppController::start()
{
    switchEngine(Paths::loadSettings().value(QStringLiteral("engine")).toString(QStringLiteral("opencode")));
}

void AppController::reloadExtensions()
{
    m_plugins->reload();
    m_skills->setPluginSkillRoots(m_plugins->skillRoots());
    m_skills->reload();
    Q_EMIT actionsChanged();
}

// ---------------------------------------------------------------- properties

QString AppController::engineId() const
{
    return m_engine ? m_engine->id() : QString();
}

QString AppController::projectName() const
{
    const QString name = QDir(m_directory).dirName();
    return name.isEmpty() ? m_directory : name;
}

QString AppController::modelRef() const
{
    if (!m_modelRef.isEmpty())
        return m_modelRef;
    const QString saved = Paths::loadSettings().value(QStringLiteral("model.") + engineId()).toString();
    if (!saved.isEmpty())
        return saved;
    return engineId() == QLatin1String("native") ? Config::defaultModel() : QString();
}

QString AppController::modelLabel() const
{
    const QString ref = modelRef();
    for (const QVariant &v : m_models) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("ref")).toString() == ref)
            return m.value(QStringLiteral("name")).toString();
    }
    bool free = false;
    return ref.isEmpty() ? QStringLiteral("Model seç") : prettyModelName(ref.section(QLatin1Char('/'), 1), &free);
}

QVariantList AppController::agents() const
{
    QVariantList out;
    if (!m_engine)
        return out;
    for (const AgentInfo &a : m_engine->agents())
        out << QVariantMap{{QStringLiteral("id"), a.id}, {QStringLiteral("description"), a.description}};
    return out;
}

void AppController::setAgent(const QString &agent)
{
    if (agent == m_agent)
        return;
    m_agent = agent;
    Q_EMIT agentsChanged();
}

QVariantList AppController::actions() const
{
    QVariantList out;
    for (const PluginAction &a : m_plugins->actions())
        out << QVariantMap{{QStringLiteral("label"), a.label}, {QStringLiteral("run"), a.run},
                           {QStringLiteral("plugin"), a.pluginId}};
    return out;
}

QString AppController::themeMode() const
{
    return Paths::loadSettings().value(QStringLiteral("theme")).toString(QStringLiteral("system"));
}

void AppController::setThemeMode(const QString &mode)
{
    QJsonObject s = Paths::loadSettings();
    s.insert(QStringLiteral("theme"), mode);
    Paths::saveSettings(s);
    Q_EMIT themeModeChanged();
}

void AppController::setStatus(const QString &text, bool error)
{
    m_status = text;
    m_statusIsError = error;
    Q_EMIT statusChanged();
}

void AppController::setThinking(bool thinking)
{
    if (m_thinking == thinking)
        return;
    m_thinking = thinking;
    Q_EMIT thinkingChanged();
}

void AppController::setBusyFor(const QString &sid, bool busy)
{
    m_busy.insert(sid, busy);
    if (isCurrent(sid)) {
        if (!busy)
            setThinking(false);
        Q_EMIT busyChanged();
    }
}

// ---------------------------------------------------------------- engines

void AppController::switchEngine(const QString &id)
{
    if (m_engine) {
        disconnect(m_engine, nullptr, this, nullptr);
        m_engine->stop();
    }
    m_engine = m_engines.value(id, m_engines.value(QStringLiteral("native")));
    m_engineReady = false;
    m_sessionId.clear();
    m_sessionTitle.clear();
    m_queued.clear();
    m_busy.clear();
    m_models.clear();
    m_modelRef.clear();
    m_permissionQueue.clear();
    m_permission.clear();
    m_chat->clear();
    m_sessions->setRows({});
    setThinking(false);
    m_usage.clear();
    Q_EMIT engineChanged();
    Q_EMIT sessionChanged();
    Q_EMIT busyChanged();
    Q_EMIT modelsChanged();
    Q_EMIT modelChanged();
    Q_EMIT agentsChanged();
    Q_EMIT usageChanged();
    Q_EMIT permissionChanged();

    QJsonObject s = Paths::loadSettings();
    s.insert(QStringLiteral("engine"), m_engine->id());
    Paths::saveSettings(s);

    connectEngine();
    setStatus(QStringLiteral("Hazırlanıyor…"));
    m_engine->start(m_directory);
}

void AppController::retryEngine()
{
    switchEngine(engineId());
}

void AppController::connectEngine()
{
    Engine *e = m_engine;
    connect(e, &Engine::ready, this, &AppController::onEngineReady);
    connect(e, &Engine::failed, this, [this](const QString &reason) {
        m_engineReady = false;
        Q_EMIT engineChanged();
        setThinking(false);
        setStatus(reason.section(QLatin1Char('\n'), 0, 0), true);
        m_chat->addError(reason);
    });
    connect(e, &Engine::statusMessage, this, [this](const QString &text) { setStatus(text); });
    connect(e, &Engine::textDelta, this, [this](const QString &sid, const QString &key, const QString &d) {
        if (!isCurrent(sid))
            return;
        setThinking(false);
        m_chat->appendText(key, d);
    });
    connect(e, &Engine::textSet, this, [this](const QString &sid, const QString &key, const QString &t) {
        if (!isCurrent(sid))
            return;
        if (!t.trimmed().isEmpty())
            setThinking(false);
        m_chat->setText(key, t);
    });
    connect(e, &Engine::reasoningDelta, this, [this](const QString &sid, const QString &key, const QString &d) {
        if (!isCurrent(sid))
            return;
        setThinking(false);
        m_chat->appendReasoning(key, d);
    });
    connect(e, &Engine::toolUpdate, this,
            [this](const QString &sid, const QString &id, const QString &name, const QJsonObject &input,
                   const QString &status, const QString &output) {
                if (!isCurrent(sid))
                    return;
                setThinking(false);
                m_chat->toolUpdate(id, name, input, status, output);
            });
    connect(e, &Engine::busyChanged, this, [this](const QString &sid, bool busy) { setBusyFor(sid, busy); });
    connect(e, &Engine::errorOccurred, this, [this](const QString &sid, const QString &message) {
        if (!isCurrent(sid))
            return;
        setThinking(false);
        m_chat->addError(message);
    });
    connect(e, &Engine::permissionRequested, this, [this](const PermissionRequest &r) {
        m_permissionQueue << r;
        if (m_permission.isEmpty()) {
            m_permission = QVariantMap{{QStringLiteral("tool"), r.title}, {QStringLiteral("detail"), r.detail}};
            Q_EMIT permissionChanged();
        }
    });
    connect(e, &Engine::usageUpdated, this, [this](const QString &sid, int in, int out, double cost) {
        if (!isCurrent(sid))
            return;
        const auto k = [](int n) { return n >= 1000 ? QStringLiteral("%1k").arg(n / 1000.0, 0, 'f', 1) : QString::number(n); };
        m_usage = cost > 0 ? QStringLiteral("%1 bağlam · %2 çıktı · $%3").arg(k(in), k(out)).arg(cost, 0, 'f', 4)
                           : QStringLiteral("%1 bağlam · %2 çıktı").arg(k(in), k(out));
        Q_EMIT usageChanged();
    });
    connect(e, &Engine::sessionsChanged, this, [this] { m_sessionRefresh.start(); });
}

void AppController::onEngineReady()
{
    m_engineReady = true;
    setStatus({});
    Q_EMIT engineChanged();
    Q_EMIT agentsChanged();
    if (!m_engine->agents().isEmpty() && m_agent.isEmpty())
        m_agent = m_engine->agents().first().id;
    refreshModels();
    refreshCommands();
    refreshSessions();
    if (!m_queued.isEmpty()) {
        const QString q = m_queued;
        m_queued.clear();
        submit(q);
    }
    disconnect(m_mcpConnection);
    if (auto *native = qobject_cast<NativeEngine *>(m_engine)) {
        m_mcpConnection = connect(native->tools(), &ToolRegistry::extensionsChanged, this, [this, native] {
            for (const QString &s : native->tools()->mcpStatus())
                if (s.contains(QStringLiteral("hata")))
                    Q_EMIT toast(QStringLiteral("MCP · %1").arg(s));
        });
    }
}

// "deepseek-v4.1-flash-free" / "DeepSeek V4.1 Flash Free" -> "DeepSeek 4.1 Flash" (+ free flag).
static QString prettyModelName(QString name, bool *free)
{
    static const QRegularExpression freeSuffix(QStringLiteral("[\\s_-]*free$"), QRegularExpression::CaseInsensitiveOption);
    *free = name.contains(freeSuffix);
    name.remove(freeSuffix);
    if (!name.contains(QLatin1Char(' '))) {
        // A raw id: split into words and capitalise them.
        name.replace(QRegularExpression(QStringLiteral("[-_:/]+")), QStringLiteral(" "));
        QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (QString &w : words) {
            if (w.isEmpty())
                continue;
            if (QRegularExpression(QStringLiteral("^\\d+(\\.\\d+)?[bkm]$"), QRegularExpression::CaseInsensitiveOption).match(w).hasMatch())
                w = w.toUpper(); // 7b -> 7B
            else
                w[0] = w.at(0).toUpper();
        }
        name = words.join(QLatin1Char(' '));
    }
    static const QList<QPair<QString, QString>> brands{
        {QStringLiteral("Deepseek"), QStringLiteral("DeepSeek")}, {QStringLiteral("Gpt"), QStringLiteral("GPT")},
        {QStringLiteral("Glm"), QStringLiteral("GLM")},           {QStringLiteral("Minimax"), QStringLiteral("MiniMax")},
        {QStringLiteral("Mimo"), QStringLiteral("MiMo")},         {QStringLiteral("Longcat"), QStringLiteral("LongCat")},
        {QStringLiteral("Openai"), QStringLiteral("OpenAI")},     {QStringLiteral("Xai"), QStringLiteral("xAI")},
        {QStringLiteral("Vl"), QStringLiteral("VL")},
    };
    for (const auto &[from, to] : brands)
        name.replace(QRegularExpression(QStringLiteral("\\b%1\\b").arg(from)), to);
    // "V4.1" -> "4.1" (keep model families like "V3" only when followed by a version dot or digit run)
    name.replace(QRegularExpression(QStringLiteral("\\bV(\\d)")), QStringLiteral("\\1"));
    return name.simplified();
}

static QString prettyProviderName(const QString &id, const QString &name)
{
    static const QHash<QString, QString> headings{
        {QStringLiteral("opencode"), QStringLiteral("ORBITAL Cloud")},
        {QStringLiteral("gemini"), QStringLiteral("Google Modelleri")},
        {QStringLiteral("google"), QStringLiteral("Google Modelleri")},
        {QStringLiteral("openai"), QStringLiteral("OpenAI Modelleri")},
        {QStringLiteral("anthropic"), QStringLiteral("Anthropic Modelleri")},
        {QStringLiteral("openrouter"), QStringLiteral("OpenRouter Modelleri")},
        {QStringLiteral("deepseek"), QStringLiteral("DeepSeek Modelleri")},
        {QStringLiteral("groq"), QStringLiteral("Groq Modelleri")},
        {QStringLiteral("mistral"), QStringLiteral("Mistral Modelleri")},
        {QStringLiteral("xai"), QStringLiteral("xAI Modelleri")},
    };
    return headings.value(id, name + QStringLiteral(" Modelleri"));
}

void AppController::refreshModels()
{
    Engine *engine = m_engine;
    engine->listModels([this, engine](const QList<ModelChoice> &models) {
        if (engine != m_engine)
            return;
        m_models.clear();
        for (const ModelChoice &m : models) {
            bool free = false;
            const QString pretty = prettyModelName(m.name.isEmpty() ? m.model : m.name, &free);
            m_models << QVariantMap{{QStringLiteral("ref"), m.ref()},
                                    {QStringLiteral("provider"), prettyProviderName(m.provider, m.providerName)},
                                    {QStringLiteral("model"), m.model},
                                    {QStringLiteral("name"), pretty},
                                    {QStringLiteral("free"), free || m.provider == QLatin1String("opencode")}};
        }
        QString wanted = Paths::loadSettings().value(QStringLiteral("model.") + engine->id()).toString();
        if (wanted.isEmpty() && engine->id() == QLatin1String("native"))
            wanted = Config::defaultModel();
        // Fall back to a free cloud model that works without an API key.
        static const QString kFreeDefault = QStringLiteral("opencode/deepseek-v4.1-flash-free");
        bool found = false;
        bool haveFree = false;
        for (const ModelChoice &m : models) {
            found = found || m.ref() == wanted;
            haveFree = haveFree || m.ref() == kFreeDefault;
        }
        m_modelRef = found ? wanted : haveFree ? kFreeDefault : (models.isEmpty() ? QString() : models.first().ref());
        Q_EMIT modelsChanged();
        Q_EMIT modelChanged();
        if (models.isEmpty())
            Q_EMIT toast(engine->id() == QLatin1String("native")
                             ? QStringLiteral("Bu motorda model yok: Ayarlar → Sağlayıcılar'dan API anahtarı ekle ya da Orbital Cloud'a geç")
                             : QStringLiteral("Hiç model bulunamadı. İnternet bağlantını kontrol et."));
    });
}

void AppController::setModel(const QString &ref)
{
    m_modelRef = ref;
    QJsonObject s = Paths::loadSettings();
    s.insert(QStringLiteral("model.") + engineId(), ref);
    Paths::saveSettings(s);
    if (engineId() == QLatin1String("native"))
        Config::setDefaultModel(ref);
    Q_EMIT modelChanged();
}

void AppController::refreshCommands()
{
    Engine *engine = m_engine;
    engine->listCommands([this, engine](const QList<CommandDef> &defs) {
        if (engine != m_engine)
            return;
        m_commands.clear();
        for (const Skill &s : m_skills->skills())
            if (s.enabled)
                m_commands << QVariantMap{{QStringLiteral("name"), s.name}, {QStringLiteral("description"), s.description},
                                          {QStringLiteral("source"), QStringLiteral("skill")}};
        for (const CommandDef &d : defs)
            m_commands << QVariantMap{{QStringLiteral("name"), d.name}, {QStringLiteral("description"), d.description},
                                      {QStringLiteral("source"), d.source}};
        Q_EMIT commandsChanged();
    });
}

void AppController::refreshSessions()
{
    if (!m_engine || !m_engineReady)
        return;
    Engine *engine = m_engine;
    engine->listSessions([this, engine](const QList<SessionInfo> &list) {
        if (engine != m_engine)
            return;
        QList<SessionModel::Row> rows;
        for (const SessionInfo &s : list)
            rows << SessionModel::Row{s.id, s.title, s.updated};
        m_sessions->setRows(rows);
        if (!m_sessionId.isEmpty()) {
            const QString t = m_sessions->titleOf(m_sessionId);
            if (!t.isEmpty() && t != m_sessionTitle) {
                m_sessionTitle = t;
                Q_EMIT sessionChanged();
            }
        }
    });
}

// ---------------------------------------------------------------- chat

void AppController::newChat()
{
    m_sessionId.clear();
    m_sessionTitle.clear();
    m_chat->clear();
    m_usage.clear();
    setThinking(false);
    Q_EMIT sessionChanged();
    Q_EMIT busyChanged();
    Q_EMIT usageChanged();
}

void AppController::openSession(const QString &id)
{
    if (!m_engine || id == m_sessionId)
        return;
    m_sessionId = id;
    m_sessionTitle = m_sessions->titleOf(id);
    m_usage.clear();
    m_chat->clear();
    setThinking(false);
    Q_EMIT sessionChanged();
    Q_EMIT busyChanged();
    Q_EMIT usageChanged();
    m_engine->loadHistory(id, [this, id](const QJsonArray &items) {
        if (id != m_sessionId)
            return;
        m_chat->addHistory(items);
        setThinking(m_busy.value(id));
        Q_EMIT scrollToEnd();
    });
}

void AppController::deleteSession(const QString &id)
{
    if (!m_engine)
        return;
    m_engine->deleteSession(id, [this, id](const QString &) {
        if (id == m_sessionId)
            newChat();
        refreshSessions();
    });
}

void AppController::setProject(const QUrl &folder)
{
    const QString dir = folder.isLocalFile() ? folder.toLocalFile() : folder.toString();
    if (dir.isEmpty() || dir == m_directory)
        return;
    m_directory = dir;
    QJsonObject s = Paths::loadSettings();
    s.insert(QStringLiteral("lastProject"), dir);
    Paths::saveSettings(s);
    Q_EMIT projectChanged();
    switchEngine(engineId());
}

void AppController::submit(const QString &rawText)
{
    const QString trimmed = rawText.trimmed();
    if (trimmed.isEmpty())
        return;
    if (!m_engine || !m_engineReady) {
        m_queued = trimmed;
        Q_EMIT toast(QStringLiteral("Motor hazırlanıyor; mesajın hazır olunca gönderilecek"));
        return;
    }
    QString text = trimmed;
    QStringList skills = m_attached;

    if (text.startsWith(QLatin1Char('/'))) {
        const QString name = text.mid(1).section(QLatin1Char(' '), 0, 0);
        if (m_skills->find(name)) {
            if (!skills.contains(name))
                skills << name;
            text = text.mid(name.size() + 1).trimmed();
            if (text.isEmpty()) {
                attachSkill(name);
                return;
            }
        }
    }

    QString reason;
    if (!m_plugins->runBeforePrompt(&text, m_sessionId, m_directory, &reason)) {
        Q_EMIT toast(reason);
        return;
    }
    m_attached.clear();
    Q_EMIT attachedSkillsChanged();

    if (m_sessionId.isEmpty()) {
        m_chat->clear();
        m_engine->createSession([this, text, skills](const QString &id, const QString &error) {
            if (id.isEmpty()) {
                m_chat->addError(QStringLiteral("Oturum oluşturulamadı: %1").arg(error));
                return;
            }
            m_sessionId = id;
            m_sessionTitle = text.simplified().left(60);
            Q_EMIT sessionChanged();
            dispatch(text, skills);
            refreshSessions();
        });
        return;
    }
    dispatch(text, skills);
}

void AppController::dispatch(const QString &text, const QStringList &skills)
{
    m_chat->addUser(text, skills);
    setThinking(true);
    setBusyFor(m_sessionId, true); // before dispatching: a synchronous failure must be able to clear it
    Q_EMIT scrollToEnd();
    if (text.startsWith(QLatin1Char('/'))) {
        const QString name = text.mid(1).section(QLatin1Char(' '), 0, 0);
        m_engine->runCommand(m_sessionId, name, text.mid(name.size() + 1).trimmed(), modelRef(), m_agent);
    } else {
        m_engine->send(m_sessionId, text, skills, modelRef(), m_agent);
    }
}

void AppController::stop()
{
    if (m_engine && !m_sessionId.isEmpty() && busy())
        m_engine->abort(m_sessionId);
}

void AppController::attachSkill(const QString &name)
{
    if (m_attached.contains(name))
        return;
    m_attached << name;
    Q_EMIT attachedSkillsChanged();
}

void AppController::detachSkill(const QString &name)
{
    if (m_attached.removeAll(name))
        Q_EMIT attachedSkillsChanged();
}

void AppController::replyPermission(const QString &reply)
{
    if (m_permissionQueue.isEmpty())
        return;
    const PermissionRequest r = m_permissionQueue.takeFirst();
    if (m_engine)
        m_engine->replyPermission(r, reply);
    if (m_permissionQueue.isEmpty()) {
        m_permission.clear();
    } else {
        const PermissionRequest &n = m_permissionQueue.first();
        m_permission = QVariantMap{{QStringLiteral("tool"), n.title}, {QStringLiteral("detail"), n.detail}};
    }
    Q_EMIT permissionChanged();
}

void AppController::runAction(int index)
{
    const QList<PluginAction> list = m_plugins->actions();
    if (index < 0 || index >= list.size())
        return;
    const PluginAction action = list.at(index);
    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(m_directory);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(proc, &QProcess::finished, this, [this, proc, action] {
        proc->deleteLater();
        const QString out = QString::fromUtf8(proc->readAll()).trimmed();
        submit(QStringLiteral("```\n$ %1\n%2\n```").arg(action.run, out.left(20000)));
    });
    proc->start(QStringLiteral("/bin/bash"), {QStringLiteral("-c"), action.run});
}

void AppController::copyText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
    Q_EMIT toast(QStringLiteral("Panoya kopyalandı"));
}

void AppController::openPath(const QString &path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

// ---------------------------------------------------------------- settings

QVariantList AppController::skillList() const
{
    QVariantList out;
    for (const Skill &s : m_skills->skills())
        out << QVariantMap{{QStringLiteral("name"), s.name},       {QStringLiteral("description"), s.description},
                           {QStringLiteral("body"), s.body},       {QStringLiteral("dir"), s.dir},
                           {QStringLiteral("origin"), s.origin},   {QStringLiteral("enabled"), s.enabled},
                           {QStringLiteral("editable"), s.editable()}};
    return out;
}

QString AppController::saveSkill(const QString &original, const QString &name, const QString &description,
                                 const QString &body)
{
    QString error;
    if (!m_skills->save(original, name, description, body, &error))
        return error;
    m_settingsDirty = true;
    refreshCommands();
    return {};
}

QString AppController::deleteSkill(const QString &name)
{
    QString error;
    if (!m_skills->remove(name, &error))
        return error;
    m_settingsDirty = true;
    refreshCommands();
    return {};
}

void AppController::setSkillEnabled(const QString &name, bool enabled)
{
    m_skills->setEnabled(name, enabled);
    m_settingsDirty = true;
    refreshCommands();
}

QString AppController::importSkill(const QUrl &folder)
{
    QString error;
    if (!m_skills->importFolder(folder.toLocalFile(), &error))
        return error;
    m_settingsDirty = true;
    refreshCommands();
    return {};
}

QVariantList AppController::pluginList() const
{
    QVariantList out;
    for (const Plugin &p : m_plugins->plugins())
        out << QVariantMap{{QStringLiteral("id"), p.id},
                           {QStringLiteral("name"), p.name},
                           {QStringLiteral("version"), p.version},
                           {QStringLiteral("description"), p.description},
                           {QStringLiteral("author"), p.author},
                           {QStringLiteral("dir"), p.dir},
                           {QStringLiteral("enabled"), p.enabled},
                           {QStringLiteral("error"), p.error},
                           {QStringLiteral("summary"), p.summary().join(QStringLiteral(" · "))}};
    return out;
}

void AppController::setPluginEnabled(const QString &id, bool enabled)
{
    m_plugins->setEnabled(id, enabled);
    reloadExtensions();
    m_settingsDirty = true;
}

QString AppController::removePlugin(const QString &id)
{
    QString error;
    if (!m_plugins->remove(id, &error))
        return error;
    reloadExtensions();
    m_settingsDirty = true;
    return {};
}

QString AppController::createPlugin(const QString &id)
{
    QString error;
    if (!m_plugins->create(id, id, &error))
        return error;
    reloadExtensions();
    m_settingsDirty = true;
    return {};
}

QString AppController::installPlugin(const QUrl &folder)
{
    QString error;
    if (!m_plugins->install(folder.toLocalFile(), &error))
        return error;
    reloadExtensions();
    m_settingsDirty = true;
    return {};
}

// Providers users can connect with just an API key.
struct ProviderPreset
{
    const char *id;
    const char *name;
    const char *description;
    const char *type;
    const char *baseUrl;
    const char *keyUrl;
    const char *modelFilter;
    const char *color;
};

static const ProviderPreset kPresets[] = {
    {"openai", "OpenAI", "GPT ve o-serisi modeller", "openai", "https://api.openai.com/v1",
     "https://platform.openai.com/api-keys", "^(gpt-|o[0-9]|chatgpt)(?!.*(audio|realtime|transcribe|tts|image|search))", "#10a37f"},
    {"anthropic", "Anthropic", "Claude modelleri", "anthropic", "https://api.anthropic.com",
     "https://console.anthropic.com/settings/keys", "", "#d97757"},
    {"gemini", "Google Gemini", "Gemini modelleri", "openai", "https://generativelanguage.googleapis.com/v1beta/openai",
     "https://aistudio.google.com/apikey", "^gemini(?!.*(embedding|tts|image|live))", "#4285f4"},
    {"openrouter", "OpenRouter", "Tek anahtarla yüzlerce model", "openai", "https://openrouter.ai/api/v1",
     "https://openrouter.ai/keys", "", "#6467f2"},
    {"deepseek", "DeepSeek", "DeepSeek sohbet ve akıl yürütme modelleri", "openai", "https://api.deepseek.com/v1",
     "https://platform.deepseek.com/api_keys", "", "#4d6bfe"},
    {"groq", "Groq", "Çok hızlı açık modeller", "openai", "https://api.groq.com/openai/v1",
     "https://console.groq.com/keys", "^(?!.*(whisper|tts|guard|playai)).*$", "#f55036"},
    {"mistral", "Mistral", "Mistral ve Codestral modelleri", "openai", "https://api.mistral.ai/v1",
     "https://console.mistral.ai/api-keys", "^(?!.*(embed|moderation|ocr)).*$", "#fa520f"},
    {"xai", "xAI", "Grok modelleri", "openai", "https://api.x.ai/v1", "https://console.x.ai", "^grok(?!.*image)", "#7c7c84"},
};

static const ProviderPreset *findPreset(const QString &id)
{
    for (const ProviderPreset &p : kPresets)
        if (id == QLatin1String(p.id))
            return &p;
    return nullptr;
}

QVariantList AppController::providerCatalog() const
{
    const QJsonObject configured = Config::raw().value(QStringLiteral("providers")).toObject();
    const auto connected = [&configured](const QString &id) {
        if (!configured.contains(id))
            return false;
        ProviderConfig p;
        p.apiKey = configured.value(id).toObject().value(QStringLiteral("apiKey")).toString();
        return !p.resolvedApiKey().isEmpty();
    };
    QVariantList out;
    for (const ProviderPreset &p : kPresets)
        out << QVariantMap{{QStringLiteral("id"), QString::fromLatin1(p.id)},
                           {QStringLiteral("name"), QString::fromUtf8(p.name)},
                           {QStringLiteral("description"), QString::fromUtf8(p.description)},
                           {QStringLiteral("keyUrl"), QString::fromLatin1(p.keyUrl)},
                           {QStringLiteral("color"), QString::fromLatin1(p.color)},
                           {QStringLiteral("custom"), false},
                           {QStringLiteral("connected"), connected(QString::fromLatin1(p.id))}};
    for (auto it = configured.begin(); it != configured.end(); ++it) {
        if (findPreset(it.key()))
            continue;
        const QJsonObject o = it.value().toObject();
        out << QVariantMap{{QStringLiteral("id"), it.key()},
                           {QStringLiteral("name"), o.value(QStringLiteral("name")).toString(it.key())},
                           {QStringLiteral("description"), o.value(QStringLiteral("baseURL")).toString()},
                           {QStringLiteral("keyUrl"), QString()},
                           {QStringLiteral("color"), QStringLiteral("#8a8a93")},
                           {QStringLiteral("custom"), true},
                           {QStringLiteral("connected"), true}};
    }
    return out;
}

void AppController::connectProvider(const QString &id, const QString &apiKey)
{
    const ProviderPreset *preset = findPreset(id);
    const QJsonObject existing = Config::raw().value(QStringLiteral("providers")).toObject().value(id).toObject();
    if (!preset && existing.isEmpty()) {
        Q_EMIT providerResult(id, false, QStringLiteral("Bilinmeyen sağlayıcı."));
        return;
    }
    ProviderConfig p;
    p.id = id;
    p.name = preset ? QString::fromUtf8(preset->name) : existing.value(QStringLiteral("name")).toString(id);
    p.type = preset ? QString::fromLatin1(preset->type) : existing.value(QStringLiteral("type")).toString(QStringLiteral("openai"));
    p.baseUrl = preset ? QString::fromLatin1(preset->baseUrl) : existing.value(QStringLiteral("baseURL")).toString();
    p.modelFilter = preset ? QString::fromLatin1(preset->modelFilter) : existing.value(QStringLiteral("modelFilter")).toString();
    p.apiKey = apiKey.trimmed();
    if (p.apiKey.isEmpty()) {
        Q_EMIT providerResult(id, false, QStringLiteral("API anahtarını yapıştır."));
        return;
    }
    testProvider(m_nam, p, [this, p](bool ok, const QString &message, const QStringList &models) {
        if (!ok) {
            Q_EMIT providerResult(p.id, false, message);
            return;
        }
        QJsonObject entry{{QStringLiteral("name"), p.name},
                          {QStringLiteral("type"), p.type},
                          {QStringLiteral("baseURL"), p.baseUrl},
                          {QStringLiteral("apiKey"), p.apiKey}};
        if (!p.modelFilter.isEmpty())
            entry.insert(QStringLiteral("modelFilter"), p.modelFilter);
        if (p.type == QLatin1String("anthropic")) // no live listing in the chat path: keep what we found
            entry.insert(QStringLiteral("models"), QJsonArray::fromStringList(models));
        QJsonObject config = Config::raw();
        QJsonObject providers = config.value(QStringLiteral("providers")).toObject();
        providers.insert(p.id, entry);
        config.insert(QStringLiteral("providers"), providers);
        Config::saveRaw(config, nullptr);
        m_settingsDirty = true;
        Q_EMIT providerResult(p.id, true, message);
    });
}

void AppController::addCustomProvider(const QString &name, const QString &baseUrl, const QString &apiKey)
{
    const QString id = SkillManager::sanitizeName(name);
    if (id.isEmpty() || baseUrl.trimmed().isEmpty()) {
        Q_EMIT providerResult(QStringLiteral("custom"), false, QStringLiteral("Ad ve adres gerekli."));
        return;
    }
    QString url = baseUrl.trimmed();
    while (url.endsWith(QLatin1Char('/')))
        url.chop(1);
    ProviderConfig p;
    p.id = id;
    p.name = name.trimmed();
    p.type = QStringLiteral("openai");
    p.baseUrl = url;
    p.apiKey = apiKey.trimmed();
    testProvider(m_nam, p, [this, p](bool ok, const QString &message, const QStringList &) {
        if (!ok) {
            Q_EMIT providerResult(QStringLiteral("custom"), false, message);
            return;
        }
        QJsonObject config = Config::raw();
        QJsonObject providers = config.value(QStringLiteral("providers")).toObject();
        providers.insert(p.id, QJsonObject{{QStringLiteral("name"), p.name},
                                           {QStringLiteral("type"), p.type},
                                           {QStringLiteral("baseURL"), p.baseUrl},
                                           {QStringLiteral("apiKey"), p.apiKey}});
        config.insert(QStringLiteral("providers"), providers);
        Config::saveRaw(config, nullptr);
        m_settingsDirty = true;
        Q_EMIT providerResult(QStringLiteral("custom"), true, message);
    });
}

void AppController::disconnectProvider(const QString &id)
{
    removeProvider(id);
}

QVariantList AppController::providerList() const
{
    QVariantList out;
    const QJsonObject providers = Config::raw().value(QStringLiteral("providers")).toObject();
    for (auto it = providers.begin(); it != providers.end(); ++it) {
        const QJsonObject p = it.value().toObject();
        QStringList models;
        for (const QJsonValue &m : p.value(QStringLiteral("models")).toArray())
            models << m.toString();
        out << QVariantMap{{QStringLiteral("id"), it.key()},
                           {QStringLiteral("name"), p.value(QStringLiteral("name")).toString(it.key())},
                           {QStringLiteral("type"), p.value(QStringLiteral("type")).toString(QStringLiteral("openai"))},
                           {QStringLiteral("baseURL"), p.value(QStringLiteral("baseURL")).toString()},
                           {QStringLiteral("apiKey"), p.value(QStringLiteral("apiKey")).toString()},
                           {QStringLiteral("models"), models.join(QStringLiteral(", "))}};
    }
    return out;
}

void AppController::saveProvider(const QString &id, const QVariantMap &fields)
{
    QJsonObject config = Config::raw();
    QJsonObject providers = config.value(QStringLiteral("providers")).toObject();
    QJsonObject p = providers.value(id).toObject();
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        if (it.key() == QLatin1String("models")) {
            QJsonArray models;
            for (const QString &m : it.value().toString().split(QLatin1Char(','), Qt::SkipEmptyParts))
                models << m.trimmed();
            p.insert(QStringLiteral("models"), models);
        } else {
            p.insert(it.key(), it.value().toString());
        }
    }
    providers.insert(id, p);
    config.insert(QStringLiteral("providers"), providers);
    Config::saveRaw(config, nullptr);
    m_settingsDirty = true;
}

QString AppController::addProvider(const QString &name)
{
    const QString id = SkillManager::sanitizeName(name);
    if (id.isEmpty())
        return {};
    saveProvider(id, {{QStringLiteral("name"), name},
                      {QStringLiteral("type"), QStringLiteral("openai")},
                      {QStringLiteral("baseURL"), QStringLiteral("http://localhost:1234/v1")}});
    return id;
}

void AppController::removeProvider(const QString &id)
{
    QJsonObject config = Config::raw();
    QJsonObject providers = config.value(QStringLiteral("providers")).toObject();
    providers.remove(id);
    config.insert(QStringLiteral("providers"), providers);
    Config::saveRaw(config, nullptr);
    m_settingsDirty = true;
}

QVariantMap AppController::permissionMap() const
{
    return Config::raw().value(QStringLiteral("permissions")).toObject().toVariantMap();
}

void AppController::setPermission(const QString &tool, const QString &policy)
{
    QJsonObject config = Config::raw();
    QJsonObject perms = config.value(QStringLiteral("permissions")).toObject();
    perms.insert(tool, policy);
    config.insert(QStringLiteral("permissions"), perms);
    Config::saveRaw(config, nullptr);
}

int AppController::maxSteps() const
{
    return Config::maxSteps();
}

void AppController::setMaxSteps(int steps)
{
    QJsonObject config = Config::raw();
    config.insert(QStringLiteral("maxSteps"), steps);
    Config::saveRaw(config, nullptr);
}

QString AppController::opencodeVersion() const
{
    return OpencodeEngine::version();
}

void AppController::setOpencodeVersion(const QString &version)
{
    QJsonObject config = Config::raw();
    QJsonObject oc = config.value(QStringLiteral("opencode")).toObject();
    oc.insert(QStringLiteral("version"), version.trimmed());
    config.insert(QStringLiteral("opencode"), oc);
    Config::saveRaw(config, nullptr);
    m_settingsDirty = true;
}

QVariantMap AppController::paths() const
{
    return {{QStringLiteral("config"), Config::file()},
            {QStringLiteral("skills"), Paths::skillsDir()},
            {QStringLiteral("plugins"), Paths::pluginsDir()},
            {QStringLiteral("data"), Paths::dataDir()},
            {QStringLiteral("engine"), Paths::dataDir() + QStringLiteral("/engine")},
            {QStringLiteral("opencodeHome"), OpencodeEngine::homeDir()}};
}

void AppController::applySettings()
{
    if (!m_settingsDirty)
        return;
    m_settingsDirty = false;
    reloadExtensions();
    switchEngine(engineId());
    Q_EMIT toast(QStringLiteral("Ayarlar uygulandı"));
}

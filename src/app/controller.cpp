#include "controller.h"

#include "../config.h"
#include "../nativeengine.h"
#include "../opencodeengine.h"
#include "../paths.h"
#include "../platform.h"
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
#include <QJsonDocument>
#include <QNetworkReply>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>
#include <memory>

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
        m_directory = Paths::noFolderDir();

    m_engines.insert(QStringLiteral("native"), new NativeEngine(m_skills, m_plugins, this));
    m_engines.insert(QStringLiteral("opencode"), new OpencodeEngine(m_skills, m_plugins, this));

    m_sessionRefresh.setSingleShot(true);
    m_sessionRefresh.setInterval(300);
    connect(&m_sessionRefresh, &QTimer::timeout, this, &AppController::refreshSessions);
    loadPromptHistory();
}

AppController::~AppController()
{
    for (Engine *e : std::as_const(m_engines))
        e->stop();
}

void AppController::start()
{
    // One engine for everyone: free cloud models plus any provider the user connects.
    switchEngine(QStringLiteral("opencode"));
    QTimer::singleShot(4000, this, [this] { checkForUpdates(true); });
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
    if (m_directory == Paths::noFolderDir())
        return QStringLiteral("Klasörsüz (no-folder)");
    const QString name = QDir(m_directory).dirName();
    return name.isEmpty() ? m_directory : name;
}

bool AppController::isNoFolder() const
{
    return m_directory == Paths::noFolderDir();
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
    m_questionQueue.clear();
    m_question.clear();
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
    Q_EMIT questionChanged();

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
    connect(e, &Engine::questionRequested, this, [this](const QuestionRequest &r) {
        m_questionQueue << r;
        if (m_question.isEmpty()) {
            m_question = QVariantMap{{QStringLiteral("id"), r.id},
                                     {QStringLiteral("sessionId"), r.sessionId},
                                     {QStringLiteral("questions"), r.questions.toVariantList()}};
            Q_EMIT questionChanged();
        }
    });
    connect(e, &Engine::questionDismissed, this, [this](const QString &requestId) {
        for (int i = 0; i < m_questionQueue.size(); ++i) {
            if (m_questionQueue.at(i).id == requestId) {
                m_questionQueue.removeAt(i);
                break;
            }
        }
        if (m_question.value(QStringLiteral("id")).toString() == requestId) {
            if (m_questionQueue.isEmpty()) {
                m_question.clear();
            } else {
                const QuestionRequest &n = m_questionQueue.first();
                m_question = QVariantMap{{QStringLiteral("id"), n.id},
                                         {QStringLiteral("sessionId"), n.sessionId},
                                         {QStringLiteral("questions"), n.questions.toVariantList()}};
            }
            Q_EMIT questionChanged();
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
            rows << SessionModel::Row{s.id, s.title, s.updated, s.directory};
        m_sessions->setRows(rows);
        Q_EMIT projectsChanged();
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
    m_questionQueue.clear();
    m_question.clear();
    setThinking(false);
    Q_EMIT sessionChanged();
    Q_EMIT busyChanged();
    Q_EMIT usageChanged();
    Q_EMIT questionChanged();
}

void AppController::openSession(const QString &id)
{
    if (!m_engine || id == m_sessionId)
        return;

    const QString sessionDir = m_sessions->directoryOf(id);
    if (!sessionDir.isEmpty() && sessionDir != m_directory && (sessionDir == Paths::noFolderDir() || QDir(sessionDir).exists())) {
        m_directory = sessionDir;
        QJsonObject s = Paths::loadSettings();
        s.insert(QStringLiteral("lastProject"), sessionDir);
        Paths::saveSettings(s);
        Q_EMIT projectChanged();
        Q_EMIT projectsChanged();
        m_engine->start(m_directory);
    }

    m_sessionId = id;
    m_sessionTitle = m_sessions->titleOf(id);
    m_usage.clear();
    m_chat->clear();
    m_questionQueue.clear();
    m_question.clear();
    setThinking(false);
    Q_EMIT sessionChanged();
    Q_EMIT busyChanged();
    Q_EMIT usageChanged();
    Q_EMIT questionChanged();
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

    QJsonArray recents = s.value(QStringLiteral("recentProjects")).toArray();
    QStringList paths;
    paths << dir;
    for (const QJsonValue &v : recents) {
        const QString p = v.toString();
        if (p != dir && (p == Paths::noFolderDir() || QDir(p).exists()))
            paths << p;
    }
    while (paths.size() > 20)
        paths.removeLast();
    s.insert(QStringLiteral("recentProjects"), QJsonArray::fromStringList(paths));
    Paths::saveSettings(s);

    Q_EMIT projectChanged();
    Q_EMIT projectsChanged();
    switchEngine(engineId());
}

void AppController::openProjectPath(const QString &path)
{
    setProject(QUrl::fromLocalFile(path));
}

void AppController::openNoFolderProject()
{
    setProject(QUrl::fromLocalFile(Paths::noFolderDir()));
}

void AppController::removeRecentProject(const QString &path)
{
    QJsonObject s = Paths::loadSettings();
    QJsonArray recents = s.value(QStringLiteral("recentProjects")).toArray();
    QStringList paths;
    for (const QJsonValue &v : recents) {
        const QString p = v.toString();
        if (p != path)
            paths << p;
    }
    s.insert(QStringLiteral("recentProjects"), QJsonArray::fromStringList(paths));
    Paths::saveSettings(s);
    Q_EMIT projectsChanged();
}

void AppController::setProjectFilter(const QString &filter)
{
    if (m_projectFilter == filter)
        return;
    m_projectFilter = filter;
    Q_EMIT projectFilterChanged();
}

QVariantList AppController::projects() const
{
    QVariantList list;
    QSet<QString> seen;
    const QString noFolder = Paths::noFolderDir();

    QHash<QString, int> counts;
    for (const SessionModel::Row &r : m_sessions->allRows()) {
        if (!r.directory.isEmpty())
            counts[r.directory]++;
    }

    auto addProject = [&](const QString &path) {
        if (path.isEmpty() || seen.contains(path))
            return;
        if (path != noFolder && !QDir(path).exists())
            return;
        seen.insert(path);
        const bool isNo = (path == noFolder);
        const QString name = isNo ? QStringLiteral("Klasörsüz (no-folder)") : QDir(path).dirName();
        list.append(QVariantMap{
            {QStringLiteral("name"), name.isEmpty() ? path : name},
            {QStringLiteral("path"), path},
            {QStringLiteral("isNoFolder"), isNo},
            {QStringLiteral("isCurrent"), (path == m_directory)},
            {QStringLiteral("sessionCount"), counts.value(path, 0)}
        });
    };

    if (!m_directory.isEmpty())
        addProject(m_directory);
    addProject(noFolder);

    const QJsonObject s = Paths::loadSettings();
    for (const QJsonValue &v : s.value(QStringLiteral("recentProjects")).toArray())
        addProject(v.toString());

    for (auto it = counts.begin(); it != counts.end(); ++it)
        addProject(it.key());

    return list;
}

void AppController::submit(const QString &rawText)
{
    const QString trimmed = rawText.trimmed();
    if (trimmed.isEmpty())
        return;

    m_promptHistory.removeAll(trimmed);
    m_promptHistory.append(trimmed);
    while (m_promptHistory.size() > 100)
        m_promptHistory.removeFirst();
    savePromptHistory();
    Q_EMIT promptHistoryChanged();

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

void AppController::replyQuestion(const QVariantList &answers)
{
    if (m_questionQueue.isEmpty())
        return;
    const QuestionRequest r = m_questionQueue.takeFirst();
    QJsonArray answersArray;
    for (const QVariant &ans : answers) {
        if (ans.userType() == QMetaType::QVariantList || ans.canConvert<QVariantList>()) {
            QJsonArray sub;
            for (const QVariant &item : ans.toList()) {
                const QString s = item.toString().trimmed();
                if (!s.isEmpty())
                    sub.append(s);
            }
            answersArray.append(sub);
        } else {
            const QString s = ans.toString().trimmed();
            answersArray.append(s.isEmpty() ? QJsonArray{} : QJsonArray{s});
        }
    }
    if (m_engine)
        m_engine->replyQuestion(r.id, answersArray);

    if (m_questionQueue.isEmpty()) {
        m_question.clear();
    } else {
        const QuestionRequest &n = m_questionQueue.first();
        m_question = QVariantMap{{QStringLiteral("id"), n.id},
                                 {QStringLiteral("sessionId"), n.sessionId},
                                 {QStringLiteral("questions"), n.questions.toVariantList()}};
    }
    Q_EMIT questionChanged();
}

void AppController::rejectQuestion()
{
    if (m_questionQueue.isEmpty())
        return;
    const QuestionRequest r = m_questionQueue.takeFirst();
    if (m_engine)
        m_engine->rejectQuestion(r.id);

    if (m_questionQueue.isEmpty()) {
        m_question.clear();
    } else {
        const QuestionRequest &n = m_questionQueue.first();
        m_question = QVariantMap{{QStringLiteral("id"), n.id},
                                 {QStringLiteral("sessionId"), n.sessionId},
                                 {QStringLiteral("questions"), n.questions.toVariantList()}};
    }
    Q_EMIT questionChanged();
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
    const Platform::Command sh = Platform::shell(action.run);
    proc->start(sh.program, sh.arguments);
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

void AppController::loadPromptHistory()
{
    QFile f(Paths::dataDir() + QStringLiteral("/prompt_history.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
        for (const QJsonValue &v : arr) {
            const QString s = v.toString();
            if (!s.isEmpty())
                m_promptHistory.append(s);
        }
    }
}

void AppController::savePromptHistory()
{
    QFile f(Paths::dataDir() + QStringLiteral("/prompt_history.json"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonArray arr;
        for (const QString &s : m_promptHistory)
            arr.append(s);
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    }
}

void AppController::clearPromptHistory()
{
    m_promptHistory.clear();
    savePromptHistory();
    Q_EMIT promptHistoryChanged();
}

void AppController::retryLastTurn()
{
    if (busy() || !m_engine)
        return;
    const QString prompt = m_chat->lastUserPrompt();
    if (!prompt.isEmpty())
        submit(prompt);
}

void AppController::renameSession(const QString &id, const QString &newTitle)
{
    const QString trimmed = newTitle.trimmed();
    if (trimmed.isEmpty() || !m_engine)
        return;
    m_engine->renameSession(id, trimmed, [this, id, trimmed](const QString &error) {
        if (!error.isEmpty()) {
            Q_EMIT toast(QStringLiteral("Yeniden adlandırılamadı: %1").arg(error));
        } else {
            if (id == m_sessionId) {
                m_sessionTitle = trimmed;
                Q_EMIT sessionChanged();
            }
            refreshSessions();
            Q_EMIT toast(QStringLiteral("Sohbet adı güncellendi"));
        }
    });
}

void AppController::exportChatAsMarkdown()
{
    const QString md = m_chat->exportMarkdown(m_sessionTitle, projectName());
    copyText(md);
    Q_EMIT toast(QStringLiteral("Sohbet Markdown olarak panoya kopyalandı"));
}

QString AppController::formatDroppedUrl(const QUrl &url) const
{
    if (!url.isLocalFile())
        return url.toString();
    const QString local = url.toLocalFile();
    QDir projectDir(m_directory);
    if (!m_directory.isEmpty() && local.startsWith(m_directory)) {
        const QString rel = projectDir.relativeFilePath(local);
        return rel.contains(QLatin1Char(' ')) ? QStringLiteral("\"%1\"").arg(rel) : rel;
    }
    return local.contains(QLatin1Char(' ')) ? QStringLiteral("\"%1\"").arg(local) : local;
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
    m_settingsDirty = true; // the engine reads permissions at startup
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

// ---------------------------------------------------------------- store

QString AppController::storeUrl() const
{
    const QString env = qEnvironmentVariable("ORBITAL_STORE_URL");
    if (!env.isEmpty())
        return env;
    return Config::raw().value(QStringLiteral("storeUrl"))
        .toString(QStringLiteral("https://raw.githubusercontent.com/kefy266/orbital/main/store"));
}

void AppController::fetchStoreFile(const QString &path, std::function<void(const QByteArray &, const QString &)> done)
{
    const QString base = storeUrl();
    if (base.startsWith(QLatin1Char('/')) || base.startsWith(QStringLiteral("file:"))) {
        QFile f(QUrl(base).isLocalFile() ? QUrl(base).toLocalFile() + QLatin1Char('/') + path : base + QLatin1Char('/') + path);
        if (!f.open(QIODevice::ReadOnly)) {
            done({}, f.errorString());
            return;
        }
        done(f.readAll(), {});
        return;
    }
    QNetworkRequest req(QUrl(base + QLatin1Char('/') + path));
    req.setTransferTimeout(20000);
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done({}, reply->errorString());
            return;
        }
        done(reply->readAll(), {});
    });
}

static QString installedPath(const QString &type, const QString &id)
{
    return type == QLatin1String("plugin") ? Paths::pluginsDir() + QLatin1Char('/') + id + QStringLiteral("/plugin.json")
                                           : Paths::skillsDir() + QLatin1Char('/') + id + QStringLiteral("/SKILL.md");
}

QVariantList AppController::storeItems() const
{
    QVariantList out;
    for (const QVariant &v : m_store) {
        QVariantMap m = v.toMap();
        const QString id = m.value(QStringLiteral("id")).toString();
        m.insert(QStringLiteral("installed"), QFile::exists(installedPath(m.value(QStringLiteral("type")).toString(), id)));
        m.insert(QStringLiteral("busy"), m_storeBusy.contains(id));
        out << m;
    }
    return out;
}

void AppController::loadStore()
{
    m_storeLoading = true;
    m_storeError.clear();
    Q_EMIT storeChanged();
    fetchStoreFile(QStringLiteral("index.json"), [this](const QByteArray &data, const QString &error) {
        m_storeLoading = false;
        if (!error.isEmpty()) {
            m_storeError = QStringLiteral("Mağazaya ulaşılamadı: %1").arg(error);
        } else {
            m_store = QJsonDocument::fromJson(data).object().value(QStringLiteral("items")).toArray().toVariantList();
            if (m_store.isEmpty())
                m_storeError = QStringLiteral("Mağaza kataloğu okunamadı.");
        }
        Q_EMIT storeChanged();
    });
}

void AppController::installStoreItem(const QString &id)
{
    QVariantMap item;
    for (const QVariant &v : std::as_const(m_store))
        if (v.toMap().value(QStringLiteral("id")).toString() == id)
            item = v.toMap();
    if (item.isEmpty() || m_storeBusy.contains(id))
        return;
    const QString type = item.value(QStringLiteral("type")).toString();
    const QStringList files = item.value(QStringLiteral("files")).toStringList();
    const QString target = (type == QLatin1String("plugin") ? Paths::pluginsDir() : Paths::skillsDir()) + QLatin1Char('/') + id;
    const QString staging = Paths::dataDir() + QStringLiteral("/store-staging/") + id;
    QDir(staging).removeRecursively();
    m_storeBusy << id;
    Q_EMIT storeChanged();

    // Download every file into a staging folder, then move it into place in one step.
    auto remaining = std::make_shared<QStringList>(files);
    auto failed = std::make_shared<QString>();
    auto next = std::make_shared<std::function<void()>>();
    *next = [this, id, type, target, staging, remaining, failed, next] {
        if (remaining->isEmpty() || !failed->isEmpty()) {
            m_storeBusy.removeAll(id);
            if (failed->isEmpty()) {
                QDir(target).removeRecursively();
                QDir().mkpath(QFileInfo(target).absolutePath());
                QDir().rename(staging, target);
                reloadExtensions();
                refreshCommands();
                m_settingsDirty = true;
                if (type == QLatin1String("plugin"))
                    applySettings(); // plugins hook into the engine at startup
                else
                    m_settingsDirty = true;
                Q_EMIT storeItemDone(id, true, QStringLiteral("Yüklendi"));
            } else {
                QDir(staging).removeRecursively();
                Q_EMIT storeItemDone(id, false, *failed);
            }
            Q_EMIT storeChanged();
            *next = nullptr; // break the self-reference
            return;
        }
        const QString rel = remaining->takeFirst();
        if (rel.contains(QStringLiteral(".."))) {
            *failed = QStringLiteral("Geçersiz dosya yolu: %1").arg(rel);
            (*next)();
            return;
        }
        fetchStoreFile(type + QStringLiteral("s/") + id + QLatin1Char('/') + rel,
                       [staging, rel, failed, next](const QByteArray &data, const QString &error) {
                           if (!error.isEmpty()) {
                               *failed = QStringLiteral("İndirilemedi (%1): %2").arg(rel, error);
                           } else {
                               const QString path = staging + QLatin1Char('/') + rel;
                               QDir().mkpath(QFileInfo(path).absolutePath());
                               QFile f(path);
                               if (f.open(QIODevice::WriteOnly)) {
                                   f.write(data);
                                   f.close();
                                   if (rel.endsWith(QStringLiteral(".sh")))
                                       f.setPermissions(f.permissions() | QFile::ExeOwner | QFile::ExeUser);
                               } else {
                                   *failed = f.errorString();
                               }
                           }
                           (*next)();
                       });
    };
    (*next)();
}

void AppController::removeStoreItem(const QString &id)
{
    for (const QVariant &v : std::as_const(m_store)) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("id")).toString() != id)
            continue;
        const QString type = m.value(QStringLiteral("type")).toString();
        QString error;
        const bool ok = type == QLatin1String("plugin") ? m_plugins->remove(id, &error) : m_skills->remove(id, &error);
        reloadExtensions();
        refreshCommands();
        m_settingsDirty = true;
        if (type == QLatin1String("plugin"))
            applySettings();
        Q_EMIT storeItemDone(id, ok, ok ? QStringLiteral("Kaldırıldı") : error);
        Q_EMIT storeChanged();
        return;
    }
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

QString AppController::appVersion() const
{
    const QString ver = QCoreApplication::applicationVersion();
    return ver.isEmpty() ? QStringLiteral("0.3.2") : ver;
}

void AppController::checkForUpdates(bool silent)
{
    if (m_checkingUpdate || m_updating)
        return;

    m_checkingUpdate = true;
    m_updateStatus = QStringLiteral("Güncellemeler denetleniyor...");
    Q_EMIT updateChanged();

    QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com/repos/kefy266/orbital/releases/latest")));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Orbital-App"));
    req.setRawHeader("Accept", "application/vnd.github.v3+json");

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, silent]() {
        reply->deleteLater();
        m_checkingUpdate = false;

        if (reply->error() != QNetworkReply::NoError) {
            m_updateStatus = QStringLiteral("Güncelleme denetlenemedi.");
            Q_EMIT updateChanged();
            if (!silent)
                Q_EMIT toast(QStringLiteral("Güncelleme denetlenemedi: %1").arg(reply->errorString()));
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            m_updateStatus = QStringLiteral("Geçersiz güncelleme yanıtı.");
            Q_EMIT updateChanged();
            return;
        }

        const QJsonObject obj = doc.object();
        const QString tag = obj.value(QStringLiteral("tag_name")).toString();
        const QString cleanTag = tag.startsWith(QLatin1Char('v')) ? tag.mid(1) : tag;
        const QString current = appVersion();

        auto parseVer = [](const QString &v) -> QList<int> {
            QList<int> parts;
            const QStringList pieces = v.split(QLatin1Char('.'));
            for (const QString &p : pieces) {
                const QString numOnly = p.section(QRegularExpression(QStringLiteral("[^0-9]")), 0, 0);
                parts.append(numOnly.toInt());
            }
            while (parts.size() < 3)
                parts.append(0);
            return parts;
        };

        const QList<int> remote = parseVer(cleanTag);
        const QList<int> local = parseVer(current);

        bool newer = false;
        for (int i = 0; i < 3; ++i) {
            if (remote.at(i) > local.at(i)) {
                newer = true;
                break;
            } else if (remote.at(i) < local.at(i)) {
                break;
            }
        }

        if (newer) {
            m_updateVersion = cleanTag;
            m_updateNotes = obj.value(QStringLiteral("body")).toString();

            const QJsonArray assets = obj.value(QStringLiteral("assets")).toArray();
            m_updateDownloadUrl.clear();
            m_updateAssetSize = 0;

            for (const QJsonValue &v : assets) {
                const QJsonObject a = v.toObject();
                const QString name = a.value(QStringLiteral("name")).toString();
#if defined(Q_OS_WIN)
                if (name.endsWith(QStringLiteral("-windows-x64.zip"), Qt::CaseInsensitive) ||
                    name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
                    m_updateDownloadUrl = a.value(QStringLiteral("browser_download_url")).toString();
                    m_updateAssetSize = a.value(QStringLiteral("size")).toInteger();
                    break;
                }
#elif defined(Q_OS_MACOS)
                if (name.endsWith(QStringLiteral(".dmg"), Qt::CaseInsensitive)) {
                    m_updateDownloadUrl = a.value(QStringLiteral("browser_download_url")).toString();
                    m_updateAssetSize = a.value(QStringLiteral("size")).toInteger();
                    break;
                }
#else
                if (name.endsWith(QStringLiteral(".AppImage"), Qt::CaseInsensitive)) {
                    m_updateDownloadUrl = a.value(QStringLiteral("browser_download_url")).toString();
                    m_updateAssetSize = a.value(QStringLiteral("size")).toInteger();
                    break;
                }
#endif
            }

            if (!m_updateDownloadUrl.isEmpty()) {
                m_updateAvailable = true;
                m_updateStatus = QStringLiteral("Yeni sürüm mevcut: v%1").arg(m_updateVersion);
                Q_EMIT updateChanged();
                Q_EMIT toast(QStringLiteral("Yeni Orbital sürümü mevcut: v%1").arg(m_updateVersion));
            } else {
                m_updateAvailable = false;
                m_updateStatus = QStringLiteral("Platformunuz için uygun indirme paketi bulunamadı.");
                Q_EMIT updateChanged();
            }
        } else {
            m_updateAvailable = false;
            m_updateStatus = QStringLiteral("Orbital güncel (v%1).").arg(current);
            Q_EMIT updateChanged();
            if (!silent)
                Q_EMIT toast(QStringLiteral("Orbital en güncel sürümde (v%1).").arg(current));
        }
    });
}

void AppController::applyUpdate()
{
    if (m_updating || m_updateDownloadUrl.isEmpty())
        return;

    m_updating = true;
    m_updateProgress = 0.0;
    const double mb = static_cast<double>(m_updateAssetSize) / (1024.0 * 1024.0);
    m_updateStatus = mb > 0.0 ? QStringLiteral("İndiriliyor (%1 MB)...").arg(QString::number(mb, 'f', 1))
                              : QStringLiteral("İndiriliyor...");
    Q_EMIT updateChanged();
    Q_EMIT updateProgressChanged();

#if defined(Q_OS_WIN)
    const QString destFile = QDir::tempPath() + QDir::separator() + QStringLiteral("Orbital-update.zip");
#elif defined(Q_OS_MACOS)
    const QString destFile = QDir::tempPath() + QDir::separator() + QStringLiteral("Orbital-update.dmg");
#else
    const QString appImagePath = qEnvironmentVariable("APPIMAGE");
    const QString destFile = (!appImagePath.isEmpty())
                                 ? (appImagePath + QStringLiteral(".update"))
                                 : (QDir::tempPath() + QDir::separator() + QStringLiteral("Orbital-x86_64.AppImage"));
#endif

    QFile *file = new QFile(destFile, this);
    if (!file->open(QIODevice::WriteOnly)) {
        m_updating = false;
        m_updateStatus = QStringLiteral("Dosya yazılamadı: %1").arg(file->errorString());
        Q_EMIT updateChanged();
        Q_EMIT toast(m_updateStatus);
        delete file;
        return;
    }

    QNetworkRequest req;
    req.setUrl(QUrl(m_updateDownloadUrl));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Orbital-App"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            m_updateProgress = static_cast<qreal>(received) / static_cast<qreal>(total);
            m_updateStatus = QStringLiteral("İndiriliyor: %%%1").arg(qRound(m_updateProgress * 100));
            Q_EMIT updateProgressChanged();
            Q_EMIT updateChanged();
        }
    });

    connect(reply, &QNetworkReply::readyRead, file, [reply, file]() {
        file->write(reply->readAll());
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, file, destFile]() {
        reply->deleteLater();
        file->flush();
        file->close();
        file->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QFile::remove(destFile);
            m_updating = false;
            m_updateStatus = QStringLiteral("İndirme başarısız: %1").arg(reply->errorString());
            Q_EMIT updateChanged();
            Q_EMIT toast(m_updateStatus);
            return;
        }

        m_updateStatus = QStringLiteral("Güncelleme uygulanıyor, yeniden başlatılıyor...");
        Q_EMIT updateChanged();

#if defined(Q_OS_WIN)
        const QString appDir = QCoreApplication::applicationDirPath();
        const QString appExe = QCoreApplication::applicationFilePath();
        const QString batPath = QDir::tempPath() + QDir::separator() + QStringLiteral("orbital_update.bat");
        QFile bat(batPath);
        if (bat.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&bat);
            out << "@echo off\r\n";
            out << "timeout /t 2 /nobreak >nul\r\n";
            out << "tar -xf \"" << QDir::toNativeSeparators(destFile) << "\" -C \"" << QDir::toNativeSeparators(appDir) << "\"\r\n";
            out << "start \"\" \"" << QDir::toNativeSeparators(appExe) << "\"\r\n";
            out << "del \"" << QDir::toNativeSeparators(destFile) << "\"\r\n";
            out << "(goto) 2>nul & del \"%~f0\"\r\n";
            bat.close();

            QProcess::startDetached(QStringLiteral("cmd.exe"), QStringList() << QStringLiteral("/c") << batPath);
            QCoreApplication::quit();
            return;
        } else {
            m_updating = false;
            m_updateStatus = QStringLiteral("Güncelleme betiği oluşturulamadı.");
            Q_EMIT updateChanged();
            Q_EMIT toast(m_updateStatus);
        }
#elif defined(Q_OS_MACOS)
        const QString shPath = QDir::tempPath() + QDir::separator() + QStringLiteral("orbital_update.sh");
        QFile sh(shPath);
        if (sh.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&sh);
            out << "#!/usr/bin/env bash\n";
            out << "sleep 2\n";
            out << "MOUNT_DIR=$(mktemp -d)\n";
            out << "hdiutil attach \"" << destFile << "\" -mountpoint \"$MOUNT_DIR\" -nobrowse -quiet\n";
            out << "TARGET_DIR=\"/Applications\"\n";
            out << "if [ ! -w \"$TARGET_DIR\" ]; then TARGET_DIR=\"$HOME/Applications\"; mkdir -p \"$TARGET_DIR\"; fi\n";
            out << "rm -rf \"$TARGET_DIR/Orbital.app\"\n";
            out << "cp -R \"$MOUNT_DIR/Orbital.app\" \"$TARGET_DIR/\"\n";
            out << "hdiutil detach \"$MOUNT_DIR\" -quiet 2>/dev/null || true\n";
            out << "xattr -cr \"$TARGET_DIR/Orbital.app\" 2>/dev/null || true\n";
            out << "rm -rf \"" << destFile << "\" \"$MOUNT_DIR\"\n";
            out << "open \"$TARGET_DIR/Orbital.app\"\n";
            out << "rm -f \"$0\"\n";
            sh.close();

            QFile::setPermissions(shPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                              QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                                              QFileDevice::ReadOther | QFileDevice::ExeOther);

            QProcess::startDetached(QStringLiteral("/bin/bash"), QStringList() << shPath);
            QCoreApplication::quit();
            return;
        } else {
            m_updating = false;
            m_updateStatus = QStringLiteral("Güncelleme betiği oluşturulamadı.");
            Q_EMIT updateChanged();
            Q_EMIT toast(m_updateStatus);
        }
#else
        const QString appImagePath = qEnvironmentVariable("APPIMAGE");
        QFile::setPermissions(destFile, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                            QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                                            QFileDevice::ReadOther | QFileDevice::ExeOther);

        if (!appImagePath.isEmpty()) {
            QFile::remove(appImagePath);
            if (!QFile::rename(destFile, appImagePath)) {
                QFile::copy(destFile, appImagePath);
                QFile::remove(destFile);
            }
            QProcess::startDetached(appImagePath, QStringList());
            QCoreApplication::quit();
            return;
        } else {
            QProcess::startDetached(destFile, QStringList());
            QCoreApplication::quit();
            return;
        }
#endif
    });
}


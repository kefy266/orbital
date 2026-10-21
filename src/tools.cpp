#include "tools.h"

#include "mcpclient.h"
#include "pluginmanager.h"
#include "skillmanager.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

static const int kMaxOutput = 30000;

static QJsonObject schema(const QJsonObject &props, const QStringList &required)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                       {QStringLiteral("properties"), props},
                       {QStringLiteral("required"), QJsonArray::fromStringList(required)}};
}

static QJsonObject prop(const QString &type, const QString &description)
{
    return QJsonObject{{QStringLiteral("type"), type}, {QStringLiteral("description"), description}};
}

static QJsonObject tool(const QString &name, const QString &description, const QJsonObject &parameters)
{
    return QJsonObject{{QStringLiteral("name"), name},
                       {QStringLiteral("description"), description},
                       {QStringLiteral("parameters"), parameters}};
}

static QString truncate(QString s)
{
    if (s.size() > kMaxOutput)
        s = s.left(kMaxOutput) + QStringLiteral("\n… (çıktı %1 karakterde kesildi)").arg(kMaxOutput);
    return s;
}

static QString resolve(const QString &directory, const QString &path)
{
    if (path.isEmpty())
        return directory;
    QString p = path;
    if (p.startsWith(QStringLiteral("~/")))
        p = QDir::homePath() + p.mid(1);
    return QDir::cleanPath(QDir(directory).absoluteFilePath(p));
}

static QRegularExpression globToRegex(const QString &glob)
{
    QString rx;
    for (int i = 0; i < glob.size(); ++i) {
        const QChar c = glob.at(i);
        if (c == QLatin1Char('*')) {
            if (i + 1 < glob.size() && glob.at(i + 1) == QLatin1Char('*')) {
                ++i;
                if (i + 1 < glob.size() && glob.at(i + 1) == QLatin1Char('/')) {
                    ++i;
                    rx += QStringLiteral("(?:.*/)?");
                } else {
                    rx += QStringLiteral(".*");
                }
            } else {
                rx += QStringLiteral("[^/]*");
            }
        } else if (c == QLatin1Char('?')) {
            rx += QStringLiteral("[^/]");
        } else if (c == QLatin1Char('{')) {
            rx += QStringLiteral("(?:");
        } else if (c == QLatin1Char('}')) {
            rx += QLatin1Char(')');
        } else if (c == QLatin1Char(',')) {
            rx += QLatin1Char('|');
        } else {
            rx += QRegularExpression::escape(QString(c));
        }
    }
    return QRegularExpression(QStringLiteral("^") + rx + QStringLiteral("$"));
}

static bool skipDir(const QString &rel)
{
    static const QStringList skipped{QStringLiteral(".git"), QStringLiteral("node_modules"), QStringLiteral("build"),
                                     QStringLiteral("dist"), QStringLiteral(".venv"), QStringLiteral("__pycache__"),
                                     QStringLiteral("target"), QStringLiteral(".cache")};
    for (const QString &part : rel.split(QLatin1Char('/')))
        if (skipped.contains(part))
            return true;
    return false;
}

static QString mcpToolName(const QString &server, const QString &tool)
{
    QString name = server + QLatin1Char('_') + tool;
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), QStringLiteral("_"));
    return name.left(64);
}

ToolRegistry::ToolRegistry(SkillManager *skills, PluginManager *plugins, QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_skills(skills)
    , m_plugins(plugins)
    , m_nam(nam)
{
}

void ToolRegistry::reloadExtensions()
{
    qDeleteAll(m_mcp);
    m_mcp.clear();
    m_scripts.clear();

    const QJsonObject servers = m_plugins->mcpServers();
    for (auto it = servers.begin(); it != servers.end(); ++it) {
        auto *client = new McpClient(it.key(), it.value().toObject(), m_nam, this);
        connect(client, &McpClient::statusChanged, this, &ToolRegistry::extensionsChanged);
        m_mcp << client;
        client->start();
    }
    for (const auto &[dir, def] : m_plugins->scriptTools())
        m_scripts << ScriptTool{dir, def};
    Q_EMIT extensionsChanged();
}

QStringList ToolRegistry::mcpStatus() const
{
    QStringList out;
    for (McpClient *c : m_mcp)
        out << QStringLiteral("%1: %2").arg(c->name(), c->status());
    return out;
}

bool ToolRegistry::isReadOnly(const QString &name) const
{
    static const QStringList ro{QStringLiteral("read"), QStringLiteral("list"), QStringLiteral("glob"),
                                QStringLiteral("grep"), QStringLiteral("skill")};
    return ro.contains(name);
}

bool ToolRegistry::exists(const QString &name) const
{
    for (const QJsonValue &v : definitions(false))
        if (v.toObject().value(QStringLiteral("name")).toString() == name)
            return true;
    return false;
}

QJsonArray ToolRegistry::definitions(bool readOnlyAgent) const
{
    QJsonArray out;
    out << tool(QStringLiteral("read"), QStringLiteral("Read a text file. Returns numbered lines. Use offset/limit for large files."),
                schema({{QStringLiteral("path"), prop(QStringLiteral("string"), QStringLiteral("File path, relative to the project or absolute"))},
                        {QStringLiteral("offset"), prop(QStringLiteral("integer"), QStringLiteral("1-based first line (optional)"))},
                        {QStringLiteral("limit"), prop(QStringLiteral("integer"), QStringLiteral("Max lines, default 2000 (optional)"))}},
                       {QStringLiteral("path")}));
    out << tool(QStringLiteral("list"), QStringLiteral("List the entries of a directory."),
                schema({{QStringLiteral("path"), prop(QStringLiteral("string"), QStringLiteral("Directory, default project root"))}}, {}));
    out << tool(QStringLiteral("glob"), QStringLiteral("Find files by glob pattern such as **/*.cpp or src/*.{h,cpp}."),
                schema({{QStringLiteral("pattern"), prop(QStringLiteral("string"), QStringLiteral("Glob pattern"))},
                        {QStringLiteral("path"), prop(QStringLiteral("string"), QStringLiteral("Directory to search (optional)"))}},
                       {QStringLiteral("pattern")}));
    out << tool(QStringLiteral("grep"), QStringLiteral("Search file contents with a regular expression. Returns file:line: text."),
                schema({{QStringLiteral("pattern"), prop(QStringLiteral("string"), QStringLiteral("Regular expression"))},
                        {QStringLiteral("path"), prop(QStringLiteral("string"), QStringLiteral("Directory to search (optional)"))},
                        {QStringLiteral("include"), prop(QStringLiteral("string"), QStringLiteral("Glob filter for file names, e.g. *.py (optional)"))}},
                       {QStringLiteral("pattern")}));
    if (!m_skills->skills().isEmpty())
        out << tool(QStringLiteral("skill"), QStringLiteral("Load the full instructions of an available skill by name before doing a task it covers."),
                    schema({{QStringLiteral("name"), prop(QStringLiteral("string"), QStringLiteral("Skill name"))}}, {QStringLiteral("name")}));
    if (readOnlyAgent)
        return out;

    out << tool(QStringLiteral("write"), QStringLiteral("Create or overwrite a file with the given content."),
                schema({{QStringLiteral("path"), prop(QStringLiteral("string"), QStringLiteral("File path"))},
                        {QStringLiteral("content"), prop(QStringLiteral("string"), QStringLiteral("Full file content"))}},
                       {QStringLiteral("path"), QStringLiteral("content")}));
    out << tool(QStringLiteral("edit"), QStringLiteral("Replace an exact string in a file. oldString must match exactly and be unique unless replaceAll is true."),
                schema({{QStringLiteral("path"), prop(QStringLiteral("string"), QStringLiteral("File path"))},
                        {QStringLiteral("oldString"), prop(QStringLiteral("string"), QStringLiteral("Exact text to replace"))},
                        {QStringLiteral("newString"), prop(QStringLiteral("string"), QStringLiteral("Replacement text"))},
                        {QStringLiteral("replaceAll"), prop(QStringLiteral("boolean"), QStringLiteral("Replace every occurrence (optional)"))}},
                       {QStringLiteral("path"), QStringLiteral("oldString"), QStringLiteral("newString")}));
    out << tool(QStringLiteral("bash"), QStringLiteral("Run a shell command in the project directory and return its output."),
                schema({{QStringLiteral("command"), prop(QStringLiteral("string"), QStringLiteral("Shell command"))},
                        {QStringLiteral("timeout"), prop(QStringLiteral("integer"), QStringLiteral("Timeout in seconds, default 120 (optional)"))}},
                       {QStringLiteral("command")}));

    for (const ScriptTool &s : m_scripts)
        out << tool(s.def.value(QStringLiteral("name")).toString(), s.def.value(QStringLiteral("description")).toString(),
                    s.def.value(QStringLiteral("parameters")).toObject(schema({}, {})));
    for (McpClient *c : m_mcp) {
        for (const QJsonValue &v : c->tools()) {
            const QJsonObject t = v.toObject();
            out << tool(mcpToolName(c->name(), t.value(QStringLiteral("name")).toString()),
                        t.value(QStringLiteral("description")).toString(),
                        t.value(QStringLiteral("inputSchema")).toObject(schema({}, {})));
        }
    }
    return out;
}

QString ToolRegistry::describe(const QString &name, const QJsonObject &args)
{
    for (const char *key : {"command", "path", "pattern", "name", "url", "query"}) {
        const QString v = args.value(QLatin1String(key)).toString();
        if (!v.isEmpty())
            return v.simplified().left(120);
    }
    return QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact)).left(120);
    Q_UNUSED(name);
}

void ToolRegistry::abortRunning()
{
    for (const QPointer<QProcess> &p : std::as_const(m_running))
        if (p)
            p->kill();
    m_running.clear();
}

void ToolRegistry::runProcess(const QString &program, const QStringList &args, const QString &cwd,
                              const QByteArray &stdinData, int timeoutMs, ToolDone done,
                              const QHash<QString, QString> &env)
{
    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(cwd);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment e = QProcessEnvironment::systemEnvironment();
    for (auto it = env.begin(); it != env.end(); ++it)
        e.insert(it.key(), it.value());
    proc->setProcessEnvironment(e);
    m_running << proc;

    auto *timer = new QTimer(proc);
    timer->setSingleShot(true);
    auto timedOut = std::make_shared<bool>(false);
    connect(timer, &QTimer::timeout, proc, [proc, timedOut] {
        *timedOut = true;
        proc->kill();
    });
    connect(proc, &QProcess::errorOccurred, proc, [proc, done](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart) {
            done(QStringLiteral("Komut başlatılamadı: %1").arg(proc->program()), true);
            proc->deleteLater();
        }
    });
    connect(proc, &QProcess::finished, proc, [proc, done, timedOut](int code, QProcess::ExitStatus status) {
        QString out = truncate(QString::fromUtf8(proc->readAll()));
        if (*timedOut)
            out += QStringLiteral("\n(zaman aşımı nedeniyle sonlandırıldı)");
        else if (status == QProcess::CrashExit)
            out += QStringLiteral("\n(süreç sonlandırıldı)");
        else if (code != 0)
            out += QStringLiteral("\n(çıkış kodu %1)").arg(code);
        done(out.isEmpty() ? QStringLiteral("(çıktı yok)") : out, *timedOut || code != 0);
        proc->deleteLater();
    });
    proc->start(program, args);
    if (!stdinData.isNull()) {
        proc->write(stdinData);
        proc->closeWriteChannel();
    }
    timer->start(timeoutMs);
}

void ToolRegistry::execute(const QString &name, const QJsonObject &args, const QString &directory, ToolDone done)
{
    const auto str = [&args](const char *key) { return args.value(QLatin1String(key)).toString(); };

    if (name == QLatin1String("read")) {
        QFile f(resolve(directory, str("path")));
        if (!f.open(QIODevice::ReadOnly))
            return done(QStringLiteral("Dosya okunamadı: %1").arg(f.errorString()), true);
        const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
        const int offset = qMax(1, args.value(QStringLiteral("offset")).toInt(1));
        const int limit = qMax(1, args.value(QStringLiteral("limit")).toInt(2000));
        QString out;
        for (int i = offset - 1; i < lines.size() && i < offset - 1 + limit; ++i)
            out += QStringLiteral("%1\t%2\n").arg(i + 1, 5).arg(lines.at(i).left(2000));
        if (offset - 1 + limit < lines.size())
            out += QStringLiteral("… (%1 satır daha; offset ile devam et)\n").arg(lines.size() - (offset - 1 + limit));
        return done(truncate(out), false);
    }
    if (name == QLatin1String("list")) {
        QDir dir(resolve(directory, str("path")));
        if (!dir.exists())
            return done(QStringLiteral("Klasör bulunamadı."), true);
        QStringList out;
        for (const QFileInfo &fi : dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                                                     QDir::DirsFirst | QDir::Name))
            out << (fi.isDir() ? fi.fileName() + QLatin1Char('/') : fi.fileName());
        return done(out.join(QLatin1Char('\n')), false);
    }
    if (name == QLatin1String("glob") || name == QLatin1String("grep")) {
        const QString root = resolve(directory, str("path"));
        const bool isGrep = name == QLatin1String("grep");
        const QRegularExpression fileRx = globToRegex(isGrep ? (str("include").isEmpty() ? QStringLiteral("**") : QStringLiteral("**/") + str("include"))
                                                             : str("pattern"));
        const QRegularExpression contentRx(str("pattern"));
        if (isGrep && !contentRx.isValid())
            return done(QStringLiteral("Geçersiz regex: %1").arg(contentRx.errorString()), true);
        QStringList out;
        QDirIterator it(root, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext() && out.size() < 300) {
            const QString file = it.next();
            const QString rel = QDir(root).relativeFilePath(file);
            if (skipDir(rel) || !fileRx.match(rel).hasMatch())
                continue;
            if (!isGrep) {
                out << rel;
                continue;
            }
            QFile f(file);
            if (f.size() > 2 * 1024 * 1024 || !f.open(QIODevice::ReadOnly))
                continue;
            const QByteArray data = f.readAll();
            if (data.left(4096).contains('\0'))
                continue;
            int lineNo = 0;
            for (const QString &line : QString::fromUtf8(data).split(QLatin1Char('\n'))) {
                ++lineNo;
                if (contentRx.match(line).hasMatch()) {
                    out << QStringLiteral("%1:%2: %3").arg(rel).arg(lineNo).arg(line.trimmed().left(300));
                    if (out.size() >= 300)
                        break;
                }
            }
        }
        if (out.isEmpty())
            return done(QStringLiteral("Eşleşme yok."), false);
        return done(truncate(out.join(QLatin1Char('\n'))), false);
    }
    if (name == QLatin1String("skill")) {
        const Skill *s = m_skills->find(str("name"));
        if (!s || !s->enabled)
            return done(QStringLiteral("Böyle bir skill yok: %1").arg(str("name")), true);
        return done(QStringLiteral("<skill name=\"%1\" dir=\"%2\">\n%3\n</skill>").arg(s->name, s->dir, s->body), false);
    }
    if (name == QLatin1String("write")) {
        const QString path = resolve(directory, str("path"));
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return done(QStringLiteral("Yazılamadı: %1").arg(f.errorString()), true);
        f.write(str("content").toUtf8());
        f.close(); // flush before the next tool in this turn reads the file
        return done(QStringLiteral("Yazıldı: %1 (%2 bayt)").arg(str("path")).arg(str("content").toUtf8().size()), false);
    }
    if (name == QLatin1String("edit")) {
        const QString path = resolve(directory, str("path"));
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return done(QStringLiteral("Dosya okunamadı: %1").arg(f.errorString()), true);
        QString text = QString::fromUtf8(f.readAll());
        f.close();
        const QString oldS = str("oldString");
        const int count = text.count(oldS);
        if (oldS.isEmpty() || count == 0)
            return done(QStringLiteral("oldString dosyada bulunamadı."), true);
        if (count > 1 && !args.value(QStringLiteral("replaceAll")).toBool())
            return done(QStringLiteral("oldString %1 kez geçiyor; daha fazla bağlam ver veya replaceAll kullan.").arg(count), true);
        if (args.value(QStringLiteral("replaceAll")).toBool())
            text.replace(oldS, str("newString"));
        else
            text.replace(text.indexOf(oldS), oldS.size(), str("newString"));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return done(QStringLiteral("Yazılamadı: %1").arg(f.errorString()), true);
        f.write(text.toUtf8());
        f.close();
        return done(QStringLiteral("Düzenlendi: %1 (%2 değişiklik)").arg(str("path")).arg(args.value(QStringLiteral("replaceAll")).toBool() ? count : 1), false);
    }
    if (name == QLatin1String("bash")) {
        const int timeout = qBound(1, args.value(QStringLiteral("timeout")).toInt(120), 1800) * 1000;
        return runProcess(QStringLiteral("/bin/bash"), {QStringLiteral("-c"), str("command")}, directory, QByteArray(), timeout, done);
    }
    for (const ScriptTool &s : std::as_const(m_scripts)) {
        if (s.def.value(QStringLiteral("name")).toString() != name)
            continue;
        const QString run = QDir(s.pluginDir).absoluteFilePath(s.def.value(QStringLiteral("run")).toString());
        return runProcess(run, {}, directory, QJsonDocument(args).toJson(QJsonDocument::Compact),
                          s.def.value(QStringLiteral("timeout")).toInt(120) * 1000, done,
                          {{QStringLiteral("ORBITAL_PLUGIN_DIR"), s.pluginDir}, {QStringLiteral("ORBITAL_PROJECT_DIR"), directory}});
    }
    for (McpClient *c : std::as_const(m_mcp)) {
        for (const QJsonValue &v : c->tools()) {
            const QString toolName = v.toObject().value(QStringLiteral("name")).toString();
            if (mcpToolName(c->name(), toolName) == name)
                return c->callTool(toolName, args, done);
        }
    }
    done(QStringLiteral("Bilinmeyen araç: %1").arg(name), true);
}

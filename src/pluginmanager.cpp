#include "pluginmanager.h"

#include "paths.h"
#include "skillmanager.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>

static QStringList disabledPlugins()
{
    QStringList out;
    for (const QJsonValue &v : Paths::loadSettings().value(QStringLiteral("disabledPlugins")).toArray())
        out << v.toString();
    return out;
}

QString Plugin::skillsRoot() const
{
    const QString rel = manifest.value(QStringLiteral("skills")).toString();
    if (rel.isEmpty())
        return {};
    return QDir(dir).absoluteFilePath(rel);
}

QStringList Plugin::summary() const
{
    QStringList out;
    const auto count = [this](const char *key) {
        const QJsonValue v = manifest.value(QLatin1String(key));
        return v.isObject() ? v.toObject().size() : v.toArray().size();
    };
    if (int n = count("commands"))
        out << QObject::tr("%1 komut").arg(n);
    if (int n = count("mcp"))
        out << QObject::tr("%1 MCP sunucusu").arg(n);
    if (!skillsRoot().isEmpty())
        out << QObject::tr("skill paketi");
    if (int n = count("actions"))
        out << QObject::tr("%1 eylem").arg(n);
    if (int n = count("instructions"))
        out << QObject::tr("%1 talimat").arg(n);
    if (int n = count("tools"))
        out << QObject::tr("%1 araç").arg(n);
    if (int n = count("opencodePlugins"))
        out << QObject::tr("%1 sunucu eklentisi").arg(n);
    if (manifest.value(QStringLiteral("hooks")).toObject().contains(QStringLiteral("beforePrompt")))
        out << QObject::tr("prompt hook");
    return out;
}

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
{
}

void PluginManager::reload()
{
    m_plugins.clear();
    const QStringList disabled = disabledPlugins();
    const QDir root(Paths::pluginsDir());
    for (const QString &sub : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        QFile f(root.filePath(sub + QStringLiteral("/plugin.json")));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        Plugin p;
        p.dir = root.filePath(sub);
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        if (err.error != QJsonParseError::NoError) {
            p.id = sub;
            p.name = sub;
            p.error = tr("plugin.json okunamadı: %1").arg(err.errorString());
            p.enabled = false;
            m_plugins << p;
            continue;
        }
        p.manifest = doc.object();
        p.id = p.manifest.value(QStringLiteral("id")).toString(sub);
        p.name = p.manifest.value(QStringLiteral("name")).toString(p.id);
        p.version = p.manifest.value(QStringLiteral("version")).toString();
        p.description = p.manifest.value(QStringLiteral("description")).toString();
        p.author = p.manifest.value(QStringLiteral("author")).toString();
        p.enabled = !disabled.contains(p.id);
        m_plugins << p;
    }
    Q_EMIT changed();
}

const Plugin *PluginManager::find(const QString &id) const
{
    for (const Plugin &p : m_plugins)
        if (p.id == id)
            return &p;
    return nullptr;
}

void PluginManager::setEnabled(const QString &id, bool enabled)
{
    QJsonObject settings = Paths::loadSettings();
    QStringList disabled = disabledPlugins();
    disabled.removeAll(id);
    if (!enabled)
        disabled << id;
    settings.insert(QStringLiteral("disabledPlugins"), QJsonArray::fromStringList(disabled));
    Paths::saveSettings(settings);
    reload();
}

bool PluginManager::install(const QString &folder, QString *error)
{
    QFile f(folder + QStringLiteral("/plugin.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        *error = tr("Klasörde plugin.json bulunamadı.");
        return false;
    }
    const QJsonObject manifest = QJsonDocument::fromJson(f.readAll()).object();
    const QString id = SkillManager::sanitizeName(manifest.value(QStringLiteral("id")).toString(QFileInfo(folder).fileName()));
    const QString target = Paths::pluginsDir() + QLatin1Char('/') + id;
    if (QFileInfo::exists(target)) {
        *error = tr("'%1' kimlikli bir eklenti zaten kurulu.").arg(id);
        return false;
    }
    QDirIterator it(folder, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString src = it.next();
        if (src.contains(QStringLiteral("/.git/")))
            continue;
        const QString dst = target + QLatin1Char('/') + QDir(folder).relativeFilePath(src);
        QDir().mkpath(QFileInfo(dst).absolutePath());
        QFile::copy(src, dst);
    }
    reload();
    return true;
}

bool PluginManager::create(const QString &rawId, const QString &name, QString *error)
{
    const QString id = SkillManager::sanitizeName(rawId);
    if (id.isEmpty()) {
        *error = tr("Eklenti kimliği boş olamaz.");
        return false;
    }
    QDir root(Paths::pluginsDir());
    if (root.exists(id)) {
        *error = tr("'%1' kimlikli bir eklenti zaten var.").arg(id);
        return false;
    }
    root.mkpath(id + QStringLiteral("/skills"));
    const QJsonObject manifest{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name.isEmpty() ? id : name},
        {QStringLiteral("version"), QStringLiteral("0.1.0")},
        {QStringLiteral("description"), tr("Yeni Orbital eklentisi")},
        {QStringLiteral("skills"), QStringLiteral("skills")},
        {QStringLiteral("commands"), QJsonObject{
             {QStringLiteral("merhaba"), QJsonObject{
                  {QStringLiteral("description"), tr("Örnek komut")},
                  {QStringLiteral("template"), tr("Kullanıcıyı selamla ve şunu özetle: $ARGUMENTS")}}}}},
        {QStringLiteral("mcp"), QJsonObject{}},
        {QStringLiteral("actions"), QJsonArray{}},
    };
    QFile f(root.filePath(id + QStringLiteral("/plugin.json")));
    if (!f.open(QIODevice::WriteOnly)) {
        *error = f.errorString();
        return false;
    }
    f.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    f.close();
    reload();
    return true;
}

bool PluginManager::remove(const QString &id, QString *error)
{
    const Plugin *p = find(id);
    if (!p) {
        *error = tr("Eklenti bulunamadı.");
        return false;
    }
    QDir(p->dir).removeRecursively();
    reload();
    return true;
}

QList<QPair<QString, QString>> PluginManager::skillRoots() const
{
    QList<QPair<QString, QString>> out;
    for (const Plugin &p : m_plugins)
        if (p.enabled && !p.skillsRoot().isEmpty())
            out << qMakePair(p.id, p.skillsRoot());
    return out;
}

QList<PluginAction> PluginManager::actions() const
{
    QList<PluginAction> out;
    for (const Plugin &p : m_plugins) {
        if (!p.enabled)
            continue;
        for (const QJsonValue &v : p.manifest.value(QStringLiteral("actions")).toArray()) {
            const QJsonObject o = v.toObject();
            out << PluginAction{p.id, o.value(QStringLiteral("label")).toString(), o.value(QStringLiteral("run")).toString(),
                                o.value(QStringLiteral("mode")).toString(QStringLiteral("insert"))};
        }
    }
    return out;
}

QJsonObject PluginManager::commands() const
{
    QJsonObject out;
    for (const Plugin &p : m_plugins) {
        if (!p.enabled)
            continue;
        const QJsonObject cmds = p.manifest.value(QStringLiteral("commands")).toObject();
        for (auto it = cmds.begin(); it != cmds.end(); ++it)
            out.insert(it.key(), it.value());
    }
    return out;
}

QJsonObject PluginManager::mcpServers() const
{
    QJsonObject out;
    for (const Plugin &p : m_plugins) {
        if (!p.enabled)
            continue;
        const QDir dir(p.dir);
        const QJsonObject mcp = p.manifest.value(QStringLiteral("mcp")).toObject();
        for (auto it = mcp.begin(); it != mcp.end(); ++it) {
            QJsonObject server = it.value().toObject();
            if (!server.contains(QStringLiteral("type")))
                server.insert(QStringLiteral("type"), server.contains(QStringLiteral("url")) ? QStringLiteral("remote")
                                                                                          : QStringLiteral("local"));
            // Relative script paths in a local server command resolve against the plugin folder.
            if (server.value(QStringLiteral("type")).toString() == QLatin1String("local")) {
                QJsonArray cmd = server.value(QStringLiteral("command")).toArray();
                for (int i = 0; i < cmd.size(); ++i) {
                    const QString arg = cmd.at(i).toString();
                    if (arg.startsWith(QStringLiteral("./")))
                        cmd[i] = dir.absoluteFilePath(arg);
                }
                server.insert(QStringLiteral("command"), cmd);
                if (!server.contains(QStringLiteral("cwd")))
                    server.insert(QStringLiteral("cwd"), p.dir);
            }
            out.insert(it.key(), server);
        }
    }
    return out;
}

QList<QPair<QString, QJsonObject>> PluginManager::scriptTools() const
{
    QList<QPair<QString, QJsonObject>> out;
    for (const Plugin &p : m_plugins) {
        if (!p.enabled)
            continue;
        for (const QJsonValue &v : p.manifest.value(QStringLiteral("tools")).toArray())
            out << qMakePair(p.dir, v.toObject());
    }
    return out;
}

QStringList PluginManager::instructionFiles() const
{
    QStringList out;
    for (const Plugin &p : m_plugins) {
        if (!p.enabled)
            continue;
        for (const QJsonValue &v : p.manifest.value(QStringLiteral("instructions")).toArray())
            out << QDir(p.dir).absoluteFilePath(v.toString());
    }
    return out;
}

QStringList PluginManager::opencodePlugins() const
{
    QStringList out;
    for (const Plugin &p : m_plugins) {
        if (!p.enabled)
            continue;
        for (const QJsonValue &v : p.manifest.value(QStringLiteral("opencodePlugins")).toArray()) {
            const QString spec = v.toString();
            out << (spec.startsWith(QStringLiteral("./")) ? QStringLiteral("file://") + QDir(p.dir).absoluteFilePath(spec) : spec);
        }
    }
    return out;
}

bool PluginManager::runBeforePrompt(QString *text, const QString &sessionId, const QString &directory,
                                    QString *reason) const
{
    for (const Plugin &p : m_plugins) {
        if (!p.enabled)
            continue;
        const QString hook = p.manifest.value(QStringLiteral("hooks")).toObject().value(QStringLiteral("beforePrompt")).toString();
        if (hook.isEmpty())
            continue;

        QProcess proc;
        proc.setWorkingDirectory(directory);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("ORBITAL_PLUGIN_DIR"), p.dir);
        proc.setProcessEnvironment(env);
        proc.start(QDir(p.dir).absoluteFilePath(hook), {});
        if (!proc.waitForStarted(3000)) {
            *reason = tr("%1 hook'u başlatılamadı: %2").arg(p.name, hook);
            return false;
        }
        const QJsonObject input{{QStringLiteral("text"), *text},
                                {QStringLiteral("sessionID"), sessionId},
                                {QStringLiteral("directory"), directory}};
        proc.write(QJsonDocument(input).toJson(QJsonDocument::Compact));
        proc.closeWriteChannel();
        if (!proc.waitForFinished(15000)) {
            proc.kill();
            *reason = tr("%1 hook'u zaman aşımına uğradı.").arg(p.name);
            return false;
        }
        if (proc.exitCode() != 0) {
            *reason = tr("%1 gönderimi engelledi: %2").arg(p.name, QString::fromUtf8(proc.readAllStandardError()).trimmed());
            return false;
        }
        const QString out = QString::fromUtf8(proc.readAllStandardOutput());
        if (!out.trimmed().isEmpty())
            *text = out.trimmed();
    }
    return true;
}

#include "paths.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

namespace Paths {

QString configDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    QDir dir(base + QStringLiteral("/orbital"));
    dir.mkpath(QStringLiteral("."));
    return dir.absolutePath();
}

QString skillsDir()
{
    QDir dir(configDir() + QStringLiteral("/skills"));
    dir.mkpath(QStringLiteral("."));
    return dir.absolutePath();
}

QString pluginsDir()
{
    QDir dir(configDir() + QStringLiteral("/plugins"));
    dir.mkpath(QStringLiteral("."));
    return dir.absolutePath();
}

QString dataDir()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/orbital"));
    dir.mkpath(QStringLiteral("."));
    return dir.absolutePath();
}

QString noFolderDir()
{
    // Try /orbital/no-folder if existing or writable
    QDir rootDir(QStringLiteral("/orbital/no-folder"));
    if (rootDir.exists())
        return rootDir.absolutePath();
    if (rootDir.mkpath(QStringLiteral(".")))
        return rootDir.absolutePath();

    // Default to user data dir: ~/.local/share/orbital/no-folder
    QDir userDir(dataDir() + QStringLiteral("/no-folder"));
    userDir.mkpath(QStringLiteral("."));
    return userDir.absolutePath();
}

QString settingsFile()
{
    return configDir() + QStringLiteral("/settings.json");
}

static void copyTree(const QString &from, const QString &to)
{
    QDirIterator it(from, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString src = it.next();
        const QString rel = QDir(from).relativeFilePath(src);
        const QString dst = to + QLatin1Char('/') + rel;
        QDir().mkpath(QFileInfo(dst).absolutePath());
        if (QFile::exists(dst))
            continue;
        QFile::copy(src, dst);
        QFile::setPermissions(dst, QFile::permissions(dst) | QFile::WriteOwner
                                       | (rel.endsWith(QStringLiteral(".sh")) ? QFile::ExeOwner : QFile::Permissions()));
    }
}

void installBundledContent()
{
    copyTree(QStringLiteral(":/bundled/skills"), skillsDir());
    copyTree(QStringLiteral(":/bundled/plugins"), pluginsDir());

    // Plugins that need extra setup (or are risky, like oto-mod) ship disabled
    // ("enabledByDefault": false). Each bundled plugin gets this default once, the first
    // time this install sees it, so plugins added by an update start out disabled too.
    QJsonObject settings = loadSettings();
    QJsonArray disabled = settings.value(QStringLiteral("disabledPlugins")).toArray();
    QJsonArray seen = settings.value(QStringLiteral("bundledPluginsSeen")).toArray();
    const bool firstRun = !settings.value(QStringLiteral("bundledInstalled")).toBool();
    const bool legacy = !firstRun && !settings.contains(QStringLiteral("bundledPluginsSeen"));
    bool changed = firstRun;
    const QDir bundled(QStringLiteral(":/bundled/plugins"));
    for (const QString &sub : bundled.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile f(bundled.filePath(sub + QStringLiteral("/plugin.json")));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject m = QJsonDocument::fromJson(f.readAll()).object();
        const QString id = m.value(QStringLiteral("id")).toString(sub);
        if (seen.contains(id))
            continue;
        seen << id;
        changed = true;
        // Installs from before this list existed already applied every default except
        // oto-mod's (it lacked the flag); don't touch what their users chose since.
        const bool known = legacy && id != QLatin1String("oto-mod");
        if (!known && !m.value(QStringLiteral("enabledByDefault")).toBool(true) && !disabled.contains(id))
            disabled << id;
    }
    if (!changed)
        return;
    settings.insert(QStringLiteral("disabledPlugins"), disabled);
    settings.insert(QStringLiteral("bundledPluginsSeen"), seen);
    settings.insert(QStringLiteral("bundledInstalled"), true);
    saveSettings(settings);
}

QJsonObject loadSettings()
{
    QFile f(settingsFile());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

void saveSettings(const QJsonObject &settings)
{
    QFile f(settingsFile());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(settings).toJson(QJsonDocument::Indented));
}

}

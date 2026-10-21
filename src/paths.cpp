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

    QJsonObject settings = loadSettings();
    if (settings.value(QStringLiteral("bundledInstalled")).toBool())
        return;
    // Plugins that need extra setup ship disabled ("enabledByDefault": false).
    QJsonArray disabled = settings.value(QStringLiteral("disabledPlugins")).toArray();
    const QDir bundled(QStringLiteral(":/bundled/plugins"));
    for (const QString &sub : bundled.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile f(bundled.filePath(sub + QStringLiteral("/plugin.json")));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject m = QJsonDocument::fromJson(f.readAll()).object();
        if (!m.value(QStringLiteral("enabledByDefault")).toBool(true))
            disabled << m.value(QStringLiteral("id")).toString(sub);
    }
    settings.insert(QStringLiteral("disabledPlugins"), disabled);
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

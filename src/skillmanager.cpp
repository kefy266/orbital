#include "skillmanager.h"

#include "paths.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QRegularExpression>

static QStringList disabledSkills()
{
    QStringList out;
    for (const QJsonValue &v : Paths::loadSettings().value(QStringLiteral("disabledSkills")).toArray())
        out << v.toString();
    return out;
}

SkillManager::SkillManager(QObject *parent)
    : QObject(parent)
{
}

QString SkillManager::sanitizeName(const QString &name)
{
    QString out = name.trimmed().toLower();
    out.replace(QRegularExpression(QStringLiteral("[^a-z0-9._-]+")), QStringLiteral("-"));
    out.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    return out;
}

bool SkillManager::parse(const QString &skillMdPath, Skill *out)
{
    QFile f(skillMdPath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QString text = QString::fromUtf8(f.readAll());
    out->dir = QFileInfo(skillMdPath).absolutePath();
    out->name = QFileInfo(out->dir).fileName();
    out->body = text;

    static const QRegularExpression front(QStringLiteral("\\A---\\s*\\n(.*?)\\n---\\s*\\n?"),
                                          QRegularExpression::DotMatchesEverythingOption);
    const auto m = front.match(text);
    if (m.hasMatch()) {
        out->body = text.mid(m.capturedLength()).trimmed();
        for (const QString &line : m.captured(1).split(QLatin1Char('\n'))) {
            const int colon = line.indexOf(QLatin1Char(':'));
            if (colon < 0)
                continue;
            const QString key = line.left(colon).trimmed();
            QString value = line.mid(colon + 1).trimmed();
            if (value.size() >= 2 && (value.startsWith(QLatin1Char('"')) || value.startsWith(QLatin1Char('\''))))
                value = value.mid(1, value.size() - 2);
            if (key == QLatin1String("name"))
                out->name = value;
            else if (key == QLatin1String("description"))
                out->description = value;
        }
    }
    return true;
}

void SkillManager::setPluginSkillRoots(const QList<QPair<QString, QString>> &pluginIdAndRoot)
{
    m_pluginRoots = pluginIdAndRoot;
}

void SkillManager::scanRoot(const QString &root, const QString &origin)
{
    const QStringList disabled = disabledSkills();
    const QDir dir(root);
    for (const QString &sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const QString md = dir.filePath(sub + QStringLiteral("/SKILL.md"));
        Skill s;
        if (!QFile::exists(md) || !parse(md, &s))
            continue;
        s.origin = origin;
        s.enabled = !disabled.contains(s.name);
        m_skills << s;
    }
}

void SkillManager::reload()
{
    m_skills.clear();
    scanRoot(Paths::skillsDir(), QStringLiteral("orbital"));
    for (const auto &[pluginId, root] : m_pluginRoots)
        scanRoot(root, pluginId);
    Q_EMIT changed();
}

const Skill *SkillManager::find(const QString &name) const
{
    for (const Skill &s : m_skills)
        if (s.name == name)
            return &s;
    return nullptr;
}

void SkillManager::setEnabled(const QString &name, bool enabled)
{
    QJsonObject settings = Paths::loadSettings();
    QStringList disabled = disabledSkills();
    disabled.removeAll(name);
    if (!enabled)
        disabled << name;
    settings.insert(QStringLiteral("disabledSkills"), QJsonArray::fromStringList(disabled));
    Paths::saveSettings(settings);
    reload();
}

static QString frontmatter(const QString &name, const QString &description, const QString &body)
{
    QString desc = description.simplified();
    desc.replace(QLatin1Char('"'), QLatin1Char('\''));
    return QStringLiteral("---\nname: %1\ndescription: \"%2\"\n---\n\n%3\n").arg(name, desc, body.trimmed());
}

bool SkillManager::save(const QString &originalName, const QString &rawName, const QString &description,
                        const QString &body, QString *error)
{
    const QString name = sanitizeName(rawName);
    if (name.isEmpty()) {
        *error = tr("Skill adı boş olamaz.");
        return false;
    }
    if (description.trimmed().isEmpty()) {
        *error = tr("Açıklama gerekli: model skill'i ne zaman kullanacağını buradan anlar.");
        return false;
    }
    QDir root(Paths::skillsDir());
    if (!originalName.isEmpty() && originalName != name) {
        if (root.exists(name)) {
            *error = tr("'%1' adında bir skill zaten var.").arg(name);
            return false;
        }
        const Skill *old = find(originalName);
        if (old && old->editable())
            QDir().rename(old->dir, root.filePath(name));
    } else if (originalName.isEmpty() && root.exists(name)) {
        *error = tr("'%1' adında bir skill zaten var.").arg(name);
        return false;
    }
    root.mkpath(name);
    QFile f(root.filePath(name + QStringLiteral("/SKILL.md")));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = f.errorString();
        return false;
    }
    f.write(frontmatter(name, description, body).toUtf8());
    f.close();
    reload();
    return true;
}

bool SkillManager::remove(const QString &name, QString *error)
{
    const Skill *s = find(name);
    if (!s || !s->editable()) {
        *error = tr("Bu skill bir eklentiye ait; eklentiyi devre dışı bırakın.");
        return false;
    }
    QDir(s->dir).removeRecursively();
    reload();
    return true;
}

bool SkillManager::importFolder(const QString &folder, QString *error, bool replace, QString *installedName)
{
    Skill probe;
    if (!parse(folder + QStringLiteral("/SKILL.md"), &probe)) {
        *error = tr("Klasörde SKILL.md bulunamadı.");
        return false;
    }
    const QString name = sanitizeName(probe.name);
    const QString target = Paths::skillsDir() + QLatin1Char('/') + name;
    if (QFileInfo::exists(target)) {
        if (!replace) {
            *error = tr("'%1' adında bir skill zaten var.").arg(name);
            return false;
        }
        QDir(target).removeRecursively();
    }
    if (installedName)
        *installedName = name;
    QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString src = it.next();
        const QString rel = QDir(folder).relativeFilePath(src);
        if (rel != QLatin1String("SKILL.md") && rel.endsWith(QLatin1String("/SKILL.md"), Qt::CaseInsensitive))
            continue;
        const QString dst = target + QLatin1Char('/') + rel;
        QDir().mkpath(QFileInfo(dst).absolutePath());
        QFile::copy(src, dst);
    }
    reload();
    return true;
}

QStringList SkillManager::enabledSkillDirs() const
{
    QStringList out;
    for (const Skill &s : m_skills)
        if (s.enabled)
            out << s.dir;
    return out;
}

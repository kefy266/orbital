#pragma once

#include <QList>
#include <QObject>
#include <QString>

// A skill is a folder holding a SKILL.md: YAML-ish frontmatter (name, description)
// followed by markdown instructions. Enabled skills are handed to opencode,
// which exposes them to the model and lets the user attach them to prompts.
struct Skill
{
    QString name;
    QString description;
    QString body;
    QString dir;      // absolute folder containing SKILL.md
    QString origin;   // "orbital" or the plugin id that bundles it
    bool enabled = true;

    bool editable() const { return origin == QLatin1String("orbital"); }
};

class SkillManager : public QObject
{
    Q_OBJECT
public:
    explicit SkillManager(QObject *parent = nullptr);

    void reload();
    QList<Skill> skills() const { return m_skills; }
    const Skill *find(const QString &name) const;

    // Plugins contribute their own skill folders; call before reload().
    void setPluginSkillRoots(const QList<QPair<QString, QString>> &pluginIdAndRoot);

    void setEnabled(const QString &name, bool enabled);
    bool save(const QString &originalName, const QString &name, const QString &description, const QString &body,
              QString *error);
    bool remove(const QString &name, QString *error);
    // replace: overwrite an installed skill of the same name (updates from GitHub).
    bool importFolder(const QString &folder, QString *error, bool replace = false, QString *installedName = nullptr);

    QStringList enabledSkillDirs() const;

    static bool parse(const QString &skillMdPath, Skill *out);
    static QString sanitizeName(const QString &name);

Q_SIGNALS:
    void changed();

private:
    void scanRoot(const QString &root, const QString &origin);

    QList<Skill> m_skills;
    QList<QPair<QString, QString>> m_pluginRoots;
};

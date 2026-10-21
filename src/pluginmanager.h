#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QStringList>

// Toolbar button contributed by a plugin. Runs a shell command in the project
// folder and either drops the output into the composer or sends it directly.
struct PluginAction
{
    QString pluginId;
    QString label;
    QString run;
    QString mode; // "insert" (default) or "send"
};

// An Orbital plugin is a folder with a plugin.json manifest. It can contribute:
//   commands      -> opencode slash commands (prompt templates)
//   mcp           -> MCP servers (tools for the model)
//   skills        -> a folder of skills bundled with the plugin
//   instructions  -> markdown files appended to the system instructions
//   tools         -> script tools (native engine): executable gets JSON args on stdin
//   opencodePlugins -> opencode server plugins (JS/TS, opencode engine)
//   actions       -> Orbital toolbar buttons
//   hooks.beforePrompt -> executable that can rewrite or block outgoing prompts
struct Plugin
{
    QString id;
    QString name;
    QString version;
    QString description;
    QString author;
    QString dir;
    QJsonObject manifest;
    bool enabled = true;
    QString error;

    QString skillsRoot() const;
    QStringList summary() const;
};

class PluginManager : public QObject
{
    Q_OBJECT
public:
    explicit PluginManager(QObject *parent = nullptr);

    void reload();
    QList<Plugin> plugins() const { return m_plugins; }
    const Plugin *find(const QString &id) const;

    void setEnabled(const QString &id, bool enabled);
    bool install(const QString &folder, QString *error);
    bool create(const QString &id, const QString &name, QString *error);
    bool remove(const QString &id, QString *error);

    QList<QPair<QString, QString>> skillRoots() const;
    QList<PluginAction> actions() const;

    // Contributions of every enabled plugin, merged.
    QJsonObject commands() const;     // name -> {template, description, agent?}
    QJsonObject mcpServers() const;   // name -> {type, command[] | url, ...}
    QList<QPair<QString, QJsonObject>> scriptTools() const; // (plugin dir, tool def)
    QStringList instructionFiles() const;
    QStringList opencodePlugins() const;

    // Runs all beforePrompt hooks in order. Returns false (with reason) if a hook blocks.
    bool runBeforePrompt(QString *text, const QString &sessionId, const QString &directory, QString *reason) const;

Q_SIGNALS:
    void changed();

private:
    QList<Plugin> m_plugins;
};

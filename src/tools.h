#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <functional>

class McpClient;
class PluginManager;
class QNetworkAccessManager;
class QProcess;
class SkillManager;

using ToolDone = std::function<void(const QString &output, bool isError)>;

// Every tool the native engine can call: built-ins (files, shell, search,
// skills), plugin script tools and tools exposed by MCP servers.
class ToolRegistry : public QObject
{
    Q_OBJECT
public:
    ToolRegistry(SkillManager *skills, PluginManager *plugins, QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Restarts MCP servers and re-reads plugin script tools.
    void reloadExtensions();

    QJsonArray definitions(bool readOnlyAgent) const;
    bool isReadOnly(const QString &tool) const;
    bool exists(const QString &tool) const;
    void execute(const QString &tool, const QJsonObject &args, const QString &directory, ToolDone done);
    void abortRunning();

    static QString describe(const QString &tool, const QJsonObject &args);
    QStringList mcpStatus() const;

Q_SIGNALS:
    void extensionsChanged();

private:
    struct ScriptTool
    {
        QString pluginDir;
        QJsonObject def;
    };

    void runProcess(const QString &program, const QStringList &args, const QString &cwd, const QByteArray &stdinData,
                    int timeoutMs, ToolDone done, const QHash<QString, QString> &env = {});

    SkillManager *m_skills;
    PluginManager *m_plugins;
    QNetworkAccessManager *m_nam;
    QList<McpClient *> m_mcp;
    QList<ScriptTool> m_scripts;
    QList<QPointer<QProcess>> m_running;
};

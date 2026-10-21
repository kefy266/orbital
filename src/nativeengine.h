#pragma once

#include "engine.h"

#include <QHash>
#include <QPointer>
#include <QSet>

class LlmStream;
class PluginManager;
class QNetworkAccessManager;
class SkillManager;
class ToolRegistry;

// Orbital's own agent: streams from the model API, executes tool calls
// locally (asking permission where configured) and loops until done.
// Sessions are JSON files under ~/.local/share/orbital/sessions.
class NativeEngine : public Engine
{
    Q_OBJECT
public:
    NativeEngine(SkillManager *skills, PluginManager *plugins, QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("native"); }
    QString displayName() const override { return tr("Orbital Core"); }

    void start(const QString &directory) override;
    void stop() override;
    QString directory() const override { return m_directory; }

    void listSessions(std::function<void(const QList<SessionInfo> &)> done) override;
    void createSession(std::function<void(const QString &, const QString &)> done) override;
    void deleteSession(const QString &id, Done done) override;
    void loadHistory(const QString &id, std::function<void(const QJsonArray &)> done) override;

    void listModels(std::function<void(const QList<ModelChoice> &)> done) override;
    QList<AgentInfo> agents() const override;
    void listCommands(std::function<void(const QList<CommandDef> &)> done) override;

    void send(const QString &sessionId, const QString &text, const QStringList &skills, const QString &modelRef,
              const QString &agent) override;
    void runCommand(const QString &sessionId, const QString &command, const QString &arguments, const QString &modelRef,
                    const QString &agent) override;
    void abort(const QString &sessionId) override;
    void replyPermission(const PermissionRequest &request, const QString &reply) override;

    ToolRegistry *tools() const { return m_tools; }

private:
    struct Run
    {
        quint64 serial = 0;
        QString sessionId;
        QString modelRef;
        QString agent;
        QJsonObject session;
        QPointer<LlmStream> stream;
        int step = 0;
        QJsonArray pendingCalls;
        int callIndex = 0;
        QString permissionId;
        bool aborted = false;
    };

    QString sessionPath(const QString &id) const;
    QJsonObject loadSession(const QString &id) const;
    void saveSession(QJsonObject session) const;
    QString systemPrompt(const QString &agent) const;

    void step(Run *run);
    void runNextTool(Run *run);
    void executeTool(Run *run, const QJsonObject &call);
    void finishRun(Run *run, const QString &error = {});
    bool recoverTextToolCall(QJsonObject *assistant) const;

    SkillManager *m_skills;
    PluginManager *m_plugins;
    QNetworkAccessManager *m_nam;
    ToolRegistry *m_tools;
    QString m_directory;
    QHash<QString, Run *> m_runs;
    quint64 m_serial = 0;
    Run *liveRun(const QString &sessionId, quint64 serial) const;
    QHash<QString, QSet<QString>> m_alwaysAllowed; // session -> tool names
};

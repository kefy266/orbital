#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

struct ModelChoice
{
    QString provider;
    QString providerName;
    QString model;
    QString label;
    QString name; // display name from the provider, when it has one
    QString ref() const { return provider + QLatin1Char('/') + model; }
};

struct SessionInfo
{
    QString id;
    QString title;
    qint64 updated = 0;
};

struct AgentInfo
{
    QString id;
    QString description;
};

struct CommandDef
{
    QString name;
    QString description;
    QString source; // "engine", "plugin" or "skill"
};

struct PermissionRequest
{
    QString sessionId;
    QString id;
    QString title;   // e.g. "bash"
    QString detail;  // e.g. the command or file path
};

// What Orbital's UI talks to. Two implementations:
//   NativeEngine   — Orbital's own agent loop, talks to model APIs directly
//   OpencodeEngine — an Orbital-managed, isolated opencode server
// Both stream the same signals so the UI does not care which one is active.
//
// History items handed to loadHistory's callback use this neutral shape:
//   {role:"user", text}
//   {role:"assistant", parts:[{type:"text"|"reasoning", text} |
//                             {type:"tool", id, name, input, status, output}], error?}
class Engine : public QObject
{
    Q_OBJECT
public:
    using Done = std::function<void(const QString &error)>;

    using QObject::QObject;

    virtual QString id() const = 0;
    virtual QString displayName() const = 0;

    virtual void start(const QString &directory) = 0; // emits ready() or failed()
    virtual void stop() = 0;
    virtual QString directory() const = 0;

    virtual void listSessions(std::function<void(const QList<SessionInfo> &)> done) = 0;
    virtual void createSession(std::function<void(const QString &id, const QString &error)> done) = 0;
    virtual void deleteSession(const QString &id, Done done) = 0;
    virtual void loadHistory(const QString &id, std::function<void(const QJsonArray &items)> done) = 0;

    virtual void listModels(std::function<void(const QList<ModelChoice> &)> done) = 0;
    virtual QList<AgentInfo> agents() const = 0;
    virtual void listCommands(std::function<void(const QList<CommandDef> &)> done) = 0;

    // skills: names of skills the user attached to this message.
    virtual void send(const QString &sessionId, const QString &text, const QStringList &skills, const QString &modelRef,
                      const QString &agent) = 0;
    virtual void runCommand(const QString &sessionId, const QString &command, const QString &arguments,
                            const QString &modelRef, const QString &agent) = 0;
    virtual void abort(const QString &sessionId) = 0;
    virtual void replyPermission(const PermissionRequest &request, const QString &reply) = 0; // once|always|reject

Q_SIGNALS:
    void ready();
    void failed(const QString &reason);
    void statusMessage(const QString &text);

    void textDelta(const QString &sessionId, const QString &key, const QString &delta);
    void textSet(const QString &sessionId, const QString &key, const QString &text);
    void reasoningDelta(const QString &sessionId, const QString &key, const QString &delta);
    void toolUpdate(const QString &sessionId, const QString &callId, const QString &name, const QJsonObject &input,
                    const QString &status, const QString &output);
    void busyChanged(const QString &sessionId, bool busy);
    void errorOccurred(const QString &sessionId, const QString &message);
    void permissionRequested(const PermissionRequest &request);
    void usageUpdated(const QString &sessionId, int inputTokens, int outputTokens, double cost);
    void sessionsChanged();
};

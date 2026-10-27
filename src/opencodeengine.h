#pragma once

#include "engine.h"

#include <QHash>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSet>

class PluginManager;
class QNetworkReply;
class QProcess;
class SkillManager;

// Runs an Orbital-owned copy of opencode (downloaded from the npm registry into
// ~/.local/share/orbital/engine) with its own XDG home, so it never touches the
// user's own opencode install or ~/.config/opencode. Orbital's providers,
// skills and plugin contributions are passed in via OPENCODE_CONFIG_CONTENT.
class OpencodeEngine : public Engine
{
    Q_OBJECT
public:
    static constexpr const char *kDefaultVersion = "1.19.3";

    OpencodeEngine(SkillManager *skills, PluginManager *plugins, QObject *parent = nullptr);
    ~OpencodeEngine() override;

    QString id() const override { return QStringLiteral("opencode"); }
    QString displayName() const override { return tr("Orbital Cloud"); }

    void start(const QString &directory) override;
    void stop() override;
    QString directory() const override { return m_directory; }

    void listSessions(std::function<void(const QList<SessionInfo> &)> done) override;
    void createSession(std::function<void(const QString &, const QString &)> done) override;
    void deleteSession(const QString &id, Done done) override;
    void renameSession(const QString &id, const QString &title, Done done) override;
    void loadHistory(const QString &id, std::function<void(const QJsonArray &)> done) override;

    void listModels(std::function<void(const QList<ModelChoice> &)> done) override;
    QList<AgentInfo> agents() const override { return m_agents; }
    void listCommands(std::function<void(const QList<CommandDef> &)> done) override;

    void send(const QString &sessionId, const QString &text, const QStringList &skills, const QString &modelRef,
              const QString &agent) override;
    void runCommand(const QString &sessionId, const QString &command, const QString &arguments, const QString &modelRef,
                    const QString &agent) override;
    void abort(const QString &sessionId) override;
    void replyPermission(const PermissionRequest &request, const QString &reply) override;
    void replyQuestion(const QString &requestId, const QJsonArray &answers) override;
    void rejectQuestion(const QString &requestId) override;

    static QString version();
    static QString binaryPath();
    static QString homeDir();

private:
    using Callback = std::function<void(const QJsonValue &data, const QString &error)>;

    void install();
    void listProjectSessions(std::function<void(const QList<SessionInfo> &)> done);
    void launch();
    void buildOverlay(std::function<void(const QByteArray &)> done);
    void onServerReady();

    QNetworkRequest request(const QString &path) const;
    void call(const QByteArray &verb, const QString &path, const QJsonValue &body, Callback cb);
    void startEvents();
    void parseEvents();
    void handleEvent(const QString &type, const QJsonObject &props);

    SkillManager *m_skills;
    PluginManager *m_plugins;
    QNetworkAccessManager m_nam;
    QPointer<QProcess> m_proc;
    QPointer<QNetworkReply> m_events;
    QByteArray m_eventBuffer;
    QByteArray m_stdout;
    QString m_directory;
    QString m_baseUrl;
    QByteArray m_auth;
    bool m_ready = false;
    bool m_stopping = false;
    QList<AgentInfo> m_agents;

    QSet<QString> m_userMessages;
    QSet<QString> m_reasoningParts;
    QHash<QString, QString> m_partSession;
};

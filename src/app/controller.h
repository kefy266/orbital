#pragma once

#include "../engine.h"

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "chatmodel.h"
class PluginManager;
class SkillManager;

// Everything the QML UI talks to. Owns both engines, the skill/plugin
// managers and the chat/session models; exposed as the "App" singleton.
class QNetworkAccessManager;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ChatModel *chat READ chat CONSTANT)
    Q_PROPERTY(SessionModel *sessions READ sessions CONSTANT)

    Q_PROPERTY(QString engineId READ engineId NOTIFY engineChanged)
    Q_PROPERTY(bool engineReady READ engineReady NOTIFY engineChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY statusChanged)

    Q_PROPERTY(QString projectName READ projectName NOTIFY projectChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectChanged)
    Q_PROPERTY(bool isNoFolder READ isNoFolder NOTIFY projectChanged)
    Q_PROPERTY(QVariantList projects READ projects NOTIFY projectsChanged)
    Q_PROPERTY(QString projectFilter READ projectFilter WRITE setProjectFilter NOTIFY projectFilterChanged)

    Q_PROPERTY(QVariantList models READ models NOTIFY modelsChanged)
    Q_PROPERTY(QString modelRef READ modelRef NOTIFY modelChanged)
    Q_PROPERTY(QString modelLabel READ modelLabel NOTIFY modelChanged)
    Q_PROPERTY(bool isCurrentModelLocked READ isCurrentModelLocked NOTIFY modelChanged)
    Q_PROPERTY(QVariantList agents READ agents NOTIFY agentsChanged)
    Q_PROPERTY(QString agent READ agent WRITE setAgent NOTIFY agentsChanged)

    Q_PROPERTY(QString sessionId READ sessionId NOTIFY sessionChanged)
    Q_PROPERTY(QString sessionTitle READ sessionTitle NOTIFY sessionChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool thinking READ thinking NOTIFY thinkingChanged)
    Q_PROPERTY(QString usage READ usage NOTIFY usageChanged)

    Q_PROPERTY(QStringList attachedSkills READ attachedSkills NOTIFY attachedSkillsChanged)
    Q_PROPERTY(QVariantList commands READ commands NOTIFY commandsChanged)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY actionsChanged)
    Q_PROPERTY(QVariantMap permission READ permission NOTIFY permissionChanged)
    Q_PROPERTY(QVariantMap question READ question NOTIFY questionChanged)
    Q_PROPERTY(QStringList promptHistory READ promptHistory NOTIFY promptHistoryChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(bool autoMode READ autoMode NOTIFY actionsChanged)
    Q_PROPERTY(QVariantList storeItems READ storeItems NOTIFY storeChanged)
    Q_PROPERTY(bool storeLoading READ storeLoading NOTIFY storeChanged)
    Q_PROPERTY(QString storeError READ storeError NOTIFY storeChanged)

    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateChanged)
    Q_PROPERTY(QString updateVersion READ updateVersion NOTIFY updateChanged)
    Q_PROPERTY(QString updateNotes READ updateNotes NOTIFY updateChanged)
    Q_PROPERTY(bool checkingUpdate READ checkingUpdate NOTIFY updateChanged)
    Q_PROPERTY(bool updating READ updating NOTIFY updateChanged)
    Q_PROPERTY(qreal updateProgress READ updateProgress NOTIFY updateProgressChanged)
    Q_PROPERTY(QString updateStatus READ updateStatus NOTIFY updateChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    void start();

    ChatModel *chat() const { return m_chat; }
    SessionModel *sessions() const { return m_sessions; }
    QString engineId() const;
    bool engineReady() const { return m_engineReady; }
    QString status() const { return m_status; }
    bool statusIsError() const { return m_statusIsError; }
    QString projectName() const;
    QString projectPath() const { return m_directory; }
    bool isNoFolder() const;
    QVariantList models() const { return m_models; }
    QString modelRef() const;
    QString modelLabel() const;
    QVariantList agents() const;
    QString agent() const { return m_agent; }
    void setAgent(const QString &agent);
    QString sessionId() const { return m_sessionId; }
    QString sessionTitle() const { return m_sessionTitle; }
    bool busy() const { return m_busy.value(m_sessionId); }
    bool thinking() const { return m_thinking; }
    QString usage() const { return m_usage; }
    QStringList attachedSkills() const { return m_attached; }
    QVariantList commands() const { return m_commands; }
    QVariantList actions() const;
    QVariantMap permission() const { return m_permission; }
    QVariantMap question() const { return m_question; }
    QString themeMode() const;
    void setThemeMode(const QString &mode);

    // Chat
    Q_INVOKABLE void submit(const QString &text);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void newChat();
    Q_INVOKABLE void openSession(const QString &id);
    Q_INVOKABLE void deleteSession(const QString &id);
    Q_INVOKABLE void setProject(const QUrl &folder);
    Q_INVOKABLE void openProjectPath(const QString &path);
    Q_INVOKABLE void openNoFolderProject();
    Q_INVOKABLE void removeRecentProject(const QString &path);
    QVariantList projects() const;
    QString projectFilter() const { return m_projectFilter; }
    void setProjectFilter(const QString &filter);
    Q_INVOKABLE void switchEngine(const QString &id);
    Q_INVOKABLE void setModel(const QString &ref);
    Q_INVOKABLE void attachSkill(const QString &name);
    Q_INVOKABLE void detachSkill(const QString &name);
    Q_INVOKABLE void replyPermission(const QString &reply);
    Q_INVOKABLE void replyQuestion(const QVariantList &answers);
    Q_INVOKABLE void rejectQuestion();
    Q_INVOKABLE void retryLastTurn();
    Q_INVOKABLE void renameSession(const QString &id, const QString &newTitle);
    Q_INVOKABLE void exportChatAsMarkdown();
    Q_INVOKABLE QString formatDroppedUrl(const QUrl &url) const;
    QStringList promptHistory() const { return m_promptHistory; }
    Q_INVOKABLE void clearPromptHistory();
    Q_INVOKABLE void runAction(int index);
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE void openPath(const QString &path);
    bool autoMode() const;
    // Turns auto mode off (disables the plugin that grants it) and restarts the engine.
    Q_INVOKABLE void disableAutoMode();
    Q_INVOKABLE void toggleAutoMode();
    bool isCurrentModelLocked() const { return m_lockedModels.contains(m_modelRef); }
    Q_INVOKABLE void lockModel(const QString &ref, const QString &reason = QString());
    Q_INVOKABLE void unlockModel(const QString &ref);
    Q_INVOKABLE void unlockAllModels();
    Q_INVOKABLE bool isModelLocked(const QString &ref) const { return m_lockedModels.contains(ref); }
    // Opens web/e-mail links from the chat in the default browser.
    Q_INVOKABLE void openLink(const QString &url);
    // Plain user text -> rich text with clickable links.
    Q_INVOKABLE QString linkifyPlain(const QString &text) const;
    Q_INVOKABLE void retryEngine();

    // Settings: skills
    Q_INVOKABLE QVariantList skillList() const;
    Q_INVOKABLE QString saveSkill(const QString &original, const QString &name, const QString &description,
                                  const QString &body);
    Q_INVOKABLE QString deleteSkill(const QString &name);
    Q_INVOKABLE void setSkillEnabled(const QString &name, bool enabled);
    Q_INVOKABLE QString importSkill(const QUrl &folder);
    // Installs every skill (folder with a SKILL.md) found at a GitHub repo/folder URL;
    // answers with githubSkillDone.
    Q_INVOKABLE void installSkillFromGitHub(const QString &url);
    // Settings: plugins
    Q_INVOKABLE QVariantList pluginList() const;
    Q_INVOKABLE void setPluginEnabled(const QString &id, bool enabled);
    Q_INVOKABLE QString removePlugin(const QString &id);
    Q_INVOKABLE QString createPlugin(const QString &id);
    Q_INVOKABLE QString installPlugin(const QUrl &folder);
    // Settings: providers, permissions, engine
    Q_INVOKABLE QVariantList providerCatalog() const;
    // Dev aid for screenshots: ORBITAL_DEV_SETTINGS=<page>[:<provider>:<key>]
    Q_INVOKABLE QString devHook() const { return qEnvironmentVariable("ORBITAL_DEV_SETTINGS"); }
    Q_INVOKABLE void connectProvider(const QString &id, const QString &apiKey);
    Q_INVOKABLE void addCustomProvider(const QString &name, const QString &baseUrl, const QString &apiKey);
    Q_INVOKABLE void disconnectProvider(const QString &id);
    Q_INVOKABLE QVariantList providerList() const;
    Q_INVOKABLE void saveProvider(const QString &id, const QVariantMap &fields);
    Q_INVOKABLE QString addProvider(const QString &name);
    Q_INVOKABLE void removeProvider(const QString &id);
    Q_INVOKABLE QVariantMap permissionMap() const;
    Q_INVOKABLE void setPermission(const QString &tool, const QString &policy);
    Q_INVOKABLE int maxSteps() const;
    Q_INVOKABLE void setMaxSteps(int steps);
    Q_INVOKABLE QString opencodeVersion() const;
    Q_INVOKABLE void setOpencodeVersion(const QString &version);
    Q_INVOKABLE QVariantMap paths() const;
    // Store: catalog of skills and plugins hosted in the Orbital repository.
    QVariantList storeItems() const;
    bool storeLoading() const { return m_storeLoading; }
    QString storeError() const { return m_storeError; }
    Q_INVOKABLE void loadStore();
    Q_INVOKABLE void syncOfficialSkills();
    Q_INVOKABLE void installStoreItem(const QString &id);
    Q_INVOKABLE void removeStoreItem(const QString &id);

    // Auto-update
    QString appVersion() const;
    bool updateAvailable() const { return m_updateAvailable; }
    QString updateVersion() const { return m_updateVersion; }
    QString updateNotes() const { return m_updateNotes; }
    bool checkingUpdate() const { return m_checkingUpdate; }
    bool updating() const { return m_updating; }
    qreal updateProgress() const { return m_updateProgress; }
    QString updateStatus() const { return m_updateStatus; }
    Q_INVOKABLE void checkForUpdates(bool silent = false);
    Q_INVOKABLE void applyUpdate();

    // Restarts the engine if settings that it reads at startup changed.
    Q_INVOKABLE void applySettings();

Q_SIGNALS:
    void engineChanged();
    void statusChanged();
    void projectChanged();
    void projectsChanged();
    void projectFilterChanged();
    void modelsChanged();
    void modelChanged();
    void agentsChanged();
    void sessionChanged();
    void busyChanged();
    void thinkingChanged();
    void usageChanged();
    void attachedSkillsChanged();
    void commandsChanged();
    void actionsChanged();
    void permissionChanged();
    void questionChanged();
    void promptHistoryChanged();
    void themeModeChanged();
    void toast(const QString &message);
    void providerResult(const QString &id, bool ok, const QString &message);
    void storeChanged();
    void storeItemDone(const QString &id, bool ok, const QString &message);
    void githubSkillDone(bool ok, const QString &message);
    void updateChanged();
    void updateProgressChanged();
    void scrollToEnd();

private:
    void reloadExtensions();
    void connectEngine();
    void onEngineReady();
    void refreshSessions();
    void loadSessionHistory(const QString &id);
    void refreshModels();
    void refreshCommands();
    void dispatch(const QString &text, const QStringList &skills);
    void setStatus(const QString &text, bool error = false);
    void setThinking(bool thinking);
    void setBusyFor(const QString &sid, bool busy);
    bool isCurrent(const QString &sid) const { return !sid.isEmpty() && sid == m_sessionId; }

    SkillManager *m_skills;
    PluginManager *m_plugins;
    ChatModel *m_chat;
    SessionModel *m_sessions;
    QHash<QString, Engine *> m_engines;
    Engine *m_engine = nullptr;
    bool m_engineReady = false;
    bool m_settingsDirty = false;

    QString m_directory;
    QString m_projectFilter = QStringLiteral("all");
    QString m_status;
    bool m_statusIsError = false;
    QVariantList m_models;
    QString m_modelRef;
    QString m_agent = QStringLiteral("build");
    QString m_sessionId;
    QString m_sessionTitle;
    QString m_queued;
    QString m_pendingHistory; // session to load once the restarted engine is ready
    QHash<QString, bool> m_busy;
    bool m_thinking = false;
    QString m_usage;
    QStringList m_attached;
    QVariantList m_commands;
    QVariantMap m_permission;
    QList<PermissionRequest> m_permissionQueue;
    QVariantMap m_question;
    QList<QuestionRequest> m_questionQueue;
    QTimer m_sessionRefresh;
    QNetworkAccessManager *m_nam;
    QVariantList m_store;
    bool m_storeLoading = false;
    QString m_storeError;
    QStringList m_storeBusy;
    QString storeUrl() const;
    void githubGet(const QUrl &url, std::function<void(const QByteArray &, const QString &)> done);
    void fetchStoreFile(const QString &path, std::function<void(const QByteArray &, const QString &)> done);
    QMetaObject::Connection m_mcpConnection;

    QStringList m_promptHistory;
    void loadPromptHistory();
    void savePromptHistory();

    QSet<QString> m_lockedModels;
    void loadLockedModels();
    void saveLockedModels();
    bool handleSlashCommand(const QString &name, const QString &args);

    bool m_updateAvailable = false;
    QString m_updateVersion;
    QString m_updateNotes;
    QString m_updateDownloadUrl;
    qint64 m_updateAssetSize = 0;
    bool m_checkingUpdate = false;
    bool m_updating = false;
    qreal m_updateProgress = 0.0;
    QString m_updateStatus;
};

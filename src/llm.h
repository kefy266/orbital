#pragma once

#include "config.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>

class QNetworkReply;

// Orbital's provider-neutral conversation format:
//   {role:"user", content}
//   {role:"assistant", content, reasoning?, tool_calls?:[{id, name, arguments:{...}}]}
//   {role:"tool", tool_call_id, name, content, is_error?}
// Tools are {name, description, parameters:<JSON schema>}.
struct LlmRequest
{
    ProviderConfig provider;
    QString model;
    QString system;
    QJsonArray messages;
    QJsonArray tools;
};

// One streamed model turn. Emits deltas while streaming and the complete
// assistant message (in Orbital format) at the end.
class LlmStream : public QObject
{
    Q_OBJECT
public:
    explicit LlmStream(QNetworkAccessManager *nam, QObject *parent = nullptr);

    void start(const LlmRequest &request);
    void abort();

Q_SIGNALS:
    void textDelta(const QString &delta);
    void reasoningDelta(const QString &delta);
    void toolCallStarted(const QString &id, const QString &name);
    void finished(const QJsonObject &assistantMessage, const QJsonObject &usage);
    void failed(const QString &error);

private:
    void startOpenAi(const LlmRequest &r);
    void startAnthropic(const LlmRequest &r);
    void onData();
    void onFinished();
    void handleOpenAiEvent(const QJsonObject &e);
    void handleAnthropicEvent(const QJsonObject &e);
    QJsonObject assemble() const;

    QNetworkAccessManager *m_nam;
    QPointer<QNetworkReply> m_reply;
    QString m_type;
    QByteArray m_buffer;
    QByteArray m_errorBody;
    bool m_done = false;

    QString m_text;
    QString m_reasoning;
    struct PendingCall { QString id; QString name; QString args; QJsonObject extraContent; };
    QList<PendingCall> m_calls;
    QHash<int, int> m_callIndex; // provider index -> m_calls position
    QJsonObject m_usage;
};

// Extracts model ids from an OpenAI-style /models response, keeping ids matching filter.
QStringList parseModelIds(const QByteArray &json, const QString &filter);

// Checks credentials by listing the provider's models.
void testProvider(QNetworkAccessManager *nam, const ProviderConfig &provider,
                  std::function<void(bool ok, const QString &message, const QStringList &models)> done);

// Lists models a provider advertises (GET /models), merged with configured ones.
void fetchModels(QNetworkAccessManager *nam, const ProviderConfig &provider,
                 std::function<void(const QStringList &models)> done);

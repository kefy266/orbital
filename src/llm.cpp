#include "llm.h"

#include <QJsonDocument>
#include <QDateTime>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

static QByteArray json(const QJsonObject &o)
{
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

static QJsonObject parseArgs(const QString &raw)
{
    if (raw.trimmed().isEmpty())
        return {};
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return QJsonObject{{QStringLiteral("_raw"), raw}};
    return doc.object();
}

LlmStream::LlmStream(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{
}

void LlmStream::start(const LlmRequest &request)
{
    m_type = request.provider.type;
    if (m_type == QLatin1String("anthropic"))
        startAnthropic(request);
    else
        startOpenAi(request);

    connect(m_reply, &QNetworkReply::readyRead, this, &LlmStream::onData);
    connect(m_reply, &QNetworkReply::finished, this, &LlmStream::onFinished);
}

void LlmStream::abort()
{
    m_done = true;
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
}

// ---------------------------------------------------------------- OpenAI-compatible

void LlmStream::startOpenAi(const LlmRequest &r)
{
    QJsonArray messages;
    if (!r.system.isEmpty())
        messages << QJsonObject{{QStringLiteral("role"), QStringLiteral("system")}, {QStringLiteral("content"), r.system}};
    for (const QJsonValue &v : r.messages) {
        const QJsonObject m = v.toObject();
        const QString role = m.value(QStringLiteral("role")).toString();
        if (role == QLatin1String("assistant")) {
            QJsonObject out{{QStringLiteral("role"), role},
                            {QStringLiteral("content"), m.value(QStringLiteral("content")).toString()}};
            QJsonArray calls;
            for (const QJsonValue &c : m.value(QStringLiteral("tool_calls")).toArray()) {
                const QJsonObject call = c.toObject();
                calls << QJsonObject{
                    {QStringLiteral("id"), call.value(QStringLiteral("id"))},
                    {QStringLiteral("type"), QStringLiteral("function")},
                    {QStringLiteral("function"), QJsonObject{
                         {QStringLiteral("name"), call.value(QStringLiteral("name"))},
                         {QStringLiteral("arguments"), QString::fromUtf8(json(call.value(QStringLiteral("arguments")).toObject()))}}}};
            }
            if (!calls.isEmpty())
                out.insert(QStringLiteral("tool_calls"), calls);
            messages << out;
        } else if (role == QLatin1String("tool")) {
            messages << QJsonObject{{QStringLiteral("role"), role},
                                    {QStringLiteral("tool_call_id"), m.value(QStringLiteral("tool_call_id"))},
                                    {QStringLiteral("content"), m.value(QStringLiteral("content")).toString()}};
        } else {
            messages << QJsonObject{{QStringLiteral("role"), role}, {QStringLiteral("content"), m.value(QStringLiteral("content"))}};
        }
    }

    QJsonArray tools;
    for (const QJsonValue &v : r.tools) {
        const QJsonObject t = v.toObject();
        tools << QJsonObject{{QStringLiteral("type"), QStringLiteral("function")},
                             {QStringLiteral("function"), QJsonObject{
                                  {QStringLiteral("name"), t.value(QStringLiteral("name"))},
                                  {QStringLiteral("description"), t.value(QStringLiteral("description"))},
                                  {QStringLiteral("parameters"), t.value(QStringLiteral("parameters"))}}}};
    }

    QJsonObject body{{QStringLiteral("model"), r.model},
                     {QStringLiteral("messages"), messages},
                     {QStringLiteral("stream"), true},
                     {QStringLiteral("stream_options"), QJsonObject{{QStringLiteral("include_usage"), true}}}};
    if (!tools.isEmpty())
        body.insert(QStringLiteral("tools"), tools);

    QNetworkRequest req(QUrl(r.provider.baseUrl + QStringLiteral("/chat/completions")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "text/event-stream");
    const QString key = r.provider.resolvedApiKey();
    if (!key.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + key.toUtf8());
    req.setRawHeader("X-Title", "Orbital");
    req.setTransferTimeout(0);
    m_reply = m_nam->post(req, json(body));
}

void LlmStream::handleOpenAiEvent(const QJsonObject &e)
{
    if (e.contains(QStringLiteral("usage")) && e.value(QStringLiteral("usage")).isObject()) {
        const QJsonObject u = e.value(QStringLiteral("usage")).toObject();
        m_usage = QJsonObject{{QStringLiteral("input"), u.value(QStringLiteral("prompt_tokens")).toInt()},
                              {QStringLiteral("output"), u.value(QStringLiteral("completion_tokens")).toInt()}};
    }
    if (e.contains(QStringLiteral("error"))) {
        const QJsonValue err = e.value(QStringLiteral("error"));
        Q_EMIT failed(err.isObject() ? err.toObject().value(QStringLiteral("message")).toString() : err.toString());
        abort();
        return;
    }
    for (const QJsonValue &cv : e.value(QStringLiteral("choices")).toArray()) {
        const QJsonObject delta = cv.toObject().value(QStringLiteral("delta")).toObject();
        const QString text = delta.value(QStringLiteral("content")).toString();
        if (!text.isEmpty()) {
            m_text += text;
            Q_EMIT textDelta(text);
        }
        QString reasoning = delta.value(QStringLiteral("reasoning_content")).toString();
        if (reasoning.isEmpty())
            reasoning = delta.value(QStringLiteral("reasoning")).toString();
        if (!reasoning.isEmpty()) {
            m_reasoning += reasoning;
            Q_EMIT reasoningDelta(reasoning);
        }
        for (const QJsonValue &tv : delta.value(QStringLiteral("tool_calls")).toArray()) {
            const QJsonObject tc = tv.toObject();
            const int index = tc.value(QStringLiteral("index")).toInt(m_calls.size());
            if (!m_callIndex.contains(index)) {
                m_callIndex.insert(index, m_calls.size());
                m_calls << PendingCall{};
            }
            PendingCall &call = m_calls[m_callIndex.value(index)];
            const QString id = tc.value(QStringLiteral("id")).toString();
            if (!id.isEmpty())
                call.id = id;
            const QJsonObject fn = tc.value(QStringLiteral("function")).toObject();
            const QString name = fn.value(QStringLiteral("name")).toString();
            if (!name.isEmpty() && call.name.isEmpty()) {
                call.name = name;
                if (call.id.isEmpty())
                    call.id = QStringLiteral("call_%1_%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(index);
                Q_EMIT toolCallStarted(call.id, call.name);
            }
            call.args += fn.value(QStringLiteral("arguments")).toString();
        }
    }
}

// ---------------------------------------------------------------- Anthropic Messages API

void LlmStream::startAnthropic(const LlmRequest &r)
{
    QJsonArray messages;
    const auto push = [&messages](const QString &role, const QJsonArray &blocks) {
        // The Messages API wants alternating roles; merge consecutive same-role turns.
        if (!messages.isEmpty() && messages.last().toObject().value(QStringLiteral("role")).toString() == role) {
            QJsonObject last = messages.last().toObject();
            QJsonArray content = last.value(QStringLiteral("content")).toArray();
            for (const QJsonValue &b : blocks)
                content << b;
            last.insert(QStringLiteral("content"), content);
            messages[messages.size() - 1] = last;
        } else {
            messages << QJsonObject{{QStringLiteral("role"), role}, {QStringLiteral("content"), blocks}};
        }
    };
    for (const QJsonValue &v : r.messages) {
        const QJsonObject m = v.toObject();
        const QString role = m.value(QStringLiteral("role")).toString();
        if (role == QLatin1String("user")) {
            push(role, {QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                                    {QStringLiteral("text"), m.value(QStringLiteral("content")).toString()}}});
        } else if (role == QLatin1String("assistant")) {
            QJsonArray blocks;
            const QString text = m.value(QStringLiteral("content")).toString();
            if (!text.isEmpty())
                blocks << QJsonObject{{QStringLiteral("type"), QStringLiteral("text")}, {QStringLiteral("text"), text}};
            for (const QJsonValue &c : m.value(QStringLiteral("tool_calls")).toArray()) {
                const QJsonObject call = c.toObject();
                blocks << QJsonObject{{QStringLiteral("type"), QStringLiteral("tool_use")},
                                      {QStringLiteral("id"), call.value(QStringLiteral("id"))},
                                      {QStringLiteral("name"), call.value(QStringLiteral("name"))},
                                      {QStringLiteral("input"), call.value(QStringLiteral("arguments")).toObject()}};
            }
            if (!blocks.isEmpty())
                push(role, blocks);
        } else if (role == QLatin1String("tool")) {
            push(QStringLiteral("user"), {QJsonObject{{QStringLiteral("type"), QStringLiteral("tool_result")},
                                                      {QStringLiteral("tool_use_id"), m.value(QStringLiteral("tool_call_id"))},
                                                      {QStringLiteral("content"), m.value(QStringLiteral("content")).toString()},
                                                      {QStringLiteral("is_error"), m.value(QStringLiteral("is_error")).toBool()}}});
        }
    }

    QJsonArray tools;
    for (const QJsonValue &v : r.tools) {
        const QJsonObject t = v.toObject();
        tools << QJsonObject{{QStringLiteral("name"), t.value(QStringLiteral("name"))},
                             {QStringLiteral("description"), t.value(QStringLiteral("description"))},
                             {QStringLiteral("input_schema"), t.value(QStringLiteral("parameters"))}};
    }

    QJsonObject body{{QStringLiteral("model"), r.model},
                     {QStringLiteral("max_tokens"), 16000},
                     {QStringLiteral("messages"), messages},
                     {QStringLiteral("stream"), true}};
    if (!r.system.isEmpty())
        body.insert(QStringLiteral("system"), r.system);
    if (!tools.isEmpty())
        body.insert(QStringLiteral("tools"), tools);

    QNetworkRequest req(QUrl(r.provider.baseUrl + QStringLiteral("/v1/messages")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("anthropic-version", "2023-06-01");
    req.setRawHeader("x-api-key", r.provider.resolvedApiKey().toUtf8());
    req.setTransferTimeout(0);
    m_reply = m_nam->post(req, json(body));
}

void LlmStream::handleAnthropicEvent(const QJsonObject &e)
{
    const QString type = e.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("message_start")) {
        const QJsonObject u = e.value(QStringLiteral("message")).toObject().value(QStringLiteral("usage")).toObject();
        m_usage.insert(QStringLiteral("input"), u.value(QStringLiteral("input_tokens")).toInt()
                                                    + u.value(QStringLiteral("cache_read_input_tokens")).toInt()
                                                    + u.value(QStringLiteral("cache_creation_input_tokens")).toInt());
    } else if (type == QLatin1String("content_block_start")) {
        const QJsonObject block = e.value(QStringLiteral("content_block")).toObject();
        if (block.value(QStringLiteral("type")).toString() == QLatin1String("tool_use")) {
            const int index = e.value(QStringLiteral("index")).toInt();
            m_callIndex.insert(index, m_calls.size());
            m_calls << PendingCall{block.value(QStringLiteral("id")).toString(), block.value(QStringLiteral("name")).toString(), {}};
            Q_EMIT toolCallStarted(m_calls.last().id, m_calls.last().name);
        }
    } else if (type == QLatin1String("content_block_delta")) {
        const QJsonObject d = e.value(QStringLiteral("delta")).toObject();
        const QString dt = d.value(QStringLiteral("type")).toString();
        if (dt == QLatin1String("text_delta")) {
            const QString t = d.value(QStringLiteral("text")).toString();
            m_text += t;
            Q_EMIT textDelta(t);
        } else if (dt == QLatin1String("thinking_delta")) {
            const QString t = d.value(QStringLiteral("thinking")).toString();
            m_reasoning += t;
            Q_EMIT reasoningDelta(t);
        } else if (dt == QLatin1String("input_json_delta")) {
            const int index = e.value(QStringLiteral("index")).toInt();
            if (m_callIndex.contains(index))
                m_calls[m_callIndex.value(index)].args += d.value(QStringLiteral("partial_json")).toString();
        }
    } else if (type == QLatin1String("message_delta")) {
        m_usage.insert(QStringLiteral("output"), e.value(QStringLiteral("usage")).toObject().value(QStringLiteral("output_tokens")).toInt());
    } else if (type == QLatin1String("error")) {
        Q_EMIT failed(e.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString());
        abort();
    }
}

// ---------------------------------------------------------------- shared SSE plumbing

void LlmStream::onData()
{
    if (!m_reply)
        return;
    const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 400) {
        m_errorBody += m_reply->readAll();
        return;
    }
    m_buffer += m_reply->readAll();
    m_buffer.replace("\r\n", "\n");
    int sep;
    while ((sep = m_buffer.indexOf("\n\n")) >= 0) {
        const QByteArray chunk = m_buffer.left(sep);
        m_buffer.remove(0, sep + 2);
        for (const QByteArray &line : chunk.split('\n')) {
            if (!line.startsWith("data:"))
                continue;
            const QByteArray payload = line.mid(5).trimmed();
            if (payload == "[DONE]" || payload.isEmpty())
                continue;
            const QJsonObject e = QJsonDocument::fromJson(payload).object();
            if (m_type == QLatin1String("anthropic"))
                handleAnthropicEvent(e);
            else
                handleOpenAiEvent(e);
            if (m_done)
                return;
        }
    }
}

QJsonObject LlmStream::assemble() const
{
    QJsonObject msg{{QStringLiteral("role"), QStringLiteral("assistant")}, {QStringLiteral("content"), m_text}};
    if (!m_reasoning.isEmpty())
        msg.insert(QStringLiteral("reasoning"), m_reasoning);
    QJsonArray calls;
    for (const PendingCall &c : m_calls) {
        if (c.name.isEmpty())
            continue;
        calls << QJsonObject{{QStringLiteral("id"), c.id}, {QStringLiteral("name"), c.name},
                             {QStringLiteral("arguments"), parseArgs(c.args)}};
    }
    if (!calls.isEmpty())
        msg.insert(QStringLiteral("tool_calls"), calls);
    return msg;
}

void LlmStream::onFinished()
{
    if (m_done || !m_reply)
        return;
    onData();
    if (m_done)
        return;
    m_done = true;
    QNetworkReply *reply = m_reply;
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || status >= 400) {
        m_errorBody += reply->readAll();
        QString message = QJsonDocument::fromJson(m_errorBody).object().value(QStringLiteral("error")).toObject()
                              .value(QStringLiteral("message")).toString();
        if (message.isEmpty())
            message = QString::fromUtf8(m_errorBody).trimmed().left(400);
        if (message.isEmpty())
            message = reply->errorString();
        Q_EMIT failed(status ? QStringLiteral("HTTP %1: %2").arg(status).arg(message) : message);
        return;
    }
    Q_EMIT finished(assemble(), m_usage);
}

void fetchModels(QNetworkAccessManager *nam, const ProviderConfig &provider,
                 std::function<void(const QStringList &models)> done)
{
    if (provider.type != QLatin1String("openai") || !provider.fetchModels) {
        done(provider.models);
        return;
    }
    QNetworkRequest req(QUrl(provider.baseUrl + QStringLiteral("/models")));
    const QString key = provider.resolvedApiKey();
    if (!key.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + key.toUtf8());
    req.setTransferTimeout(4000);
    QNetworkReply *reply = nam->get(req);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, provider, done] {
        reply->deleteLater();
        QStringList models = provider.models;
        for (const QString &id : parseModelIds(reply->readAll(), provider.modelFilter))
            if (!models.contains(id))
                models << id;
        done(models);
    });
}

QStringList parseModelIds(const QByteArray &json, const QString &filter)
{
    const QRegularExpression rx(filter, QRegularExpression::CaseInsensitiveOption);
    QStringList out;
    for (const QJsonValue &v : QJsonDocument::fromJson(json).object().value(QStringLiteral("data")).toArray()) {
        QString id = v.toObject().value(QStringLiteral("id")).toString();
        if (id.startsWith(QStringLiteral("models/"))) // Gemini's OpenAI-compatible listing
            id = id.mid(7);
        if (id.isEmpty() || (!filter.isEmpty() && !rx.match(id).hasMatch()))
            continue;
        out << id;
    }
    out.sort();
    return out;
}

void testProvider(QNetworkAccessManager *nam, const ProviderConfig &provider,
                  std::function<void(bool ok, const QString &message, const QStringList &models)> done)
{
    const QString key = provider.resolvedApiKey();
    const bool anthropic = provider.type == QLatin1String("anthropic");
    QNetworkRequest req(QUrl(provider.baseUrl + (anthropic ? QStringLiteral("/v1/models") : QStringLiteral("/models"))));
    if (anthropic) {
        req.setRawHeader("x-api-key", key.toUtf8());
        req.setRawHeader("anthropic-version", "2023-06-01");
    } else if (!key.isEmpty()) {
        req.setRawHeader("Authorization", "Bearer " + key.toUtf8());
    }
    req.setTransferTimeout(15000);
    QNetworkReply *reply = nam->get(req);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, provider, done] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (status == 401 || status == 403) {
            done(false, QStringLiteral("API anahtarı geçersiz ya da yetkisiz."), {});
            return;
        }
        if (reply->error() != QNetworkReply::NoError && status == 0) {
            done(false, QStringLiteral("Sunucuya ulaşılamadı: %1").arg(reply->errorString()), {});
            return;
        }
        if (status >= 400) {
            QString message = QJsonDocument::fromJson(body).object().value(QStringLiteral("error")).toObject()
                                  .value(QStringLiteral("message")).toString();
            done(false, QStringLiteral("Sağlayıcı hata döndürdü (HTTP %1)%2").arg(status)
                            .arg(message.isEmpty() ? QString() : QStringLiteral(": ") + message.left(200)), {});
            return;
        }
        QStringList models = parseModelIds(body, provider.modelFilter);
        if (models.isEmpty())
            models = provider.models;
        done(true, QStringLiteral("%1 model bulundu").arg(models.size()), models);
    });
}

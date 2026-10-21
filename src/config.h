#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// A model endpoint. type "openai" covers every OpenAI-compatible server
// (Ollama, LM Studio, vLLM, OpenRouter...), type "anthropic" the Messages API.
struct ProviderConfig
{
    QString id;
    QString name;
    QString type;
    QString baseUrl;
    QString apiKey;
    QStringList models;
    bool fetchModels = true;
    QString modelFilter; // regex; keeps only matching model ids (e.g. chat models)

    QString resolvedApiKey() const;
};

// ~/.config/orbital/config.json — providers, default model, tool permissions.
namespace Config {
QString file();
QJsonObject raw();
bool saveRaw(const QJsonObject &config, QString *error);
void ensureDefault();

QList<ProviderConfig> providers();
bool provider(const QString &id, ProviderConfig *out);

QString defaultModel();            // "provider/model"
void setDefaultModel(const QString &ref);

// "allow", "ask" or "deny" for a tool name; defaults to ask for mutating tools.
QString permission(const QString &tool);
int maxSteps();
}

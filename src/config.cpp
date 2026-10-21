#include "config.h"

#include "paths.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcessEnvironment>

QString ProviderConfig::resolvedApiKey() const
{
    // "{env:NAME}" reads the key from the environment so it never sits in the file.
    if (apiKey.startsWith(QStringLiteral("{env:")) && apiKey.endsWith(QLatin1Char('}')))
        return QProcessEnvironment::systemEnvironment().value(apiKey.mid(5, apiKey.size() - 6));
    return apiKey;
}

namespace Config {

QString file()
{
    return Paths::configDir() + QStringLiteral("/config.json");
}

QJsonObject raw()
{
    QFile f(file());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

bool saveRaw(const QJsonObject &config, QString *error)
{
    QFile f(file());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    f.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
    return true;
}

void ensureDefault()
{
    if (QFile::exists(file()))
        return;
    const auto provider = [](const QString &name, const QString &type, const QString &url, const QString &key,
                             const QStringList &models) {
        return QJsonObject{{QStringLiteral("name"), name},
                           {QStringLiteral("type"), type},
                           {QStringLiteral("baseURL"), url},
                           {QStringLiteral("apiKey"), key},
                           {QStringLiteral("models"), QJsonArray::fromStringList(models)}};
    };
    const QJsonObject config{
        // Cloud only: no local model servers are configured by default.
        {QStringLiteral("model"), QString()},
        {QStringLiteral("maxSteps"), 40},
        {QStringLiteral("providers"), QJsonObject{
             {QStringLiteral("anthropic"), provider(QStringLiteral("Anthropic"), QStringLiteral("anthropic"),
                                                    QStringLiteral("https://api.anthropic.com"),
                                                    QStringLiteral("{env:ANTHROPIC_API_KEY}"),
                                                    {QStringLiteral("claude-sonnet-5-5"), QStringLiteral("claude-opus-5-5"),
                                                     QStringLiteral("claude-haiku-5-5")})},
             {QStringLiteral("openrouter"), provider(QStringLiteral("OpenRouter"), QStringLiteral("openai"),
                                                     QStringLiteral("https://openrouter.ai/api/v1"),
                                                     QStringLiteral("{env:OPENROUTER_API_KEY}"), {})},
         }},
        {QStringLiteral("permissions"), QJsonObject{
             {QStringLiteral("read"), QStringLiteral("allow")},
             {QStringLiteral("list"), QStringLiteral("allow")},
             {QStringLiteral("glob"), QStringLiteral("allow")},
             {QStringLiteral("grep"), QStringLiteral("allow")},
             {QStringLiteral("skill"), QStringLiteral("allow")},
             {QStringLiteral("write"), QStringLiteral("ask")},
             {QStringLiteral("edit"), QStringLiteral("ask")},
             {QStringLiteral("bash"), QStringLiteral("ask")},
             {QStringLiteral("*"), QStringLiteral("ask")},
         }},
    };
    saveRaw(config, nullptr);
}

QList<ProviderConfig> providers()
{
    QList<ProviderConfig> out;
    const QJsonObject all = raw().value(QStringLiteral("providers")).toObject();
    for (auto it = all.begin(); it != all.end(); ++it) {
        const QJsonObject o = it.value().toObject();
        ProviderConfig p;
        p.id = it.key();
        p.name = o.value(QStringLiteral("name")).toString(p.id);
        p.type = o.value(QStringLiteral("type")).toString(QStringLiteral("openai"));
        p.baseUrl = o.value(QStringLiteral("baseURL")).toString();
        while (p.baseUrl.endsWith(QLatin1Char('/')))
            p.baseUrl.chop(1);
        p.apiKey = o.value(QStringLiteral("apiKey")).toString();
        p.fetchModels = o.value(QStringLiteral("fetchModels")).toBool(true);
        p.modelFilter = o.value(QStringLiteral("modelFilter")).toString();
        for (const QJsonValue &m : o.value(QStringLiteral("models")).toArray())
            p.models << m.toString();
        // A provider whose key comes from an unset environment variable is not usable: hide it.
        if (p.apiKey.startsWith(QStringLiteral("{env:")) && p.resolvedApiKey().isEmpty())
            continue;
        out << p;
    }
    return out;
}

bool provider(const QString &id, ProviderConfig *out)
{
    for (const ProviderConfig &p : providers()) {
        if (p.id == id) {
            *out = p;
            return true;
        }
    }
    return false;
}

QString defaultModel()
{
    return raw().value(QStringLiteral("model")).toString();
}

void setDefaultModel(const QString &ref)
{
    QJsonObject config = raw();
    config.insert(QStringLiteral("model"), ref);
    saveRaw(config, nullptr);
}

QString permission(const QString &tool)
{
    const QJsonObject perms = raw().value(QStringLiteral("permissions")).toObject();
    if (perms.contains(tool))
        return perms.value(tool).toString();
    return perms.value(QStringLiteral("*")).toString(QStringLiteral("ask"));
}

int maxSteps()
{
    return raw().value(QStringLiteral("maxSteps")).toInt(40);
}

}

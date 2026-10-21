// Headless smoke test: drives an Orbital engine through one prompt and prints the event stream.
// Usage: enginetest <native|opencode> <projectDir> <provider/model> <prompt>
#include "../src/config.h"
#include "../src/nativeengine.h"
#include "../src/opencodeengine.h"
#include "../src/paths.h"
#include "../src/pluginmanager.h"
#include "../src/skillmanager.h"

#include <QCoreApplication>
#include <QTimer>
#include <cstdio>

static void out(const QString &s) { std::printf("%s\n", s.toUtf8().constData()); std::fflush(stdout); }

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList a = app.arguments();
    if (a.size() < 5) { out("usage: enginetest <native|opencode> <dir> <provider/model> <prompt> [skill]"); return 2; }
    Config::ensureDefault();
    Paths::installBundledContent();
    PluginManager plugins; plugins.reload();
    SkillManager skills; skills.setPluginSkillRoots(plugins.skillRoots()); skills.reload();
    out(QStringLiteral("skills: %1").arg(skills.skills().size()));

    Engine *e = a.at(1) == QLatin1String("opencode") ? static_cast<Engine *>(new OpencodeEngine(&skills, &plugins))
                                                     : static_cast<Engine *>(new NativeEngine(&skills, &plugins));
    QString session;
    QObject::connect(e, &Engine::statusMessage, [](const QString &s) { if (!s.isEmpty()) out("[status] " + s); });
    QObject::connect(e, &Engine::failed, [&](const QString &s) { out("[failed] " + s); app.exit(1); });
    QObject::connect(e, &Engine::textDelta, [](const QString &, const QString &, const QString &d) { std::printf("%s", d.toUtf8().constData()); std::fflush(stdout); });
    QObject::connect(e, &Engine::textSet, [](const QString &, const QString &k, const QString &t) { out(QStringLiteral("\n[text %1] %2").arg(k, t.left(300))); });
    QObject::connect(e, &Engine::toolUpdate, [](const QString &, const QString &id, const QString &name, const QJsonObject &in, const QString &st, const QString &o) {
        out(QStringLiteral("\n[tool %1 %2 %3] %4 => %5").arg(name, id.left(12), st, QString::fromUtf8(QJsonDocument(in).toJson(QJsonDocument::Compact)).left(200), o.left(300)));
    });
    QObject::connect(e, &Engine::permissionRequested, [e](const PermissionRequest &r) {
        out(QStringLiteral("\n[permission %1] %2 -> once").arg(r.title, r.detail));
        e->replyPermission(r, QStringLiteral("once"));
    });
    QObject::connect(e, &Engine::errorOccurred, [](const QString &, const QString &m) { out("\n[error] " + m); });
    QObject::connect(e, &Engine::busyChanged, [&](const QString &sid, bool busy) {
        out(QStringLiteral("\n[busy %1]").arg(busy));
        if (!busy && sid == session)
            QTimer::singleShot(1500, &app, [&] {
                e->loadHistory(session, [&](const QJsonArray &items) {
                    out(QStringLiteral("[history] %1 items: %2").arg(items.size()).arg(QString::fromUtf8(QJsonDocument(items).toJson(QJsonDocument::Compact)).left(800)));
                    e->stop();
                    app.quit();
                });
            });
    });
    QObject::connect(e, &Engine::ready, [&] {
        out("[ready] agents=" + QString::number(e->agents().size()));
        e->listCommands([](const QList<CommandDef> &c) { QStringList n; for (auto &x : c) n << x.name + "(" + x.source + ")"; out("[commands] " + n.join(", ")); });
        e->listModels([](const QList<ModelChoice> &m) { QStringList n; for (auto &x : m) n << x.ref(); out(QStringLiteral("[models %1] ").arg(m.size()) + n.mid(0, 12).join(", ")); });
        e->createSession([&](const QString &id, const QString &err) {
            if (id.isEmpty()) { out("[create failed] " + err); app.exit(1); return; }
            session = id;
            out("[session] " + id);
            QStringList sk; if (a.size() > 5) sk << a.at(5);
            e->send(id, a.at(4), sk, a.at(3), QStringLiteral("build"));
        });
    });
    QTimer::singleShot(240000, &app, [&] { out("[timeout]"); app.exit(3); });
    e->start(a.at(2));
    return app.exec();
}

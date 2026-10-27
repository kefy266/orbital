// Orbital — Qt Quick front end over the native and opencode engines.

#include "chatmodel.h"
#include "controller.h"
#include "iconprovider.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QMouseEvent>
#include <QTimer>

int main(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
    // Native file/folder pickers: KDE's platform theme only offers them to
    // QtWidgets apps, the desktop portal works for Qt Quick too (KDE, GNOME…).
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME"))
        qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
#endif
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Orbital"));
    QGuiApplication::setOrganizationName(QStringLiteral("Orbital"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.3.4"));
    QGuiApplication::setDesktopFileName(QStringLiteral("orbital"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/theme/orbital.svg")));
    if (QFontDatabase::families().contains(QStringLiteral("Inter")))
        QGuiApplication::setFont(QFont(QStringLiteral("Inter"), 10));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    AppController controller;
    qmlRegisterSingletonInstance("Orbital", 1, 0, "App", &controller);
    qmlRegisterUncreatableType<ChatModel>("Orbital", 1, 0, "ChatModel", QStringLiteral("provided by App"));
    qmlRegisterUncreatableType<SessionModel>("Orbital", 1, 0, "SessionModel", QStringLiteral("provided by App"));

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    // Dev aid: ORBITAL_SCREENSHOT=<file.png> [ORBITAL_SCREENSHOT_DELAY=ms] grabs the window and quits.
    const QString shot = qEnvironmentVariable("ORBITAL_SCREENSHOT");
    if (!shot.isEmpty()) {
        const int delay = qEnvironmentVariableIntValue("ORBITAL_SCREENSHOT_DELAY") > 0
                              ? qEnvironmentVariableIntValue("ORBITAL_SCREENSHOT_DELAY") : 4000;
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        const QString prompt = qEnvironmentVariable("ORBITAL_SCREENSHOT_PROMPT");
        if (!prompt.isEmpty())
            QTimer::singleShot(2500, &controller, [&controller, prompt] { controller.submit(prompt); });
        const QString session = qEnvironmentVariable("ORBITAL_SCREENSHOT_SESSION");
        if (!session.isEmpty())
            QTimer::singleShot(2500, &controller, [&controller, session] { controller.openSession(session); });
        // ORBITAL_SCREENSHOT_CLICKS="x,y;x,y": left clicks, 800 ms apart, starting at 2 s.
        const QStringList clicks = qEnvironmentVariable("ORBITAL_SCREENSHOT_CLICKS").split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (int i = 0; i < clicks.size(); ++i) {
            const QPointF pos(clicks.at(i).section(QLatin1Char(','), 0, 0).toDouble(),
                              clicks.at(i).section(QLatin1Char(','), 1, 1).toDouble());
            QTimer::singleShot(2000 + i * 800, window, [window, pos] {
                for (QEvent::Type type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
                    QMouseEvent ev(type, pos, pos, window->mapToGlobal(pos), Qt::LeftButton,
                                   type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
                    QCoreApplication::sendEvent(window, &ev);
                }
            });
        }
        QTimer::singleShot(delay, &app, [window, shot] {
            window->grabWindow().save(shot);
            QCoreApplication::quit();
        });
    }

    controller.start();
    return app.exec();
}

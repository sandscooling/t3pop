// t3pop: a quick popup that sends a message and screenshots to a project's
// orchestrator thread in T3 Code.

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <cstring>

#include "backend.h"

static QString configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/t3pop";
}

static QString dropDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/t3pop";
}

// Every non-GUI command prints JSON with --json and plain text otherwise,
// and reports failures on stderr with exit code 1.
static int runCommand(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments().mid(1);
    const bool json = args.removeAll("--json") > 0;
    const QString command = args.takeFirst();
    QTextStream out(stdout), err(stderr);
    Backend backend(configDir(), dropDir());

    if (command == "projects") {
        if (json) {
            out << QJsonDocument(QJsonArray::fromVariantList(backend.projects())).toJson(QJsonDocument::Compact)
                << "\n";
        } else {
            for (const QVariant &p : backend.projects())
                out << p.toMap().value("name").toString() << "\t" << p.toMap().value("path").toString() << "\n";
        }
        return 0;
    }

    if (command == "send") {
        QStringList words;
        while (!args.isEmpty()) {
            const QString a = args.takeFirst();
            if (a == "--project" && !args.isEmpty()) {
                const QString name = args.takeFirst();
                if (!backend.selectProject(name)) {
                    err << "t3pop: no project named '" << name << "' (see: t3pop projects)\n";
                    return 1;
                }
            } else if (a == "--image" && !args.isEmpty()) {
                backend.addImage(args.takeFirst());
            } else {
                words << a;
            }
        }
        int code = 1;
        QObject::connect(&backend, &Backend::sent, &app, [&](const QString &id) {
            if (json)
                out << QJsonDocument(QJsonObject{{"threadId", id}}).toJson(QJsonDocument::Compact) << "\n";
            else
                out << id << "\n";
            code = 0;
            app.quit();
        });
        QObject::connect(&backend, &Backend::busyChanged, &app, [&] {
            if (!backend.busy() && code != 0) {
                err << "t3pop: " << backend.error() << "\n";
                app.quit();
            }
        });
        backend.send(words.join(' '));
        if (!backend.busy()) {
            err << "t3pop: " << (backend.error().isEmpty() ? "nothing to send" : backend.error()) << "\n";
            return 1;
        }
        app.exec();
        return code;
    }

    err << "t3pop: unknown command '" << command
        << "' (try: open, projects, send [--project NAME] [--image FILE]... MESSAGE)\n";
    return 1;
}

int main(int argc, char *argv[])
{
    if (argc > 1 && std::strcmp(argv[1], "open") != 0)
        return runCommand(argc, argv);

    QApplication app(argc, argv);
    app.setApplicationName("t3pop");
    app.setApplicationDisplayName("T3 Pop");
    app.setApplicationVersion("0.1.0");

    // Matches the window to t3pop.desktop, so KWin (Wayland app_id) and the
    // launcher pick up the installed icon.
    app.setDesktopFileName("t3pop");
    app.setWindowIcon(QIcon::fromTheme("t3pop"));

    // The hotkey may fire while a popup is already open (perhaps hidden with
    // a draft in it). Hand over to that one instead of opening a second.
    const QString socketName = "t3pop-" + qEnvironmentVariable("USER");
    {
        QLocalSocket probe;
        probe.connectToServer(socketName);
        if (probe.waitForConnected(300)) {
            probe.write("raise");
            probe.waitForBytesWritten(300);
            return 0;
        }
    }
    QLocalServer::removeServer(socketName);
    QLocalServer server;
    server.listen(socketName);

    // Plasma's own Qt Quick style: controls take the current color scheme and
    // follow a switch live. Falls back to Fusion outside Plasma.
    if (QQuickStyle::name().isEmpty())
        QQuickStyle::setStyle("org.kde.desktop");
    QQuickStyle::setFallbackStyle("Fusion");

    Backend backend(configDir(), dropDir());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    QObject *window = engine.rootObjects().first();

    QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
        while (QLocalSocket *s = server.nextPendingConnection()) {
            QObject::connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
            QMetaObject::invokeMethod(window, "summon");
        }
    });

    return app.exec();
}

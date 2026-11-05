#include <QtTest>

#include "backend.h"

class T3PopTests : public QObject {
    Q_OBJECT

private slots:
    void picksTheLiveOrchestrator()
    {
        // Newest first; the crew thread and the dated old orchestrator lose.
        const QByteArray json = R"([
            {"threadId":"crew","title":"17.9b-dev-2"},
            {"threadId":"live","title":"Orchestrator"},
            {"threadId":"old","title":"Orchestrator-2026-11-04-2"}])";
        QCOMPARE(Backend::findOrchestrator(json), QString("live"));
        QCOMPARE(Backend::findOrchestrator(R"([{"threadId":"x","title":"Orchestrator-old"}])"), QString());
        QCOMPARE(Backend::findOrchestrator("not json"), QString());
    }

    void buildsTheThreadDeepLink()
    {
        const QByteArray json = R"([{"threadId":"a","link":"https://app.t3.codes/threads/env-1/a"}])";
        QCOMPARE(Backend::findEnvironment(json), QString("env-1"));
        QCOMPARE(Backend::findEnvironment("[]"), QString());
        QCOMPARE(Backend::threadUrl("env-1", "mcp:36"), QString("t3code://threads/env-1/mcp%3A36"));
        QCOMPARE(Backend::threadUrl({}, "a"), QString());
    }

    void startsAnOrchestratorWhenNoneExists()
    {
        const SendPlan plan = Backend::planSend({}, "/p", "hi", {}, false);
        QCOMPARE(plan.args, QStringList({"thread", "start", "/p", "-", "--title", "Orchestrator"}));
        QCOMPARE(plan.stdinText, QString("hi"));
    }

    void freshModeStartsAnUntitledThread()
    {
        // Titled by the CLI from the first line, never "Orchestrator".
        SendPlan plan = Backend::planSend({}, "/hub", "Wi-Fi drops", {}, false, true);
        QCOMPARE(plan.args, QStringList({"thread", "start", "/hub", "-"}));
        plan = Backend::planSend({}, "/hub", "", {"/a.png"}, false, true);
        QCOMPARE(plan.args, QStringList({"thread", "start", "/hub", "-", "--title", "Screenshot"}));
    }

    void projectsDefaultToTheirOrchestrator()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/config");
        QFile f(dir.path() + "/config/projects.json");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"([{"name":"Hub","path":"/h","mode":"fresh"},{"name":"P","path":"/p"}])");
        f.close();
        Backend backend(dir.path() + "/config", dir.path() + "/drops");
        QCOMPARE(backend.projects()[0].toMap().value("mode").toString(), QString("fresh"));
        QCOMPARE(backend.projects()[1].toMap().value("mode").toString(), QString("orchestrator"));
    }

    void attachesImagesNativelyWhenTheCliCan()
    {
        const SendPlan plan = Backend::planSend("t1", "/p", "look", {"/a.png", "/b.png"}, true);
        QCOMPARE(plan.args, QStringList({"thread", "send", "t1", "-", "--image", "/a.png", "--image", "/b.png"}));
        QCOMPARE(plan.stdinText, QString("look"));
    }

    void listsImagePathsWhenTheCliCannot()
    {
        const SendPlan plan = Backend::planSend("t1", "/p", "look", {"/a.png"}, false);
        QCOMPARE(plan.args, QStringList({"thread", "send", "t1", "-"}));
        QVERIFY(plan.stdinText.startsWith("look\n\nAttached screenshot (open with the Read tool):"));
        QVERIFY(plan.stdinText.endsWith("- /a.png"));
    }

    void writesDefaultProjectsWithHubFirst()
    {
        QTemporaryDir dir;
        Backend backend(dir.path() + "/config", dir.path() + "/drops");
        QVERIFY(QFileInfo::exists(dir.path() + "/config/projects.json"));
        QCOMPARE(backend.projects().first().toMap().value("name").toString(), QString("Hub"));
        QCOMPARE(backend.projects().first().toMap().value("path").toString(), QDir::homePath() + "/Hub");
        QVERIFY(backend.selectProject("t3 code"));
        QCOMPARE(backend.projectIndex(), 1);
        QVERIFY(!backend.selectProject("nope"));
    }

    void reportsABrokenProjectFile()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/config");
        QFile f(dir.path() + "/config/projects.json");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{broken");
        f.close();
        Backend backend(dir.path() + "/config", dir.path() + "/drops");
        QVERIFY(!backend.error().isEmpty());
        QCOMPARE(backend.projects().size(), Backend::defaultProjects().size());
    }

    void removesImagesFromTheList()
    {
        QTemporaryDir dir;
        Backend backend(dir.path() + "/config", dir.path() + "/drops");
        backend.addImage("/a.png");
        backend.addImage("/b.png");
        backend.removeImage(0);
        QCOMPARE(backend.images(), QStringList({"/b.png"}));
    }
};

QTEST_MAIN(T3PopTests)
#include "t3pop_tests.moc"

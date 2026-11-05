#include "backend.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QUrl>

namespace {

// The live orchestrator thread in every project carries exactly this title;
// older ones are renamed with a date suffix when a new one takes over.
const QString kOrchestratorTitle = QStringLiteral("Orchestrator");

// Screenshots only need to outlive the agent reading them.
const int kDropKeepDays = 14;

QString expandHome(QString path)
{
    if (path == "~" || path.startsWith("~/"))
        path.replace(0, 1, QDir::homePath());
    return path;
}

} // namespace

Backend::Backend(const QString &configDir, const QString &dropDir, QObject *parent)
    : QObject(parent), m_configDir(configDir), m_dropDir(dropDir)
{
    loadProjects();
    pruneDrops();
}

QString Backend::t3Path()
{
    return QDir::homePath() + "/.t3/bin/t3";
}

QVariantList Backend::defaultProjects()
{
    auto entry = [](const QString &name, const QString &path, const QString &mode) {
        return QVariantMap{{"name", name}, {"path", path}, {"mode", mode}};
    };
    // Hub keeps no running conversation (its handoff doc carries knowledge
    // between sessions), so each message starts its own thread there.
    return {entry("Hub", "~/Hub", "fresh"), entry("T3 Code", "~/source/t3code", "orchestrator")};
}

// Projects live in a small JSON file so one can be added without a rebuild.
// The first entry is the default target. A missing file is created with the
// defaults; an unreadable one is reported rather than silently replaced.
void Backend::loadProjects()
{
    const QString file = m_configDir + "/projects.json";
    QVariantList list;

    if (!QFileInfo::exists(file)) {
        list = defaultProjects();
        QDir().mkpath(m_configDir);
        QSaveFile out(file);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(QJsonDocument(QJsonArray::fromVariantList(list)).toJson());
            out.commit();
        }
    } else {
        QFile in(file);
        QJsonParseError parseError;
        const QJsonDocument doc = in.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(in.readAll(), &parseError)
                                                               : QJsonDocument();
        if (!doc.isArray()) {
            setError(tr("Could not read %1; using the built-in project list.").arg(file));
            list = defaultProjects();
        } else {
            list = doc.array().toVariantList();
        }
    }

    m_projects.clear();
    for (const QVariant &v : list) {
        QVariantMap map = v.toMap();
        if (map.value("name").toString().isEmpty() || map.value("path").toString().isEmpty())
            continue;
        map["path"] = expandHome(map.value("path").toString());
        // Anything but "fresh" means the project's orchestrator thread.
        if (map.value("mode").toString() != "fresh")
            map["mode"] = "orchestrator";
        m_projects << map;
    }
    emit projectsChanged();
}

void Backend::pruneDrops()
{
    QDir dir(m_dropDir);
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-kDropKeepDays);
    for (const QFileInfo &info : dir.entryInfoList({"*.png"}, QDir::Files)) {
        if (info.lastModified() < cutoff)
            QFile::remove(info.absoluteFilePath());
    }
}

QString Backend::newDropPath() const
{
    QDir().mkpath(m_dropDir);
    const QString stamp = QDateTime::currentDateTime().toString("yyyy-MM-dd-HHmmss-zzz");
    return m_dropDir + "/" + stamp + ".png";
}

void Backend::setProjectIndex(int index)
{
    if (index < 0 || index >= m_projects.size() || index == m_projectIndex)
        return;
    m_projectIndex = index;
    emit projectIndexChanged();
}

bool Backend::selectProject(const QString &name)
{
    for (int i = 0; i < m_projects.size(); ++i) {
        if (m_projects[i].toMap().value("name").toString().compare(name, Qt::CaseInsensitive) == 0) {
            setProjectIndex(i);
            return true;
        }
    }
    return false;
}

void Backend::addImage(const QString &path)
{
    m_images << path;
    emit imagesChanged();
}

// Returns false when the clipboard holds no image, so the caller can fall
// back to an ordinary text paste.
bool Backend::pasteImage()
{
    const QImage image = QGuiApplication::clipboard()->image();
    if (image.isNull())
        return false;
    const QString path = newDropPath();
    if (!image.save(path, "PNG")) {
        setError(tr("Could not save the pasted image to %1.").arg(path));
        return true;
    }
    addImage(path);
    return true;
}

void Backend::removeImage(int index)
{
    if (index < 0 || index >= m_images.size())
        return;
    // Only the list entry goes; the file stays until pruned, since it may be
    // a copy the user still wants.
    m_images.removeAt(index);
    emit imagesChanged();
}

// Spectacle's region grab runs in the background while the window is hidden
// (the QML side hides it on captureStarted), so the popup is not in the shot.
void Backend::capture()
{
    const QString path = newDropPath();
    auto *proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this, proc, path] {
        proc->deleteLater();
        if (QFileInfo::exists(path))
            addImage(path);
        emit captureFinished();
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        proc->deleteLater();
        setError(tr("Spectacle is not installed, so screenshots cannot be taken."));
        emit captureFinished();
    });
    emit captureStarted();
    proc->start("spectacle", {"--region", "--background", "--nonotify", "--output", path});
}

QString Backend::findOrchestrator(const QByteArray &threadListJson)
{
    // The list is newest first, so the first exact match is the live one.
    for (const QJsonValue &v : QJsonDocument::fromJson(threadListJson).array()) {
        const QJsonObject t = v.toObject();
        if (t.value("title").toString() == kOrchestratorTitle)
            return t.value("threadId").toString();
    }
    return {};
}

// Every row's link reads https://app.t3.codes/threads/<environmentId>/<threadId>.
QString Backend::findEnvironment(const QByteArray &threadListJson)
{
    for (const QJsonValue &v : QJsonDocument::fromJson(threadListJson).array()) {
        const QStringList parts = QUrl(v.toObject().value("link").toString()).path().split('/', Qt::SkipEmptyParts);
        if (parts.size() >= 3 && parts.at(0) == "threads")
            return parts.at(1);
    }
    return {};
}

// The desktop app's deep link; thread ids can hold ':' (mcp:...), which must be encoded.
QString Backend::threadUrl(const QString &environmentId, const QString &threadId)
{
    if (environmentId.isEmpty() || threadId.isEmpty())
        return {};
    return QString("t3code://threads/%1/%2")
        .arg(QString::fromUtf8(QUrl::toPercentEncoding(environmentId)),
             QString::fromUtf8(QUrl::toPercentEncoding(threadId)));
}

SendPlan Backend::planSend(const QString &threadId, const QString &projectPath, const QString &text,
                           const QStringList &images, bool imageFlag, bool fresh)
{
    SendPlan plan;
    if (fresh) {
        // The CLI titles a new thread from the message's first line; an
        // image-only message needs a title of its own.
        plan.args = {"thread", "start", projectPath, "-"};
        if (text.trimmed().isEmpty())
            plan.args << "--title" << "Screenshot";
    } else if (threadId.isEmpty())
        plan.args = {"thread", "start", projectPath, "-", "--title", kOrchestratorTitle};
    else
        plan.args = {"thread", "send", threadId, "-"};

    plan.stdinText = text;
    if (imageFlag) {
        for (const QString &image : images)
            plan.args << "--image" << image;
    } else if (!images.isEmpty()) {
        // Until the CLI can attach images, the agent gets their paths and
        // opens them itself.
        QStringList lines;
        for (const QString &image : images)
            lines << "- " + image;
        plan.stdinText += QString("\n\nAttached screenshot%1 (open with the Read tool):\n%2")
                              .arg(images.size() > 1 ? "s" : "", lines.join("\n"));
    }
    return plan;
}

// The CLI gains --image in a later T3 build; reading its help once lets the
// popup switch to native attachments as soon as that build is installed.
bool Backend::detectImageFlag()
{
    if (m_imageFlag < 0) {
        QProcess help;
        help.start(t3Path(), {"thread", "send", "--help"});
        help.waitForFinished(5000);
        m_imageFlag = help.readAll().contains("--image") ? 1 : 0;
    }
    return m_imageFlag == 1;
}

void Backend::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void Backend::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}

void Backend::fail(const QString &error)
{
    setError(error);
    setBusy(false);
}

void Backend::send(const QString &text)
{
    if (m_busy)
        return;
    if (text.trimmed().isEmpty() && m_images.isEmpty())
        return;
    if (!QFileInfo(t3Path()).isExecutable())
        return fail(tr("The t3 CLI is missing at %1. Is T3 Code installed?").arg(t3Path()));
    if (m_projects.isEmpty())
        return fail(tr("No projects are configured."));

    setError({});
    setBusy(true);

    if (currentIsFresh())
        return runSend({}, text, true);

    const QString path = m_projects.value(m_projectIndex).toMap().value("path").toString();
    auto *list = new QProcess(this);
    connect(list, &QProcess::finished, this, [this, list, text](int code) {
        list->deleteLater();
        const QByteArray out = list->readAllStandardOutput();
        if (code != 0) {
            const QString err = QString::fromUtf8(list->readAllStandardError()).trimmed();
            return fail(err.isEmpty() ? tr("t3 thread list failed (exit %1).").arg(code) : err);
        }
        if (m_environmentId.isEmpty())
            m_environmentId = findEnvironment(out);
        runSend(findOrchestrator(out), text, false);
    });
    list->start(t3Path(), {"thread", "list", "--project", path, "--json"});
}

bool Backend::currentIsFresh() const
{
    return m_projects.value(m_projectIndex).toMap().value("mode").toString() == "fresh";
}

void Backend::runSend(const QString &threadId, const QString &text, bool fresh)
{
    const QString path = m_projects.value(m_projectIndex).toMap().value("path").toString();
    const SendPlan plan = planSend(threadId, path, text, m_images, detectImageFlag(), fresh);

    auto *proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this, proc, threadId](int code) {
        proc->deleteLater();
        if (code != 0) {
            const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            return fail(err.isEmpty() ? tr("t3 failed (exit %1).").arg(code) : err);
        }
        // A new thread prints its id; an existing one keeps the id we found.
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        const QString id = threadId.isEmpty() ? out.section('\n', -1) : threadId;
        m_images.clear();
        emit imagesChanged();
        setBusy(false);
        emit sent(id);
    });
    proc->start(t3Path(), plan.args);
    proc->write(plan.stdinText.toUtf8());
    proc->closeWriteChannel();
}

// Opens T3 Code on the thread the message went to, which also shows it under a
// collapsed project in the sidebar. Without a thread id, launching the app
// again just brings the running single-instance window forward.
void Backend::openT3(const QString &threadId)
{
    if (!threadId.isEmpty() && m_environmentId.isEmpty()) {
        // Fresh-mode sends skip the thread list, so read the id from it now.
        const QString path = m_projects.value(m_projectIndex).toMap().value("path").toString();
        QProcess list;
        list.start(t3Path(), {"thread", "list", "--project", path, "--json"});
        if (list.waitForFinished(5000))
            m_environmentId = findEnvironment(list.readAllStandardOutput());
    }
    const QString url = threadUrl(m_environmentId, threadId);
    if (url.isEmpty() || !QProcess::startDetached("xdg-open", {url}))
        QProcess::startDetached("gtk-launch", {"com.t3tools.T3Code"});
}

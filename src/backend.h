#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

class QProcess;

// One send as the t3 CLI needs it: the arguments, and the message text that
// goes to its stdin (the CLI reads the message from stdin when given "-").
struct SendPlan {
    QStringList args;
    QString stdinText;
};

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList projects READ projects NOTIFY projectsChanged)
    Q_PROPERTY(int projectIndex READ projectIndex WRITE setProjectIndex NOTIFY projectIndexChanged)
    Q_PROPERTY(QStringList images READ images NOTIFY imagesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    // configDir and dropDir are parameters so tests can point them at
    // temporary directories; the app passes the XDG locations.
    explicit Backend(const QString &configDir, const QString &dropDir, QObject *parent = nullptr);

    QVariantList projects() const { return m_projects; }
    int projectIndex() const { return m_projectIndex; }
    void setProjectIndex(int index);
    QStringList images() const { return m_images; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }

    // Selects a project by its name, ignoring case; false when none matches.
    bool selectProject(const QString &name);
    void addImage(const QString &path);

    Q_INVOKABLE bool pasteImage();
    Q_INVOKABLE void removeImage(int index);
    Q_INVOKABLE void capture();
    Q_INVOKABLE void send(const QString &text);
    Q_INVOKABLE void openT3(const QString &threadId = {});

    // Pure helpers, public so the tests can reach them.
    static QVariantList defaultProjects();
    static QString findOrchestrator(const QByteArray &threadListJson);
    static QString findEnvironment(const QByteArray &threadListJson);
    static QString threadUrl(const QString &environmentId, const QString &threadId);
    static SendPlan planSend(const QString &threadId, const QString &projectPath, const QString &text,
                             const QStringList &images, bool imageFlag, bool fresh = false);

    // Path of the t3 wrapper. It is not on PATH, so the app calls it directly.
    static QString t3Path();

signals:
    void projectsChanged();
    void projectIndexChanged();
    void imagesChanged();
    void busyChanged();
    void errorChanged();
    void captureStarted();
    void captureFinished();
    void sent(const QString &threadId);

private:
    void loadProjects();
    void pruneDrops();
    QString newDropPath() const;
    void setBusy(bool busy);
    void setError(const QString &error);
    void fail(const QString &error);
    bool detectImageFlag();
    void runSend(const QString &threadId, const QString &text, bool fresh);
    bool currentIsFresh() const;

    QString m_configDir;
    QString m_dropDir;
    QVariantList m_projects;
    int m_projectIndex = 0;
    QString m_environmentId;
    QStringList m_images;
    bool m_busy = false;
    QString m_error;
    int m_imageFlag = -1; // -1 unknown, then 0 or 1 once the CLI's help is read
};

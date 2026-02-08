#ifndef SNAPSHOTMANAGER_H
#define SNAPSHOTMANAGER_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QMap>
#include <QVariant>
#include <QThread>
#include <QMutex>
#include <atomic>


class DataLoader;

/*
 * The SnapshotSaveWorker class serializes loader data to a snapshot file in
 * a background thread and reports progress and errors.
 */
class SnapshotSaveWorker : public QObject
{
    Q_OBJECT

public:
    struct loaderInfo
    {
        QString name;
        QVector<double> X;
        QVector<double> Y;
    };

    // constructor
    SnapshotSaveWorker(const QString &savePath,
                       const QVector<loaderInfo> &loadersInfo,
                       const QString &description = "");
    
public slots:
    void process();
    void cancel() { cancelRequested.store(true); }

signals:
    void finished(bool ok, const QString &savePath);
    void progress(int percent);
    void error(const QString &errMsg);

private:
    QString savePath;
    QVector<loaderInfo> loadersInfo;
    QString description;
    std::atomic<bool> cancelRequested{false};

};


/*
 * The SnapshotLoadWorker class reads snapshot files in a background thread and
 * reconstructs loader data, reporting progress and errors.
 */
class SnapshotLoadWorker : public QObject
{
    Q_OBJECT

public:
    struct LoaderInfo
    {
        QString name {""};
        QVector<double> X;
        QVector<double> Y;
    };

    SnapshotLoadWorker(const QString &loadPath);

public slots:
    void process();
    void cancel() { cancelRequested.store(true); }

signals:
    void finished(bool ok, const QVector<LoaderInfo> &loadersInfo);
    void progress(int percent);
    void error(const QString &errMsg);

private:
    QString loadPath;
    std::atomic<bool> cancelRequested{false};
};


/*
 * The SnapshotManager class manages snapshot history (undo/redo) by saving and
 * restoring DataLoader states asynchronously.
 *
 * Responsibilities:
 * - Create and restore snapshots on worker threads
 * - Maintain undo/redo history and snapshot metadata
 * - Track progress, errors, and operation state
 * - Clean up old snapshots and manage snapshot storage directory
 */
class SnapshotManager : public QObject
{
    Q_OBJECT

public:

    static SnapshotManager& instance();

    SnapshotManager(const SnapshotManager&) = delete;
    SnapshotManager& operator=(const SnapshotManager&) = delete;

    void createSnapshotAsync(const QVector<DataLoader*> &loaders, 
        const QString &description);

    void restoreSnapshotAsync(int index);

    void clearHistory();
    
    // navigation
    bool canRedo() const;
    bool canUndo() const;

    void undo();
    void redo();

    struct SnapshotInfo 
    {
        QString description;
        QString filePath;
        qint64 fileSize;
        int loaderCount;
    };

    QVector<SnapshotManager::SnapshotInfo> getSnapshotList() const;
    SnapshotInfo getSnapshotInfo(int index) const;

    void removeAllSnapshots();

private:

    SnapshotManager();
    ~SnapshotManager();

    struct Snapshot
    {
        QVector<QString> loaderNames;
        QString description;
        int loaderCount;
        QString filePath;
    };

    QVector<Snapshot> snapshots;
    int currentSnapshotIndex {-1};
    QString snapshotsDir;
    int maxSnapshotsCount {10};

    void startSaveWorker(const QString &filePath,
                         const QVector<SnapshotSaveWorker::loaderInfo> &loadersInfo,
                         const QString &description);

    void startLoadWorker(const QString &filePath);

    QString generateSnapshotFileName() const;

    // worker thread
    QThread *workerThread {nullptr};

    // thread safety
    std::atomic<bool> operationInProgress{false};
    mutable QMutex mutex;

    // snapshot management
    void removeSnapshot(int index);
    void cleanOldSnapshots();

    // save metadata
    QString pendingDescription;
    int pendingLoaderCount {0};

private slots:
    void onSnapshotSaved(bool ok, const QString &filePath);
    void onSnapshotLoaded(bool ok, 
        const QVector<SnapshotLoadWorker::LoaderInfo> &loadersInfo);
    void onOperationProgress(int percent);
    void onOperationError(const QString &message);
    void onWorkerFinished(); 

signals:
    void snapshotCreationFailed(const QString &errMsg);
    void operationStarted(const QString &description);
    void snapshotCreated(int index, qint64 fileSize);
    void snapshotRestored(int index, const QVector<SnapshotLoadWorker::LoaderInfo> &info);
    void snapshotRestorationFailed(const QString &errMsg);

    void operationProgress(int percent);
    void operationFinished();
    
    void historyChanged();

};

#endif // SNAPSHOTMANAGER_H

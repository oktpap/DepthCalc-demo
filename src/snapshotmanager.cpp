#include "snapshotmanager.h"
#include "dataloader.h"
#include <QFile>
#include <QDir>
#include <QDataStream>
#include <QStandardPaths>
#include <QDebug>
#include <QMutexLocker>
#include <algorithm>
#include "dcsettings.h"

// --- SnapshotSaveWorker declarations ---
SnapshotSaveWorker::SnapshotSaveWorker(const QString &savePath,
                                       const QVector<loaderInfo> &loadersInfo,
                                       const QString &description)
    : savePath(savePath),
      loadersInfo(loadersInfo),
      description(description)
{}


void SnapshotSaveWorker::process()
{
    if (savePath.isEmpty() || loadersInfo.isEmpty())
    {
        emit error("Empty data for snapshot saving");
        emit finished(false, savePath);
        return;
    }

    int percent = 0;
    emit progress(percent);

    QFile file(savePath);

    if(!file.open(QIODevice::WriteOnly))
    {
        emit error("Cannot open saveFile");
        emit finished(false, savePath);
        return;
    }

    QDataStream out(&file);

    try
    {
        out << qint32(1); // format version
        out << description;
        out << qint32(loadersInfo.size()); // loader count

        for(int i = 0; i < loadersInfo.size(); i++)
        {
            if(cancelRequested.load())
            {
                file.close();
                QFile::remove(savePath);
                emit error("Snapshot saving cancelled");
                emit finished(false, savePath);
                return;
            }

            const auto X = loadersInfo[i].X;
            const auto Y = loadersInfo[i].Y;

            out << loadersInfo[i].name;
            out << qint32(X.size());

            // write data in blocks
            const int blockSize = 10000;
            for(int j = 0; j < X.size(); j += blockSize)
            {
                if(cancelRequested.load())
                {
                    file.close();
                    QFile::remove(savePath);
                    emit error("Snapshot saving cancelled");
                    emit finished(false, savePath);
                    return;
                }

                int currentBlockSize = qMin(blockSize, X.size() - j);

                // write blocks
                out.writeRawData(
                    reinterpret_cast<const char*>(X.constData() + j),
                    currentBlockSize * sizeof(double));

                percent = (i * 50 + (j * 50) / X.size()) / loadersInfo.size();

                emit progress(percent);
            }

            for(int j = 0; j < Y.size(); j += blockSize)
            {
                if(cancelRequested.load())
                {
                    file.close();
                    QFile::remove(savePath);
                    emit error("Snapshot saving cancelled");
                    emit finished(false, savePath);
                    return;
                }

                int currentBlockSize = qMin(blockSize, Y.size() - j);

                // write blocks
                out.writeRawData(
                    reinterpret_cast<const char*>(Y.constData() + j),
                    currentBlockSize * sizeof(double));

                int loaderProgress = 50 + (j * 50) / Y.size();  // 50-100%
                percent = (i * 100 + loaderProgress) / loadersInfo.size();

                emit progress(percent);
            }
        }

        file.close();

        emit progress(100);
        emit finished(true, savePath);

    } catch(const std::exception &e) {
        file.close();
        QFile::remove(savePath);
        emit error(QString("Error during snapshot saving: ") + e.what());
        emit finished(false, savePath);
        return;
    }
}


// --- SnapshotLoadWorker implementation ---
SnapshotLoadWorker::SnapshotLoadWorker(const QString &loadPath)
    : loadPath(loadPath)
{}


void SnapshotLoadWorker::process()
{
    if(loadPath.isEmpty())
    {
        emit error("Empty load path for snapshot loading");
        emit finished(false, QVector<LoaderInfo>());
        return;
    }

    int percent = 0;
    emit progress(percent);

    QFile file(loadPath);
    if(!file.open(QIODevice::ReadOnly))
    {
        emit error("Cannot open loadFile");
        emit finished(false, QVector<LoaderInfo>());
        return;
    }

    QDataStream in(&file);

    try
    {
        if(cancelRequested.load())
        {
            file.close();
            emit error("Snapshot loading cancelled");
            emit finished(false, QVector<LoaderInfo>());
            return;
        }

        qint32 version, loadersCount;
        QString description;

        in >> version; // format version
        in >> description;

        if(version != 1)
            throw std::runtime_error("Unsupported snapshot format version");

        in >> loadersCount; // loader count

        QVector<LoaderInfo> result;

        for(int i = 0; i < loadersCount; i++)
        {
            if(cancelRequested.load())
            {
                file.close();
               emit error("Snapshot loading cancelled");
               emit finished(false, QVector<LoaderInfo>());
               return;
            }

            LoaderInfo info;
            qint32 dataSize;
            QString name;

            in >> name >> dataSize;
            
            info.name = name;
            
            info.X.resize(dataSize);
            info.Y.resize(dataSize);

            in.readRawData(reinterpret_cast<char*>(info.X.data()), 
                          dataSize * sizeof(double));
            in.readRawData(reinterpret_cast<char*>(info.Y.data()), 
                          dataSize * sizeof(double));
            
            result.append(info);

            int percent = 10 + ((i + 1) * 80 / loadersCount);
            emit progress(percent);
        }

        file.close();
        emit progress(100);
        emit finished(true, result);
    }
    catch(const std::exception& e)
    {
        file.close();
        emit error(QString("Error during snapshot loading: ") + e.what());
        emit finished(false, QVector<LoaderInfo>());
        return;
    }
}


// --- SnapshotManager implementation ---
SnapshotManager::SnapshotManager()
{
    snapshotsDir = DCSettings::instance().getSnapshotsDir();
    qDebug() << "SnapshotManager: snapshots directory:" << snapshotsDir;

    QDir dir(snapshotsDir);
    if (!dir.exists()) 
    {
        if (!dir.mkpath("."))
            qWarning() << "Failed to create snapshots directory:" << snapshotsDir;
    }
}


SnapshotManager::~SnapshotManager()
{
    // Safe thread shutdown
    if (workerThread && !workerThread->isFinished()) {
        workerThread->requestInterruption();
        workerThread->quit();
        if (!workerThread->wait(3000)) {  // Wait up to 3 seconds
            qWarning() << "SnapshotManager: worker thread did not finish in time";
            workerThread->terminate();  // Force terminate
            workerThread->wait();
        }
    }
    // Do not delete manually - Qt will delete via parent relationship
}


SnapshotManager& SnapshotManager::instance()
{
    static SnapshotManager obj;
    return obj;
}


void SnapshotManager::createSnapshotAsync(const QVector<DataLoader*> &loaders,
    const QString &description)        
{
    if(operationInProgress.load())
    {
        qWarning() << "SnapshotManager::createSnapshotAsync: operation already in progress";
        emit snapshotCreationFailed("Another operation is already in progress");
        return;
    }


    if(loaders.isEmpty())
    {
        qWarning() << "SnapshotManager::createSnapshotAsync: empty loaders list";
        emit snapshotCreationFailed("Empty loaders list");
        return;
    }

    QMutexLocker locker(&mutex);

    if(currentSnapshotIndex < snapshots.size() - 1)
    {
        for(int i = snapshots.size() - 1; i > currentSnapshotIndex; i--)
            removeSnapshot(i);
        
        snapshots.resize(currentSnapshotIndex + 1);
    }
    
    QString desc = description.isEmpty() 
                  ? QString("Version %1").arg(snapshots.size() + 1)
                  : description;

    snapshotsDir = DCSettings::instance().getSnapshotsDir();
    QString filePath = generateSnapshotFileName();

    QVector<SnapshotSaveWorker::loaderInfo> loadersInfo;

    for(int i = 0; i < loaders.size(); i++)
    {
        if(!loaders[i]) continue;
        SnapshotSaveWorker::loaderInfo info;
        info.name = loaders[i]->getName();
        info.X = loaders[i]->getX();
        info.Y = loaders[i]->getY();

        if(!(info.X.isEmpty() || info.Y.isEmpty()))
            loadersInfo.append(info);
    }

    pendingDescription = desc;
    pendingLoaderCount = loadersInfo.size();

    locker.unlock();

    startSaveWorker(filePath, loadersInfo, desc);
    
}


void SnapshotManager::startSaveWorker(const QString &filePath,
                                     const QVector<SnapshotSaveWorker::loaderInfo> &loadersInfo,
                                     const QString &description)
{
    operationInProgress.store(true);
    
    emit operationStarted("Saving");
    workerThread = new QThread();  // WITHOUT parent - avoid double deletion
    auto *worker = new SnapshotSaveWorker(filePath, loadersInfo, description);
    worker->moveToThread(this->workerThread);

    // connect signals and slots
    connect(this->workerThread, &QThread::started, worker, &SnapshotSaveWorker::process);

    connect(worker, &SnapshotSaveWorker::finished, this, 
        &SnapshotManager::onSnapshotSaved);

    connect(worker, &SnapshotSaveWorker::progress, this, 
        &SnapshotManager::onOperationProgress);

    connect(worker, &SnapshotSaveWorker::error, this, &SnapshotManager::onOperationError);

    connect(worker, &SnapshotSaveWorker::finished, this, 
        &SnapshotManager::onWorkerFinished);

    connect(worker, &SnapshotSaveWorker::finished, worker, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    // start thread
    workerThread->start();
}


void SnapshotManager::onSnapshotSaved(bool ok, const QString &filePath)
{
    QMutexLocker locker(&mutex);
    
    if (ok) 
    {
        Snapshot snapshot;
        snapshot.filePath = filePath;
        snapshot.loaderCount = pendingLoaderCount; 
        snapshot.description = pendingDescription;

        
        snapshots.append(snapshot);
        currentSnapshotIndex = snapshots.size() - 1;
        
        cleanOldSnapshots();
        
        qDebug() << "SnapshotManager: snapshot saved";

        emit snapshotCreated(currentSnapshotIndex, 0);
        emit historyChanged();

        pendingDescription.clear();
        pendingLoaderCount = 0;

    } else emit snapshotCreationFailed("Failed to save snapshot");
}


void SnapshotManager::restoreSnapshotAsync(int index)
{
    if (operationInProgress.load()) {
        qWarning() << "SnapshotManager: operation already in progress";
        emit snapshotRestorationFailed("Another operation is already in progress");
        return;
    }

    QMutexLocker locker(&mutex);
    
    if (index < 0 || index >= snapshots.size()) {
        emit snapshotRestorationFailed("Invalid snapshot index");
        return;
    }
    
    QString filePath = snapshots[index].filePath;
    locker.unlock();
    
    startLoadWorker(filePath);
}


void SnapshotManager::startLoadWorker(const QString &filePath)
{
    operationInProgress.store(true);
    
    emit operationStarted("Restoring snapshot");
    
    workerThread = new QThread();  // WITHOUT parent - avoid double deletion
    auto *worker = new SnapshotLoadWorker(filePath);
    
    worker->moveToThread(workerThread);
    
    // connect signals and slots
    connect(workerThread, &QThread::started, worker, &SnapshotLoadWorker::process);

    connect(worker, &SnapshotLoadWorker::finished, this, 
        &SnapshotManager::onSnapshotLoaded);

    connect(worker, &SnapshotLoadWorker::progress, this, 
        &SnapshotManager::onOperationProgress);

    connect(worker, &SnapshotLoadWorker::error, this, 
        &SnapshotManager::onOperationError);

    connect(worker, &SnapshotLoadWorker::finished, this, 
        &SnapshotManager::onWorkerFinished);
    
    connect(worker, &SnapshotLoadWorker::finished, worker, &QObject::deleteLater);

    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
    
    // start thread
    workerThread->start();
}


QString SnapshotManager::generateSnapshotFileName() const
{
   QString currentTime = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
   return snapshotsDir + "/snapshot_" + currentTime + ".dcsnap"; 
}


void SnapshotManager::cleanOldSnapshots()
{
    if (snapshots.size() <= maxSnapshotsCount) {
        return;
    }
    
    int toRemove = snapshots.size() - maxSnapshotsCount;
    
    qDebug() << "SnapshotManager: cleaning" << toRemove << "old snapshots";
    
    for (int i = 0; i < toRemove; ++i) {
        QFile::remove(snapshots[i].filePath);
    }
    
    snapshots.remove(0, toRemove);
    currentSnapshotIndex -= toRemove;
    if (currentSnapshotIndex < 0) currentSnapshotIndex = 0;
}


void SnapshotManager::removeSnapshot(int index)
{
    if(index >= 0 && index < snapshots.size())
        QFile::remove(snapshots[index].filePath);
}

void SnapshotManager::onOperationProgress(int percent)
{
    emit operationProgress(percent);
}


void SnapshotManager::onSnapshotLoaded(bool ok, const QVector<SnapshotLoadWorker::LoaderInfo> &loadersInfo)
{
    if (ok) 
    {
        qDebug() << "SnapshotManager: snapshot loaded, loaders:" << loadersInfo.size();

        emit snapshotRestored(currentSnapshotIndex, loadersInfo); // hook up to graph loading in DCC
        emit historyChanged();
    } 

    else emit snapshotRestorationFailed("Failed to load snapshot");
}


void SnapshotManager::onOperationError(const QString &message)
{
    qWarning() << "SnapshotManager: error -" << message;
}


void SnapshotManager::onWorkerFinished()
{    
    operationInProgress.store(false);
    emit operationFinished();
    
    if (workerThread) {
        workerThread = nullptr;
    }
}


void SnapshotManager::undo()
{
    if (canUndo()) 
    {
        int targetIndex = currentSnapshotIndex - 1;
        currentSnapshotIndex = targetIndex;
        restoreSnapshotAsync(targetIndex);
    }
}


void SnapshotManager::redo()
{
    if (canRedo()) 
    {
        int targetIndex = currentSnapshotIndex + 1;
        currentSnapshotIndex = targetIndex;
        restoreSnapshotAsync(targetIndex);
    }
}


bool SnapshotManager::canUndo() const
{
    QMutexLocker locker(&mutex);
    return currentSnapshotIndex > 0 && !operationInProgress.load();
}


bool SnapshotManager::canRedo() const
{
    QMutexLocker locker(&mutex);
    return currentSnapshotIndex < snapshots.size() - 1 && !operationInProgress.load();
}


void SnapshotManager::clearHistory()
{
    if (operationInProgress.load()) return;

    QMutexLocker locker(&mutex);
    
    for (const auto &snapshot : snapshots) {
        QFile::remove(snapshot.filePath);
    }
    
    snapshots.clear();
    currentSnapshotIndex = -1;
    
    emit historyChanged();
}

// delete all snapshots and the folder itself
void SnapshotManager::removeAllSnapshots()
{
    if (operationInProgress.load()) {
        qWarning() << "Cannot remove snapshots while operation is in progress";
        return;
    }

    QMutexLocker locker(&mutex);
    
    for (const auto &snapshot : snapshots) {
        QFile::remove(snapshot.filePath);
    }
    
    snapshots.clear();
    currentSnapshotIndex = -1;
    
    locker.unlock();
    
    QDir dir(snapshotsDir);
    if (dir.exists()) {
        if (dir.removeRecursively()) {
            qDebug() << "SnapshotManager: snapshots folder deleted:" << snapshotsDir;
        } else {
            qWarning() << "SnapshotManager: failed to delete folder:" << snapshotsDir;
        }
    }
}


QVector<SnapshotManager::SnapshotInfo> SnapshotManager::getSnapshotList() const
{
    QMutexLocker locker(&mutex);
    
    QVector<SnapshotInfo> result;
    for (const auto &snapshot : snapshots) 
    {
        SnapshotInfo info;
        info.description = snapshot.description;
        info.filePath = snapshot.filePath;
        info.loaderCount = snapshot.loaderCount;
        result.append(info);
    }
    
    return result;
}
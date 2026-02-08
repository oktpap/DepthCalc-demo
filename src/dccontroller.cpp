#include "dccontroller.h"
#include "mainwindow.h"
#include "gl1manager.h"
#include "calibrationmanager.h"
#include "FileConverter.h"
#include "mainwindow.h"
#include "dcsettings.h"
#include "przmanager.h"
#include "snapshotmanager.h"
#include <QThread>


DCController::DCController(QObject *parent)
    : QObject{parent}
{}

DCController &DCController::instance()
{
    static DCController obj;
    return obj;
}

// установка главного окна и подключение всех слотов
void DCController::setMainWindow(MainWindow *win)
{
    this->window = win;

    // MainWindow signals & slots
    connect(window, &MainWindow::goMainPlotInit, this, &DCController::loadFiles);
    connect(this, &DCController::loaderRemoved, window, &MainWindow::removeLoader);
    connect(this, &DCController::loaderAdded, window, &MainWindow::addLoader);
    connect(window, &MainWindow::goDetLoad, this, &DCController::DetADNLoad);
    connect(window, &MainWindow::goManLoad, this, &DCController::manualADNLoad);
    connect(window, &MainWindow::goPDcreate, this, &DCController::PDcreate);
    connect(window, &MainWindow::showPalette, this, &DCController::applyPalette);
    connect(window, &MainWindow::goPrzConvert, this, &DCController::przConvert);
    connect(window, &MainWindow::goSavePD, this, &DCController::savePDOLFile);
    connect(this, &DCController::przToPDFinished, window, &MainWindow::przToPDFinished);
    connect(window, &MainWindow::goGl1Create, this, &DCController::gl1Create);
    connect(window, &MainWindow::goDeleteInterval, this, &DCController::deleteInterval);
    connect(window, &MainWindow::goAddInterval, this, &DCController::addInterval);
    connect(mainPlot, &PlotWidget::graphSelected, window, &MainWindow::graphSelected);
    connect(window, &MainWindow::goShiftGraph, this, &DCController::shiftGraph);
    connect(window, &MainWindow::goCancelShift, this, &DCController::cancelShift);
    connect(window, &MainWindow::goOpenMeasure, this, &DCController::openMeasure);
    connect(window, &MainWindow::goCleanMeasure, this, &DCController::clearMeasure);
    connect(window, &MainWindow::goGl1Correction, this, &DCController::gl1Correction);
    connect(window, &MainWindow::goSaveGl1, this, &DCController::saveGl1File);
    connect(window, &MainWindow::goPdCorrection, this, &DCController::pdCorrection);
    connect(window, &MainWindow::goCleanAll, this, &DCController::cleanAll);
    connect(this, &DCController::filesLoadFinished, window, &MainWindow::filesLoadFinished);
    connect(window, &MainWindow::sendFirstDvlPoint, this, &DCController::getFirstDvlPoint);
    connect(window, &MainWindow::sendSecondDvlPoint, this, &DCController::getSecondDvlPoint);
    connect(window, &MainWindow::goPrzCreate, this, &DCController::przCreate);
    connect(window, &MainWindow::goCleanAll, &PrzManager::instance(), &PrzManager::clear);
    connect(&PrzManager::instance(), &PrzManager::debug, window, &MainWindow::initLogDebug);
    connect(this, &DCController::manualLoadAdded, window, &MainWindow::manualLoadAdded);
    connect(window, &MainWindow::goUndo, this, &DCController::performUndo);
    connect(window, &MainWindow::goRedo, this, &DCController::performRedo);

    // Gl1Manager signal & slots
    connect(&Gl1Manager::instance(), &Gl1Manager::przToPDDone, this, &DCController::przToPDDone);
    connect(&Gl1Manager::instance(), &Gl1Manager::pdToGl1Done, this, &DCController::pdToGl1Done);
    connect(&Gl1Manager::instance(), &Gl1Manager::candleCorrectionDone, mainPlot, static_cast<void (PlotWidget::*)(DataLoader *)>(&PlotWidget::updateGraph));
    connect(&Gl1Manager::instance(), &Gl1Manager::lengthCorrectionDone, mainPlot, static_cast<void (PlotWidget::*)(DataLoader *)>(&PlotWidget::updateGraph));

    // PlotWidget signals & slots
    connect(mainPlot, &PlotWidget::intervalsChanged, this, &DCController::intervalsChanged);
    connect(this, &DCController::loaderRemoved, mainPlot, &PlotWidget::removeLoader);
    connect(this, &DCController::loaderAdded, mainPlot, &PlotWidget::addLoader);

    // PrzManager signals & slots
    connect(&PrzManager::instance(), &PrzManager::przCreated, this, &DCController::przCreated);

    // DCSettings signals & slots
    connect(&DCSettings::instance(), &DCSettings::graphColorsReseted, this, &DCController::graphColorsReseted);

    //ProgressDialog signals & slots
    connect(this, &DCController::synchronization, window, &MainWindow::synchronizationChanged);

    setupSnapshotManager();
}

QString DCController::lastPath() const
{
    QSettings settings("GORIZONT", "DepthCalc");
    // возвращаем путь. Если его нет, то домашний путь
    return settings.value("lastPath", QDir::homePath()).toString();
}

void DCController::saveLastPath(const QString &path)
{
    QSettings settings("GORIZONT", "DepthCalc");
    settings.setValue("lastPath", path);
}

DCController::~DCController()
{
    for (auto* th : findChildren<QThread*>())
    { th->requestInterruption(); th->quit(); th->wait(); }

    for (int i = 0; i < loaders.size(); i++)
        delete loaders[i];
}

void DCController::startConverter(const QString &path)
{
    QString fname = QFileInfo(path).fileName();
    int win = 0;
    if(fname.contains("DN", Qt::CaseInsensitive))
        win = window->ADNWindow();

    FileConverter* converter = new FileConverter(path, win);
    QThread* thread = new QThread(this);

    converter->moveToThread(thread);
    connect(thread,  &QThread::started, converter, &FileConverter::loadFiles);

    connect(converter, &FileConverter::progressUpdated,  this, &DCController::onProgress);
    connect(converter, &FileConverter::finished,  this, &DCController::onFinished);
    connect(converter, &FileConverter::errorOccurred, this, &DCController::onError);

    connect(converter, &FileConverter::finished, thread, &QThread::quit);
    connect(converter, &FileConverter::finished, converter, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

void DCController::onProgress(const QString &fileId, int percent)
{
    window->updateProgress(fileId, percent);
}

void DCController::onFinished(QString fileId, QVector<double> X,
                              QVector<double> Y, double syncFactor, double delta)
{
    DataLoader *loader = new DataLoader(X,Y,fileId);

    loaders.append(loader);
    syncFactors.append(syncFactor);

    if(deltaRealTime == 0 && (fileId.contains("DN", Qt::CaseInsensitive)
                       || fileId.contains("MK") || fileId.contains("KM") || fileId.contains("DV")))
    {
        deltaRealTime = delta;
        qDebug() << "delta:" << delta;
    }

    filesLoaded += 1;

    if(filesLoaded < filesToLoad) return;

    if(DCSettings::instance().getTimeSync())
    {
        int ADN = -1, MK = -1;
        bool dvl = false, ok = true;

        for (int i = 0; i < loaders.size(); i++)
        {
            QString name = loaders[i]->getName();
            loaders[i]->setDeltaTime(deltaRealTime);

            if(name.contains("DN", Qt::CaseInsensitive))
                ADN = i;

            else if(name.contains("MK", Qt::CaseInsensitive) || name.contains("KM", Qt::CaseInsensitive))
                MK = i;

            else if(name.contains("DV", Qt::CaseInsensitive))
                dvl = true;

            if(!syncFactorIsOK(syncFactors[i]))
                ok = false;
        }

        if(dvl && ok)
        {
            for(int k = 0; k < loaders.size(); k++)
            {
                loaders[k]->timeSync(syncFactors[k]);
                window->initLogDebug("Коэффициент коррекции " + loaders[k]->getName() + ": " + QString::number(syncFactors[k], 'f', 5));
            }
        }

        else if (ADN != -1 && MK != -1)
        {
            if (syncFactorIsOK(syncFactors[MK])
                && syncFactorIsOK(syncFactors[ADN]))
            {
                loaders[MK]->timeSync(syncFactors[MK]);
                loaders[ADN]->timeSync(syncFactors[ADN]);

                window->syncIsDone(syncFactors[MK], syncFactors[ADN]);
            }

            else
            {
                window->syncIsDone(0, 0);
            }
        }

        else if (MK == -1 && ADN != -1
                 && syncFactorIsOK(syncFactors[ADN]))
        {
            loaders[ADN]->timeSync(syncFactors[ADN]);
            window->syncIsDone(0, syncFactors[ADN]);
        }

        else if (ADN == -1 && MK != -1
                 && syncFactorIsOK(syncFactors[MK]))
        {
            loaders[MK]->timeSync(syncFactors[MK]);
            window->syncIsDone(syncFactors[MK], 0);
        }
    }

    else
        window->syncIsDone(0, 0);

    QVector<const DataLoader*> loadersToSend;
    for(int i = 0; i < loaders.size(); i++)
        loadersToSend.append(loaders[i]);

    window->initMainPlot(loadersToSend);

    bool dvlFlag = false;

    for(int i = 0; i < loadersToSend.size(); i++)
    {
        if(loadersToSend[i]->getName().contains("DV", Qt::CaseInsensitive))
        {
            dvlFlag = true;
            break;
        }
    }

    emit filesLoadFinished();
    if (!dvlFlag)
        SnapshotManager::instance().createSnapshotAsync(loaders, "PRZ snapshot");
}

void DCController::onError(QString fileId, QString msg)
{
    filesToLoad -= 1;
}


bool DCController::resampleGl1(const QVector<double> &X,
                            const QVector<double> &Y,
                            double newStep,
                            QVector<int> &resX,
                            QVector<double> &resY)
{
    resX.clear();
    resY.clear();

    if(X.size() < 2 || Y.size() < 2) return false;

    double t = X[0];
    int frame = 0;

    int i = 0;

    while (t <= X.back())
    {
        while(i + 1 < X.size() && X[i + 1] < t)
            i++;

        if (i + 1 >= X.size())
            break;

        // точки, между которыми лежит искомая (со временем t)
        double x0 = X[i], x1 = X[i + 1];
        double y0 = Y[i], y1 = Y[i + 1];

        double y = y0 + (y1 - y0) * (t - x0) / (x1 - x0);

        resX.append(frame);
        resY.append(y);
        frame++;

        t += newStep;
    }

    return true;
}


bool DCController::syncFactorIsOK(const double &factor)
{
    if (factor <= 1.15 && factor >= 0.85)
        return true;
    return false;
}

void DCController::loadFiles(const QVector<QString> &paths, bool sync)
{
    filesToLoad = paths.size();
    filesLoaded = 0;
    deltaRealTime = 0;
    syncFactors.clear();

    for(int i = 0; i < paths.size(); i++)
    {
        QString name = QFileInfo(paths[i]).fileName();
        if(name.contains("DV", Qt::CaseInsensitive))
            filesToLoad += 1;
        startConverter(paths[i]);
    }
}

// слот для авто определения пор. нагрузки
// (не подключен)
void DCController::DetADNLoad()
{
    Gl1Manager::instance().setLoaders(loaders);
    bool ok = Gl1Manager::instance().detLoad();

    qDebug() << ok;
}

// метод ручной установки пор. нагрузки
void DCController::manualADNLoad(const double &lvl)
{
    DataLoader* adn = nullptr;
    DataLoader* prz = nullptr;
    QVector<double> lenghts;
    QVector<QCPItemRect *> intervals;

    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i]->getName().contains("DN", Qt::CaseInsensitive))
            adn = loaders[i];
        if(loaders[i]->getName().contains("prz", Qt::CaseInsensitive)
            || loaders[i]->getName().contains("psc", Qt::CaseInsensitive))
            prz = loaders[i];
    }

    if(adn == nullptr || prz == nullptr) return;

    mainPlot->showLoad(lvl, adn, window->getLoadType());
    mainPlot->setLoadLine(lvl);
    mainPlot->getPDIntervals(intervals);

    Gl1Manager::instance().getLenghts(lenghts, &prz->getX(), &prz->getY(), intervals);

    for(int i = 0; i < intervals.size() && i < lenghts.size(); i++)
    {
        window->addIntervalRow("PDOL", i + 1, lenghts[i]);
        window->addIntervalRow("gl1", i + 1, lenghts[i]);
    }

    emit manualLoadAdded();
}

void DCController::przToPDDone(QVector<double> X, QVector<double> Y, QVector<double> lenghts)
{
    if (Y.isEmpty()) return;

    DataLoader *loader = new DataLoader(X, Y, "PDOL");
    loader->setParent(this);
    loaders.append(loader);

    emit loaderAdded(loader);
    Gl1Manager::instance().setLoaders(loaders);

    QVector<QCPItemRect*> intervals;
    mainPlot->getPDIntervals(intervals);
    intervalsChanged(intervals);
}

void DCController::PDcreate(double time,
                            double depth,
                            const double &firstPoint,
                            const double &secondPoint)
{
    DataLoader *prz = nullptr;
    QVector<QCPItemRect*> intervals;

    mainPlot->getPDIntervals(intervals);

    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i]->getName().contains("prz", Qt::CaseInsensitive))
        {
            prz = loaders[i];
        }

        if(loaders[i]->getName().contains("PDOL", Qt::CaseInsensitive))
        {
            emit loaderRemoved(loaders[i]);
            delete loaders[i];
            loaders.remove(i);
        }
    }

    if(prz != nullptr && !intervals.isEmpty())
    {
        Gl1Manager::instance().przToPD(&prz->getX(),
                                       &prz->getY(),
                                       intervals,
                                       time,
                                       depth,
                                       firstPoint,
                                       secondPoint);
    }

    mainPlot->cropIntervals(firstPoint, secondPoint);
}

void DCController::gl1Create(double time,
                             double depth,
                             const double &firstPoint,
                             const double &secondPoint,
                             const QString &direction,
                             const QString &method)
{
    DataLoader *prz = nullptr;
    QVector<QCPItemRect*> intervals;

    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i]->getName().contains("prz", Qt::CaseInsensitive))
        {
            prz = loaders[i];
        }

        if(loaders[i]->getName().contains("gl1", Qt::CaseInsensitive))
        {
            emit loaderRemoved(loaders[i]);
            delete loaders[i];
            loaders.remove(i);
        }
    }

    mainPlot->getPDIntervals(intervals);
    if(intervals.isEmpty()) return;

    double st, fn;

    if(prz != nullptr && !intervals.isEmpty())
    {
        Gl1Manager::instance().przToGl1(&prz->getX(),
                                       &prz->getY(),
                                       intervals,
                                       time,
                                       depth,
                                       firstPoint,
                                       secondPoint,
                                       direction,
                                       method);
    }

    mainPlot->cropIntervals(firstPoint, secondPoint);
}

void DCController::pdToGl1Done(QVector<double> X, QVector<double> Y)
{
    if (Y.isEmpty()) return;

    DataLoader *loader = new DataLoader(X, Y, "gl1");
    loader->setParent(this);
    loaders.append(loader);

    SnapshotManager::instance().createSnapshotAsync(loaders, "Глубина");

    emit loaderAdded(loader);
    Gl1Manager::instance().setLoaders(loaders);

    QVector<QCPItemRect*> intervals;
    mainPlot->getPDIntervals(intervals);
    intervalsChanged(intervals);

    emit pdToGl1Finished();
}

void DCController::applyPalette()
{
    QVector<const DataLoader*> loadersToSend;
    for(int i = 0; i < loaders.size(); i++)
        loadersToSend.append(loaders[i]);

    CalibrationManager::instance().setLoaders(loadersToSend);
    CalibrationManager::instance().setMkFactor(window->getMkFactor());

    QString firstPoint, secondPoint;
    window->getCalPoints(firstPoint, secondPoint);

    QDateTime dtStart = QDateTime::fromString(firstPoint, "hh:mm:ss dd-MM-yyyy");
    QDateTime dtFinish = QDateTime::fromString(secondPoint, "hh:mm:ss dd-MM-yyyy");

    if (!dtStart.isValid() || !dtFinish.isValid())
    {
        qWarning() << "Некорректный формат даты/времени";
        return;
    }

    double start = dtStart.toSecsSinceEpoch();
    double finish = dtFinish.toSecsSinceEpoch();

    if(CalibrationManager::instance().calibrate(start, finish))
    {
        CalibrationManager::instance().approximate();
        window->calFinished();
    }
}

void DCController::przConvert()
{
    double A = 0.0, B = 0.0;
    CalibrationManager::instance().getCalFactors(A, B);

    if(A == 0) return;

    int index = -1, mk = -1;
    for (int i = 0; i < loaders.size(); ++i)
    {
        if (loaders[i]->getName().contains("prz", Qt::CaseInsensitive))
            index = i;

        if (loaders[i]->getName().contains("MK", Qt::CaseInsensitive)
            || loaders[i]->getName().contains("KM", Qt::CaseInsensitive))
            mk = i;
    }
    if (index == -1) {
        qWarning() << "DCController::przConvert() - prz не найден";
        return;
    }

    DataLoader* prz = loaders[index];

    QVector<double> Y, przX, przY;
    double max = prz->max();

    przX = prz->getX();
    przY = prz->getY();

    if(przX.isEmpty() || przY.isEmpty()) return;

    Y.reserve(przY.size());

    for(int i = 0; i < przY.size(); i++)
        Y.append((((A * przY[i] * przY[i]) / 2 + B * przY[i])
                  - ((A * max * max) / 2 + B * max)) / 100);

    double min = *std::min_element(Y.begin(), Y.end());

    for(int i = 0; i < Y.size(); i++)
        Y[i] -= min; // привязка к нулю

    DataLoader *pd = new DataLoader(przX, Y, "przPT.psc");

    emit loaderRemoved(prz);
    loaders[index] = pd;
    emit loaderAdded(pd);

    if(mk >= 0)
    {
        DataLoader* mkLoader = loaders[mk];

        emit loaderRemoved(mkLoader);
        loaders.remove(mk);

        delete mkLoader;
    }

    delete prz;

    Gl1Manager::instance().setLoaders(loaders);
    SnapshotManager::instance().createSnapshotAsync(loaders, "Положение тальблока");
}

void DCController::savePDOLFile(const QString &path, const QString &format)
{
    QVector<double> X, Y;

    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i]->getName().contains("PDOL"))
        {
            X = loaders[i]->getX();
            Y = loaders[i]->getY();
        }
    }

    FileConverter fc(path);
    fc.savePD(X, Y, format);

    qDebug() << "PD saved:" << path;
}

// метод для инициализации строк таблиц интервалов (не используется)
void DCController::initTables(QVector<QCPItemRect *> *rectangles)
{
    for(int i = 0; i < rectangles->size(); i++)
    {
        window->addIntervalRow("PDOL", i + 1);
        window->addIntervalRow("gl1", i + 1);
    }
}


// слот для удаления интервала движения (пд или gl1)
void DCController::deleteInterval(const QString &first, const QString &second)
{
    // получение секунд из строки с датой и временем
    QDateTime dtStart = QDateTime::fromString(first, "hh:mm:ss.zzz dd-MM-yyyy");
    QDateTime dtFinish = QDateTime::fromString(second, "hh:mm:ss.zzz dd-MM-yyyy");

    if (!dtStart.isValid() || !dtFinish.isValid())
    {
        qWarning() << "Некорректный формат даты/времени";
        return;
    }

    double start  = dtStart.toMSecsSinceEpoch() / 1000.0;
    double finish = dtFinish.toMSecsSinceEpoch() / 1000.0;

    if (start > finish)
        std::swap(start, finish);

    // метод PlotWidget::deleteInterval()
    mainPlot->deleteInterval(start, finish);

    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i]->getName().contains("gl1", Qt::CaseInsensitive))
        {
            emit loaderRemoved(loaders[i]);
            delete loaders[i];
            loaders.remove(i);

            window->updateGl1();
        }

        if(loaders[i]->getName().contains("PDOL", Qt::CaseInsensitive))
        {
            emit loaderRemoved(loaders[i]);
            delete loaders[i];
            loaders.remove(i);

            window->updatePD();
        }
    }
}

void DCController::addInterval(const QString &first, const QString &second)
{
    QDateTime dtStart = QDateTime::fromString(first, "hh:mm:ss.zzz dd-MM-yyyy");
    QDateTime dtFinish = QDateTime::fromString(second, "hh:mm:ss.zzz dd-MM-yyyy");

    if (!dtStart.isValid() || !dtFinish.isValid())
    {
        qWarning() << "Некорректный формат даты/времени";
        return;
    }

    double start  = dtStart.toMSecsSinceEpoch() / 1000.0;
    double finish = dtFinish.toMSecsSinceEpoch() / 1000.0;

    if (start > finish)
        std::swap(start, finish);

    mainPlot->addInterval(start, finish);

    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i]->getName().contains("gl1", Qt::CaseInsensitive))
        {
            window->updateGl1();

            qDebug() << "UPDATE GL1";
        }

        if(loaders[i]->getName().contains("PDOL", Qt::CaseInsensitive))
        {
            window->updatePD();
            qDebug() << "UPDATE PDOL";
        }
    }
}

void DCController::intervalsChanged(const QVector<QCPItemRect *> &intervals)
{
    QVector<double> pdLenghts, pdDepth, pdSpeed, gl1Lenghts, gl1Depth, gl1Speed;
    const QVector<double> *pdX = nullptr, *pdY = nullptr, *gl1X = nullptr, *gl1Y = nullptr;

    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i]->getName().contains("PDOL", Qt::CaseInsensitive))
        {
            pdX = &loaders[i]->getX();
            pdY = &loaders[i]->getY();
            window->clearTable("PDOL");
        }

        if(loaders[i]->getName().contains("gl1", Qt::CaseInsensitive))
        {
            gl1X = &loaders[i]->getX();
            gl1Y = &loaders[i]->getY();
            window->clearTable("gl1");
        }
    }

    if(pdX && pdY && !pdX->isEmpty() && !pdY->isEmpty())
    {
        Gl1Manager::instance().getParams(pdLenghts,
                                         pdDepth,
                                         pdSpeed,
                                         pdX, pdY,
                                         intervals);

        for (int i = 0; i < intervals.size(); i++)
        {
            window->addIntervalRow("PDOL", i + 1, pdLenghts[i], -1, -1, pdDepth[i], pdSpeed[i]);
        }
    }

    if(gl1X && gl1Y && !gl1X->isEmpty() && !gl1Y->isEmpty())
    {
        Gl1Manager::instance().getParams(gl1Lenghts,
                                         gl1Depth,
                                         gl1Speed,
                                         gl1X, gl1Y,
                                         intervals);

        for (int i = 0; i < intervals.size() && i < gl1Lenghts.size() && i < gl1Depth.size() && i < gl1Speed.size(); i++)
        {
            window->addIntervalRow("gl1", i + 1, gl1Lenghts[i], -1, -1, gl1Depth[i], gl1Speed[i]);
        }
    }
}

// слот для сдвига кривых (в мс)
void DCController::shiftGraph(const double &num)
{
    const DataLoader *active = mainPlot->activeGraph();
    int index = -1;

    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i] == active)
        {
            index = i;
            break;
        }
    }

    if(index < 0) return;

    if(loaders[index]->getName().contains("DV", Qt::CaseInsensitive))
    {
        QString name = loaders[index]->getName().chopped(2);

        for(int i = 0; i < loaders.size(); i++)
        {
            if(loaders[i]->getName().contains(name))
                loaders[i]->shiftX(num / 1000.0);
        }

        return;
    }

    loaders[index]->shiftX(num / 1000);
}

void DCController::cancelShift()
{

    const DataLoader *active = mainPlot->activeGraph();
    int index = -1;

    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i] == active)
        {
            index = i;
            break;
        }
    }

    if(index < 0) return;

    double num = -loaders[index]->getShiftAmount();

    if(loaders[index]->getName().contains("DV", Qt::CaseInsensitive))
    {
        QString name = loaders[index]->getName().chopped(2);

        for(int i = 0; i < loaders.size(); i++)
        {
            if(loaders[i]->getName().contains(name))
                loaders[i]->shiftX(num);
        }

        return;
    }

    loaders[index]->shiftX(num);
}

void DCController::openMeasure(const QString &path)
{
    qDebug() << "openMeasure";

    if(!path.contains("DSV", Qt::CaseInsensitive)) return;

    QFile file(path);

    if(file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        measureData.clear();

        QTextStream in(&file);
        while(!in.atEnd())
        {
            QString line = in.readLine().trimmed();
            if(line.isEmpty())
                continue;

            bool ok;
            double val = line.toDouble(&ok);

            if(ok)
            {
                measureData.append(val);
            }
        }

        file.close();
    }

    else
    {
        qWarning() << "Не удалось открыть файл с мерой" << file.errorString();
        return;
    }

    double totalLen = 0.0; // суммарная длина свеч

    for(int i = 0; i < measureData.size(); i++)
        totalLen += measureData[i];

    window->fillMeasure(measureData, totalLen);
}

void DCController::clearMeasure()
{
    measureData.clear();
}

void DCController::gl1Correction(const bool &candle, const bool &len, double totalLen, 
        const double &refTime, const double &refDepth)
{
    DataLoader *loader = nullptr;
    QVector<QCPItemRect*> intervals;
    int refTotalLen;

    mainPlot->getPDIntervals(intervals);

    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i]->getName().contains("gl1", Qt::CaseInsensitive))
        {
            loader = loaders[i];
            break;
        }
    }

    if(!loader || intervals.isEmpty()) return;

    qDebug() << "candle:" << candle << "len:" << len;

    window->getRefTotalLen(refTotalLen);

    if(candle)
    {
        QVector<double> length, measureLength;
        double win;

        window->getMeasure("gl1", measureLength);
        window->getCandleLength("gl1", length);
        window->getCandleCorWin(win);

        Gl1Manager::instance().candleCorrection(loader,
                                                length,
                                                measureLength,
                                                intervals,
                                                win);

        intervalsChanged(intervals);
    }

    if(len)
    {
        Gl1Manager::instance().lengthCorrection(loader, refTotalLen);

        intervalsChanged(intervals);
    }

    if (candle || len)
    {
        Gl1Manager::instance().leavingCorrection(loader, refTime, refDepth / 100);
        intervalsChanged(intervals);
    }
}

void DCController::pdCorrection(const bool &candle, const bool &len, double totalLen, 
        const double &refTime, const double &refDepth)
{
    DataLoader *loader = nullptr;
    QVector<QCPItemRect*> intervals;
    int refTotalLen;

    mainPlot->getPDIntervals(intervals);

    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i]->getName().contains("PDOL", Qt::CaseInsensitive))
        {
            loader = loaders[i];
            break;
        }
    }

    if(!loader || intervals.isEmpty()) return;

    window->getPdRefTotalLen(refTotalLen);

    if(candle)
    {
        QVector<double> length, measureLength;
        double win;

        window->getMeasure("PDOL",measureLength);
        window->getCandleLength("PDOL", length);
        window->getPdCandleCorWin(win);

        Gl1Manager::instance().candleCorrection(loader,
                                                length,
                                                measureLength,
                                                intervals,
                                                win);

        intervalsChanged(intervals);
    }

    if(len)
    {
        Gl1Manager::instance().lengthCorrection(loader, refTotalLen);
        intervalsChanged(intervals);
    }

    if (candle || len)
    {
        Gl1Manager::instance().leavingCorrection(loader, refTime, refDepth / 100);
        intervalsChanged(intervals);
    }
}

void DCController::saveGl1File(const QString &path, int startFrame)
{
    QVector<double> X, Y, resY;
    QVector<int> resX;

    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i]->getName().contains("gl1", Qt::CaseInsensitive))
        {
            X = loaders[i]->getX();
            Y = loaders[i]->getY();
            break;
        }
    }

    if(X.isEmpty() || Y.isEmpty()) return;

    if(resampleGl1(X, Y, 2.097152, resX, resY))
    {
        qDebug() << "RESAMPLE RANGE:" << Y.back() - Y.front();

        FileConverter fc;
        if (!fc.saveGl1(resX, resY, startFrame, path))
            qWarning() << "Ошибка при сохранении файла gl1";
    }
}

void DCController::cleanAll()
{
    if (loaders.isEmpty()) return;

    QVector<DataLoader*> loadersToDelete = loaders;
    loaders.clear();
    
    for(int i = 0; i < loadersToDelete.size(); i++)
    {
        emit loaderRemoved(loadersToDelete[i]);
        delete loadersToDelete[i];
    }

    syncFactors.clear();
    filesToLoad = 0;
    filesLoaded = 0;

    window->clear();
}

void DCController::getFirstDvlPoint(const double &time1)
{
    int dn1 = -1, dn2 = -1;
    QString fName = "";

    for(int i = 0; i < loaders.size(); i++)
    {
        QString name = loaders[i]->getName().chopped(2);

        if(name.contains("DV", Qt::CaseInsensitive))
        {
            if(dn1 < 0)
            {
                dn1 = i;
                fName = name;
                continue;
            }

            else if(dn2 < 0 && fName != name)
            {
                dn2 = i;
                break;
            }
        }
    }

    double shift1_1 = loaders[dn1]->getShiftAmount();
    double shift2_1 = loaders[dn2]->getShiftAmount();

    double realTime = std::round(time1 / 0.008) * 0.008;

    PrzManager::instance().setFirstPoint(shift1_1, shift2_1, realTime);
}

void DCController::getSecondDvlPoint(const double &time2)
{
    int dv1 = -1, dv2 = -1;
    QString fName = "";

    for(int i = 0; i < loaders.size(); i++)
    {
        QString name = loaders[i]->getName().chopped(2);

        if(name.contains("DV", Qt::CaseInsensitive))
        {
            if(dv1 < 0)
            {
                dv1 = i;
                fName = name;
                continue;
            }

            else if(dv2 < 0 && fName != name)
            {
                dv2 = i;
                break;
            }
        }
    }

    double shift1_2 = loaders[dv1]->getShiftAmount();
    double shift2_2 = loaders[dv2]->getShiftAmount();

    double realTime = std::round(time2 / 0.008) * 0.008;

    PrzManager::instance().setSecondPoint(shift1_2, shift2_2, realTime);
}

void DCController::przCreate()
{
    PrzManager::instance().setLoaders(loaders);
    PrzManager::instance().przCreate();
}

void DCController::przCreated(QVector<double> &X, QVector<double> &Y)
{
    if(X.isEmpty() || Y.isEmpty()) return;

    for (int i = loaders.size() - 1; i >= 0; --i)
    {
        if (loaders[i]->getName().contains("DV", Qt::CaseInsensitive))
        {
            DataLoader* dvLoader = loaders[i];
            emit loaderRemoved(dvLoader);
            loaders.removeAt(i);
            delete dvLoader;
        }
    }

    qDebug() << "PRZ CREATED";

    DataLoader *loader = new DataLoader(X, Y, "1.prz");
    loader->setParent(this);

    // loader->resample(1.0);
    loaders.append(loader);
    SnapshotManager::instance().createSnapshotAsync(loaders, "prz");

    emit loaderAdded(loader);
    window->przCreated();
}

void DCController::graphColorsReseted()
{
    mainPlot->updateGraphColors();
}

void DCController::setPlot(PlotWidget *p)
{
    this->mainPlot = p;
}


void DCController::setupSnapshotManager()
{
    auto &sm = SnapshotManager::instance();

    // старт операции
    connect(&sm, &SnapshotManager::operationStarted, this,
            [this](const QString &op) {window->startLoadingProgress(op); });

    // прогресс операции
    connect(&sm, &SnapshotManager::operationProgress, this, [this](int percent) {
        window->updateLoadingProgress(percent);
    });

    // завершение операции
    connect(&sm, &SnapshotManager::operationFinished, this, [this]() {
        window->finishLoadingProgress();
    });

    // восстановление снимка
    connect(&sm, &SnapshotManager::snapshotRestored, this, 
    [this](int index, const QVector<SnapshotLoadWorker::LoaderInfo> &info) {
        applySnapshotToLoaders(info);
    });

    // удаление снимков при полной очистке
    connect(window, &MainWindow::goCleanAll, &sm, 
        &SnapshotManager::removeAllSnapshots);
}


void DCController::applySnapshotToLoaders
        (const QVector<SnapshotLoadWorker::LoaderInfo> &loadersInfo)
{
    window->setEnabled(false);
    cleanAll();

    for(int i = 0; i < loadersInfo.size(); i++)
    {
        const auto &info = loadersInfo[i];
        QVector<double> X = info.X;
        QVector<double> Y = info.Y;
        DataLoader *loader = new DataLoader(X, Y, info.name);
        loader->setParent(this);
        loaders.append(loader);
    }

    QVector<const DataLoader*> loadersToSend;
    for(int i = 0; i < loaders.size(); i++)
        loadersToSend.append(loaders[i]);

    window->initMainPlot(loadersToSend);

    window->updateStage();
    window->snapshotHistoryChanged();
    window->setEnabled(true);
}


void DCController::performUndo()
{
    auto &sm = SnapshotManager::instance();
    if (!sm.canUndo()) return;
    
    sm.undo();
}


void DCController::performRedo()
{
    auto &sm = SnapshotManager::instance();
    if (!sm.canRedo()) return;
    
    sm.redo();
}

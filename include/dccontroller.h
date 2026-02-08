#ifndef DCCONTROLLER_H
#define DCCONTROLLER_H

#include <QObject>
#include "dataloader.h"
#include "plotwidget.h"
#include "mainwindow.h"
#include "snapshotmanager.h"

/*
 * The DCController class coordinates file loading, conversions, plotting,
 * synchronization, and snapshot workflows across the application.
 *
 * Responsibilities:
 * - Manage DataLoader instances and their lifecycle
 * - Orchestrate PRZ/PD/GL1 conversion pipelines and corrections
 * - Coordinate plot updates, interval tables, and UI actions
 * - Handle synchronization, time shifting, and resampling
 * - Integrate snapshot save/restore with loaders and UI
 */
class DCController : public QObject
{
    Q_OBJECT

public:

    explicit DCController(QObject *parent = nullptr);

    static DCController& instance();

    void setLoaders(QVector<DataLoader *> &list);

    void setPlot(PlotWidget* p);

    void setMainWindow(MainWindow* win);

    QString lastPath() const;

    void saveLastPath(const QString &path);

    ~DCController();

private:

    QVector<DataLoader*> loaders;

    int filesToLoad {0};

    int filesLoaded {0};

    QVector<double> syncFactors;

    double deltaRealTime {0.0};

    PlotWidget* mainPlot = nullptr;

    MainWindow* window = nullptr;

    void startConverter(const QString& path);

    void onProgress(const QString &fileId, int percent);

    void onFinished(QString fileId, QVector<double> X,
                                  QVector<double> Y, double syncFactor, double delta);

    void onError(QString fileId, QString msg);

    QVector<double> measureData;

    // method for resampling with step newStep
    bool resampleGl1(const QVector<double> &X,
                              const QVector<double> &Y,
                              double newStep,
                              QVector<int> &resX,
                              QVector<double> &resY);


    bool syncFactorIsOK(const double &factor);

    void setupSnapshotManager();  // added

    void applySnapshotToLoaders(const QVector<SnapshotLoadWorker::LoaderInfo> &loadersInfo); 

signals:

    void goPrzConvert(double A, double B);

    void przToPDFinished(const QVector<double> *lenghts);

    void pdToGl1Finished();

    void loaderRemoved(const DataLoader* loader);

    void loaderAdded(const DataLoader* loader);

    void filesLoadFinished();

    void manualLoadAdded();

    void synchronization(bool state);

private slots:

    void loadFiles(const QVector<QString> &paths, bool sync);

    void DetADNLoad();

    void manualADNLoad(const double &lvl);

    void przToPDDone(QVector<double> X, QVector<double> Y, QVector<double> lenghts);

    void PDcreate(double time, double depth, const double &firstPoint, const double &secondPoint);

    void gl1Create(double time,
                   double depth,
                   const double &firstPoint,
                   const double &secondPoint,
                   const QString &direction,
                   const QString &method);

    void pdToGl1Done(QVector<double> X, QVector<double> Y);

    void applyPalette();

    void przConvert();

    void savePDOLFile(const QString &path, const QString &format);

    void initTables(QVector<QCPItemRect*> *rectangles); // slot for initial filling of interval tables

    void deleteInterval(const QString &first, const QString &second);

    void addInterval(const QString &first, const QString &second);

    void intervalsChanged(const QVector<QCPItemRect*> &intervals);

    void shiftGraph(const double &num);

    void cancelShift();

    void openMeasure(const QString &path);

    void clearMeasure();

    void gl1Correction(const bool &candle, const bool &len, double totalLen, 
        const double &refTime, const double &refDepth);

    void pdCorrection(const bool &candle, const bool &len, double totalLen, 
        const double &refTime, const double &refDepth);

    void saveGl1File(const QString &path, int startFrame);

    void cleanAll();

    void getFirstDvlPoint(const double &time1);

    void getSecondDvlPoint(const double &time2);

    void przCreate();

    void przCreated(QVector<double> &X, QVector<double> &Y);

    void graphColorsReseted();

    void performUndo();

    void performRedo();
};

#endif // DCCONTROLLER_H

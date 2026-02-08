#ifndef GL1MANAGER_H
#define GL1MANAGER_H

#include <QObject>
#include "dataloader.h"

/*
 * The Gl1Manager class computes PD/GL1 outputs from PRZ data, derives candle
 * intervals and parameters, and applies correction algorithms to resulting
 * depth curves.
 *
 * Responsibilities:
 * - Convert PRZ data to drill bit position (PD) and depth (GL1)
 * - Derive candle lengths, depth, and speed parameters
 * - Apply candle-based, total-length, and drift corrections
 * - Track movement intervals and emit conversion results
 */
class Gl1Manager : public QObject
{
    Q_OBJECT

public:

    explicit Gl1Manager(QObject *parent = nullptr);

    static Gl1Manager& instance();

    void setLoaders(const QVector<DataLoader *> &list);

    bool detLoad();

    void setWindowWidth(const int &num);

    void przToPD(const QVector<double> *X,
                 const QVector<double> *Y,
                 QVector<QCPItemRect*> &intervals,
                 double time,
                 double depth,
                 const double &firstPoint,
                 const double &secondPoint); // create drill bit position file

    void przToGl1(const QVector<double> *X,
                 const QVector<double> *Y,
                 QVector<QCPItemRect*> &intervals,
                 double time,
                 double depth,
                 const double &firstPoint,
                 const double &secondPoint,
                 const QString &direction,
                 const QString &method); // create depth file from prz

    // create gl1 from drill bit position
    void pdToGl1(const QVector<double> *X,
                 const QVector<double> *Y,
                 const QString &direction,
                 const QString &method);

    // determine candle lengths
    void getLenghts(QVector<double> &lenghts,
                    const QVector<double> *X,
                    const QVector<double> *Y,
                    const QVector<QCPItemRect*> &intervals);

    // determine all parameters
    void getParams(QVector<double> &lenghts,
               QVector<double> &depth,
               QVector<double> &speed,
               const QVector<double> *X,
               const QVector<double> *Y,
               const QVector<QCPItemRect*> &intervals);

    // candle-based correction
    void candleCorrection(DataLoader *loader,
                          QVector<double> &lenghts,
                          QVector<double> &measLengths,
                          const QVector<QCPItemRect*> &intervals,
                          const double &window);

    // total-length correction
    void lengthCorrection(DataLoader *loader,
                          double refTotalLen);
    
    // bottomhole point drift correction
    void leavingCorrection(DataLoader *loader,
        const double &refTime, const double &refDepth);

private:

    QVector<DataLoader *> loaders;

    QVector<double> length;

    QVector<double> errors;

    QVector<double> depth;

    QVector<double> speed;

    QVector<double> measure;

    QVector<QCPItemRect*> moveIntervals;

    int windowWidth;

    bool localPrzToPD(QVector<double> &resX,
                      QVector<double> &resY,
                      QVector<double> &lenghts,
                      const QVector<double> *X,
                      const QVector<double> *Y,
                      QVector<QCPItemRect*> &intervals,
                      double time,
                      double depth,
                      const double &firstPoint,
                      const double &secondPoint);

    bool localPdToGl1(QVector<double> &resX,
                      QVector<double> &resY,
                      const QVector<double> *X,
                      const QVector<double> *Y,
                      const QString &direction,
                      const QString &method);
signals:

    void loadDeterminited(double min, double max);

    void przToPDDone(QVector<double> X, QVector<double> Y, QVector<double> lenghts);

    void pdToGl1Done(QVector<double> X, QVector<double> Y);

    void candleCorrectionDone(DataLoader* loader);

    void lengthCorrectionDone(DataLoader* loader);

};

#endif // GL1MANAGER_H

#ifndef CALIBRATIONMANAGER_H
#define CALIBRATIONMANAGER_H

#include <QObject>
#include "dataloader.h"

/*
 * The CalibrationManager class provides calibration of PRZ and MK data over a
 * selected interval and builds approximation coefficients for speed correction.
 *
 * Responsibilities:
 * - Store DataLoader references used during calibration
 * - Build calibration curves from PRZ angles and MK measurements
 * - Apply median filtering and approximation to produce corrected curves
 * - Compute and expose linear factors A and B
 * - Emit signals with calibration and approximation results
 * - Support PRZ axis inversion and configurable angle ranges
 */
class CalibrationManager : public QObject
{
    Q_OBJECT

public:
    explicit CalibrationManager(QObject *parent = nullptr);

    static CalibrationManager& instance();

    void setLoaders(const QVector<const DataLoader*> &loaders);

    bool calibrate(const double &start, const double &finish);

    void clear();

    void approximate();

    void setRange(const double &min, const double &max);

    void setMkFactor(const double &num);

    void getCalFactors(double &A, double &B);

private:

    bool przInvertState;

    QVector<const DataLoader*> loaders;

    QVector<double> calX;

    QVector<double> calY;

    QVector<double> apprY;

    QVector<double> resY;

    QVector<double> resX;

    double mkLength;

    double mkFactor;

    double minAngle;

    double maxAngle;

    double factorA;

    double factorB;

    void calibDir(QVector<double> &calX, QVector<double> &calY
                  , const QVector<double> &prz, const QVector<double> &mk
                  , const double &m, const bool &dir);

    QVector<double> medianFilter(const QVector<double>& data, int windowSize);

signals:

    void calFinished(QVector<double> x, QVector<double> y);

    void apprFinished(QVector<double> x, QVector<double> y);

public slots:

    void setPrzInvert(int state);

};

#endif // CALIBRATIONMANAGER_H

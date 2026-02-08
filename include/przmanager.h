#ifndef PRZMANAGER_H
#define PRZMANAGER_H

#include <QObject>
#include "dataloader.h"
#include "dcsettings.h"

/*
 * The PrzManager class builds PRZ curves by aligning DVL channels using
 * two reference points and applying filtering for time correction.
 *
 * Responsibilities:
 * - Store DVL loaders and reference shift points
 * - Compute time correction between DVL channels
 * - Apply median and exponential smoothing filters
 * - Emit debug output and PRZ creation results
 */
class PrzManager : public QObject
{
    Q_OBJECT

public:

    PrzManager(const PrzManager&) = delete;

    PrzManager& operator=(const PrzManager&) = delete;

    static PrzManager& instance();

    void setLoaders(const QVector<DataLoader *> &list);

    void przCreate();

    void clear();

    template <typename T>
    int signShift(T val) {
        return (val > 0) ? 1 : -1;
    }


public slots:

    void setFirstPoint(const double &shift1_1, const double &shift2_1, const double &time1);

    void setSecondPoint(const double &shift1_2, const double &shift2_2, const double &time2);

private:

    PrzManager() = default;

    QVector<DataLoader *> loaders;

    double shift1_1 = 0.0; // DVL 1 shift value (point 1)

    double shift1_2 = 0.0; // DVL 1 shift value (point 2)

    double shift2_1 = 0.0;

    double shift2_2 = 0.0;

    double time1 = 0.0;

    double time2 = 0.0;

    DataLoader* dvl1_X = nullptr;

    DataLoader* dvl1_Z = nullptr;

    DataLoader* dvl2_X = nullptr;

    DataLoader* dvl2_Z = nullptr;

    void timeCorrection();

    void medianFilter(QVector<double>& data, int n);

    void expFilter(QVector<double>& data, double alpha);

signals:

    void debug(const QString &text);

    void przCreated(QVector<double> &X, QVector<double> &Y);
};

#endif // PRZMANAGER_H

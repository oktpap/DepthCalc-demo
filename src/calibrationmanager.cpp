#include "calibrationmanager.h"

CalibrationManager::CalibrationManager(QObject *parent)
    : QObject{parent}
    , mkLength(50)
    , mkFactor(0.0488)
    , przInvertState(false)
    , minAngle(0)
    , maxAngle(0)
    , factorA(0)
    , factorB(0)
{}

CalibrationManager &CalibrationManager::instance()
{
    static CalibrationManager obj;
    return obj;
}

void CalibrationManager::setLoaders(const QVector<const DataLoader*> &loaders)
{
    this->loaders = loaders;
    qDebug() << "added" << loaders.size() << "loaders into the CalibrationManager";
}


// method for building the speed plot
bool CalibrationManager::calibrate(const double &start, const double &finish)
{
    if(loaders.isEmpty())
    {
        qDebug() << "loaders не загружены в calibration";
        return false;
    }

    calX.clear();
    calY.clear();
    apprY.clear();
    resX.clear();
    resY.clear();
    minAngle = 0;
    maxAngle = 0;
    factorA = 0;
    factorB = 0;

    QVector<double> time, prz, mk;
    constexpr double pi = 3.14159265358979323846;
    double m = 1.0;

    // if prz is inverted, use a difference with the opposite sign
    if(przInvertState)
        m = -1.0;

    // get prz and mk intervals from DataLoaders
    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i]->getName().contains("prz", Qt::CaseInsensitive))
        {   
            if(!loaders[i]->getDataPart(start, finish, time, prz))
            {
                qWarning() << "Ошибка калибровки: не удалось получить данные.";
                return false;
            }

            this->minAngle = loaders[i]->min();
            this->maxAngle = loaders[i]->max();
        }
        if(loaders[i]->getName().contains("MK", Qt::CaseInsensitive)
           || loaders[i]->getName().contains("KM", Qt::CaseInsensitive))
        {
            if(!loaders[i]->getDataPart(start, finish, time, mk))
            {
                qWarning() << "Ошибка калибровки: не удалось получить данные.";
                return false;
            }
        }
    }

    qDebug() << "COUNT IN CAL" << finish - start + 1;

    double len = 0.0, ampl = 0.0, tempLen = 0.0;
    int refIndex = 0;
    bool dir = true;

    /*
     * Determine descent/ascent
     * If ascent, traverse arrays forward
     * If descent, traverse arrays backward
     */
    if(m * (prz[0] - prz[prz.size() - 1]) > 0)
    {
        dir = false;
        refIndex = prz.size() - 1;
    }

    int st = dir ? 0 : prz.size() - 1;
    int en = dir ? prz.size() : -1;
    int step = dir ? 1 : -1;

    if(dir == false)
        en = 0;
    else en = qMin(prz.size(), mk.size());

    // iterate points from the specified interval
    for (int i = st; i != en; i += step)
    {
        if(abs(prz[i] - prz[refIndex]) * 180 / pi >= 18)
        {

            tempLen = len + m * (prz[i] - prz[refIndex]) / 2 / pi;
            ampl = mkFactor * 2 * pi * (mk[i] - mk[refIndex]) / m / (prz[i] - prz[refIndex]);

            len = tempLen;
            calX.append(len);
            calY.append(ampl);
            refIndex = i;
        }
    }

    qDebug() << "cal ended";

    calY = medianFilter(calY,7);

    // completion signal (used to build the plot)
    emit calFinished(calX, calY);

    return true;
}

void CalibrationManager::clear()
{
    loaders.clear();
    calX.clear();
    calY.clear();
    apprY.clear();
    resX.clear();
    resY.clear();
    minAngle = 0;
    maxAngle = 0;
    factorA = 0;
    factorB = 0;
}

// method for approximating the relative speed plot
void CalibrationManager::approximate()
{
    double a, b, multSum = 0.0, xSum = 0.0, ySum = 0.0, sqrXSum = 0.0, N, m = 1.0;
    constexpr double pi = 3.14159265358979323846;
    int n = calX.size();

    if(n == 0 || minAngle == maxAngle) return;

    // compute formula components
    for(int i = 0; i < n; i++)
    {
        ySum += calY[i];
        xSum += calX[i];
        multSum += calX[i] * calY[i];
        sqrXSum += calX[i] * calX[i];
    }

    // compute coefficients
    if((n * sqrXSum - xSum * xSum) != 0 && n != 0)
    {
        a = (n * multSum - xSum * ySum) / (n * sqrXSum - xSum * xSum);
        b = (ySum - a * xSum) / n;
    }
    else return;

    qDebug() << "coefficient A:" << a << "coefficient B:" << b;

    // build the approximated line plot
    for(int i = 0; i < n; i++)
    {
        apprY.append(a * calX[i] + b);
    }

    // signal for rendering the line plot
    emit calFinished(calX, apprY);

    qDebug() << "MIN ANGLE:" << minAngle << "MAX ANGLE" << maxAngle;

    /*
     * Build the palette (integrate the curve equation)
     * Displacement equation: S(t) = (aN^2)/2 + bN,
     * where N is the number of winch revolutions
     */
    for(double ang = minAngle; ang <= maxAngle; ang += 1.0)
    {
        N = ang / 2.0 / pi;
        resX.append(N);
        resY.append((a * N * N / 2.0 + b * N) / 100.0);
    }

    // coefficients for calibrating the prz plot
    // (so we can multiply directly without converting to revolutions)
    factorA = a / 2.0 / pi / 2.0 / pi;
    factorB = b / 2.0 / pi;

    if(przInvertState)
    {
        factorA *= -1.0;
        factorB *= -1.0;
    }

    double ma = minAngle / 2.0 / pi;
    double maxa = maxAngle / 2.0 / pi;

    qDebug() << "DELTA METERS" << (ma * ma * a / 2.0 + ma * b - maxa * maxa * a / 2.0 - maxa * b) / 100.0;

    // signal for rendering the palette plot
    emit apprFinished(resX, resY);
}

void CalibrationManager::setRange(const double &min, const double &max)
{
    this->minAngle = min;
    this->maxAngle = max;
}

void CalibrationManager::setMkFactor(const double &num)
{
    this->mkFactor = num;
}

void CalibrationManager::getCalFactors(double &A, double &B)
{
    A = factorA;
    B = factorB;
}

void CalibrationManager::calibDir(QVector<double> &calX, QVector<double> &calY, const QVector<double> &prz
                                  , const QVector<double> &mk, const double &m, const bool &dir)
{
    double len = 0.0, ampl = 0.0, tempLen = 0.0;
    int refIndex = 0;


}

QVector<double> CalibrationManager::medianFilter(const QVector<double> &data, int windowSize)
{
    if (data.isEmpty() || windowSize < 1)
        return data;

    if (windowSize % 2 == 0)
        windowSize++; // окно должно быть нечётным

    int radius = windowSize / 2;
    QVector<double> result(data.size());

    for (int i = 0; i < data.size(); ++i)
    {
        QVector<double> window;
        for (int j = -radius; j <= radius; ++j)
        {
            int idx = i + j;
            if (idx < 0) idx = 0;
            if (idx >= data.size()) idx = data.size() - 1;
            window.append(data[idx]);
        }

        std::sort(window.begin(), window.end());
        result[i] = window[radius];
    }

    return result;
}

void CalibrationManager::setPrzInvert(int state)
{
    if(state == 0)
        this->przInvertState = false;
    else
        this->przInvertState = true;
}



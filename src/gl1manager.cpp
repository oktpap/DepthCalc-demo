#include "gl1manager.h"
#include <algorithm>


Gl1Manager::Gl1Manager(QObject *parent)
    : QObject{parent}
    , windowWidth(10)
{}

// get singleton reference
Gl1Manager &Gl1Manager::instance()
{
    static Gl1Manager obj;
    return obj;
}

void Gl1Manager::setLoaders(const QVector<DataLoader *> &list)
{
    this->loaders = list;
}

// method for automatic load limit detection
// (not used)
bool Gl1Manager::detLoad()
{
    if (loaders.isEmpty()) return false;

    const QVector<double> *przX = nullptr, *adnX = nullptr, *przY = nullptr, *adnY = nullptr;
    int countUp = 0, countDown = 0;
    int przIndex = -1, adnIndex = -1, a, p;
    double up = 0, down = 0;

    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i]->getName().contains("prz", Qt::CaseInsensitive))
        {
            przX = &loaders[i]->getX();
            przY = &loaders[i]->getY();
        }

        else if (loaders[i]->getName().contains("DN", Qt::CaseInsensitive))
        {
            adnX = &loaders[i]->getX();
            adnY = &loaders[i]->getY();
        }
    }

    if(przX == nullptr || adnX == nullptr) return false;

    // determine the larger time of the first point
    // (to choose the start point)
    if((*przX)[0] > (*adnX)[0])
    {
        przIndex = 0;
        adnIndex = (*przX)[0] - (*adnX)[0];
    }

    else
    {
        adnIndex = 0;
        przIndex = (*adnX)[0] - (*przX)[0];
    }

    // check window size
    if(windowWidth <= 0)
        windowWidth = 2;

    for (int i = windowWidth / 2; i < przY->size() && i < adnY->size(); i++)
    {
        a = i + adnIndex;
        p = i + przIndex;

        // average DN on up/down runs (sum accumulation)
        if((*przY)[p] != (*przY)[p - 1])
        {
            double sum = 0;

            for(int j = a - windowWidth / 2; j <= a + windowWidth / 2 && j < adnY->size(); j++)
            {
                sum += (*adnY)[j];
            }

            if((*przY)[p] > (*przY)[p - 1])
            {
                up += sum / (windowWidth + 1);
                countUp++;
            }
            else if((*przY)[p] < (*przY)[p - 1])
            {
                down += sum / (windowWidth + 1);
                countDown++;
            }
        }
    }

    double min, max;

    if(countUp != 0 && countDown != 0)
    {
        min = down / countDown;
        max = up / countUp;
    }

    else return false;

    emit loadDeterminited(min, max);
    qDebug() << "MIN MAX LOAD:" << min << max;

    return true;
}

void Gl1Manager::setWindowWidth(const int &num)
{
    this->windowWidth = num;
}


// method for trimming idle runs
void Gl1Manager::przToPD(const QVector<double> *X,
                         const QVector<double> *Y,
                         QVector<QCPItemRect*> &intervals,
                         double time,
                         double depth,
                         const double &firstPoint,
                         const double &secondPoint)
{
    QVector<double> resY, resX, lenghts;

    this->moveIntervals = intervals;

    if(localPrzToPD(resX, resY, lenghts, X, Y, intervals, time, depth, firstPoint, secondPoint))
    {
        qDebug() << "GL1: przToPDDone";
        // completion signal
        // connected to DCController::przToPDDone()
        emit przToPDDone(resX, resY, lenghts);
    }

    else
    {
        qDebug() << "Failed to generate bit position file";
    }
}

void Gl1Manager::przToGl1(const QVector<double> *X,
                          const QVector<double> *Y,
                          QVector<QCPItemRect*> &intervals,
                          double time,
                          double depth,
                          const double &firstPoint,
                          const double &secondPoint,
                          const QString &direction,
                          const QString &method)
{
    QVector<double> pdY, pdX, resY, resX, lenghts;

    this->moveIntervals = intervals;

    if(!localPrzToPD(pdX, pdY, lenghts, X, Y, intervals, time, depth, firstPoint, secondPoint))
    {
        qDebug() << "Failed to generate bit position data (gl1)";
        return;
    }

    if(localPdToGl1(resX, resY, &pdX, &pdY, direction, method))
    {
        emit pdToGl1Done(resX, resY);
    }
    else
    {
        qDebug() << "Failed to generate depth file";
    }

    qDebug() << "przToGl1 finish";
}

void Gl1Manager::pdToGl1(const QVector<double> *X, const QVector<double> *Y, const QString &direction, const QString &method)
{
    QVector<double> resY, resX;

    if(localPdToGl1(resX, resY, X, Y, direction, method))
    {
        // connected to DCController::pdToGl1Done
        emit pdToGl1Done(resX, resY);
    }

    else
    {
        qDebug() << "Failed to generate depth file";
    }
}

void Gl1Manager::getLenghts(QVector<double> &lenghts,
                            const QVector<double> *X,
                            const QVector<double> *Y,
                            const QVector<QCPItemRect *> &intervals)
{
    lenghts.clear();
    this->length.clear();

    this->moveIntervals = intervals;

    for(int i = 0; i < intervals.size(); i++)
    {
        int k = 1;
        double st = intervals[i]->topLeft->key();      // left X coordinate
        double fn = intervals[i]->bottomRight->key();  // right X coordinate

        while(k < X->size())
        {
            if((*X)[k] < st)
            {
                k++;
                continue;
            }

            else if((*X)[k] >= st)
            {
                double depth1 = (*Y)[k];

                while(k < (*X).size() && k < Y->size() && (*X)[k] <= fn)
                {
                    k++;
                }

                lenghts.append(abs((*Y)[k] - depth1) * 100);
                length.append(abs((*Y)[k] - depth1) * 100);

                break;
            }
        }
    }
}

void Gl1Manager::getParams(QVector<double> &lenghts,
                           QVector<double> &depth,
                           QVector<double> &speed,
                           const QVector<double> *X,
                           const QVector<double> *Y,
                           const QVector<QCPItemRect *> &intervals)
{
    lenghts.clear(); depth.clear(); speed.clear();
    this->length.clear(); this->errors.clear();
    this->depth.clear();  this->speed.clear();
    this->moveIntervals = intervals;

    if (!X || !Y || X->isEmpty() || Y->isEmpty()) return;

    for (int i = 0; i < intervals.size(); ++i) {
        const double st = intervals[i]->topLeft->key();
        const double fn = intervals[i]->bottomRight->key();
        if (st > fn) continue;

        // indices covering the interval [st, fn] on the X axis
        auto itL = std::lower_bound(X->begin(), X->end(), st);
        auto itR = std::upper_bound(X->begin(), X->end(), fn);

        // if there are no points in the interval
        if (itL == X->end() || itL == itR)
        {

            depth.append(0);
            this->depth.append(0);

            lenghts.append(0);
            this->length.append(0);

            speed.append(0);
            this->speed.append(0);

            continue;
        }

        const int startIdx = int(itL - X->begin());
        const int endIdx   = int(itR - X->begin()) - 1;
        if (endIdx < startIdx) continue;

        const double depth1 = (*Y)[startIdx];
        const double depth2 = (*Y)[endIdx];

        const double dDepth_cm = std::abs(depth2 - depth1) * 100.0;
        const double v = (fn > st) ? (std::abs(depth2 - depth1) / (fn - st) * 3600.0) : 0.0;

        depth.append(depth2);
        this->depth.append(depth2);

        lenghts.append(dDepth_cm);
        this->length.append(dDepth_cm);

        speed.append(v);
        this->speed.append(v);
    }
}

void Gl1Manager::candleCorrection(DataLoader *loader,
                                  QVector<double> &lenghts,
                                  QVector<double> &measLengths,
                                  const QVector<QCPItemRect*> &intervals,
                                  const double &window)
{
    if(!loader->getName().contains("gl1", Qt::CaseInsensitive)
       && !loader->getName().contains("PDOL", Qt::CaseInsensitive)) return;
    if(window < 0) return;

    const QVector<double> X = loader->getX(), Y = loader->getY();
    QVector<double> coefficients, resCoef, resY = Y;

    if(X.isEmpty() || Y.isEmpty()) return;

    for(int i = 0; i < measLengths.size() && i < lenghts.size(); i++)
    {
        coefficients.append(measLengths[i] / lenghts[i]);
    }

    // median filter
    resCoef = coefficients;

    int N = coefficients.size();

    for (int i = 0; i < N; ++i)
    {
        int left  = std::max(0, static_cast<int>(i - window));
        int right = std::min(N - 1, static_cast<int>(i + window));

        QVector<double> winValues;
        winValues.reserve(right - left + 1);

        for (int j = left; j <= right; ++j)
            winValues.append(coefficients[j]);

        std::sort(winValues.begin(), winValues.end());
        resCoef[i] = winValues[winValues.size() / 2];
    }

    //     for(int i = window; i < coefficients.size() - window; i++)
    // {
    //     QVector<double> winValues;

    //     for(int j = -window; j <= window; j++)
    //     {
    //         winValues.append(coefficients[i + j]);
    //     }

    //     std::sort(winValues.begin(), winValues.end());
    //     resCoef[i] = winValues[winValues.size() / 2];
    // }


    if (resCoef.isEmpty()) return;
    int k = 0;
    double delta = 0.0;

    for(int i = 0; i < intervals.size() && i < resCoef.size(); i++)
    {        
        double fn = intervals[i]->bottomRight->key();
        double refY = Y[k] + delta;
        double corCoef = resCoef[i];

        while(k < X.size() && X[k] <= fn)
        {
            double currentY = Y[k] + delta;
            double newY = refY + (currentY - refY) * corCoef;

            resY[k] = newY;
            k++;
        }

        delta += (corCoef - 1) * (Y[k - 1] + delta - refY);
    }

    loader->setYData(resY);

    emit candleCorrectionDone(loader);
}

void Gl1Manager::lengthCorrection(DataLoader *loader, double refTotalLen)
{
    if(!loader->getName().contains("gl1", Qt::CaseInsensitive)
        && !loader->getName().contains("PDOL", Qt::CaseInsensitive)) return;

    QVector<double> resY;
    double totalLen, corCoef;

    const QVector<double> X = loader->getX();
    const QVector<double> Y = loader->getY();


    for(int k = Y.size() / 2 ; k < Y.size() / 2 + 10 && k < Y.size(); k++)
    {
        qDebug() << "old in lenCor:" << QString::number(Y[k], 'f', 9);;
    }

    totalLen = abs(Y.back() - Y[0]) * 100; // total length from the sensor
    if(totalLen != 0)
        corCoef = refTotalLen / totalLen;
    else return;

    resY = Y;

    double startPoint = resY.front();

    for (int i = 1; i < Y.size() && i < X.size(); i++)
    {
        double currentY = resY[i];
        double newY = startPoint + (currentY - startPoint) * corCoef;

        resY[i] = newY;
    }

    for(int k = resY.size() / 2 ; k < resY.size() / 2 + 10 && k < resY.size(); k++)
    {
        qDebug() << "new in lenCor:" << QString::number(resY[k], 'f', 9);
    }

    loader->setYData(resY);

    qDebug() << "LOADER" << loader->getName() << "LEN CORRECTED";
    qDebug() << "LEN COR COEFFICIENT:" << corCoef;

    emit lengthCorrectionDone(loader);
}


bool Gl1Manager::localPrzToPD(QVector<double> &resX,
                              QVector<double> &resY,
                              QVector<double> &lenghts,
                              const QVector<double> *X,
                              const QVector<double> *Y,
                              QVector<QCPItemRect *> &intervals,
                              double time,
                              double depth,
                              const double &firstPoint,
                              const double &secondPoint)
{
    // intervals - array of rectangles showing DN load ranges
    if(intervals.isEmpty()) return false;

    this->moveIntervals = intervals;

    double len = 0.0, refDepth = 0.0, start = 0, finish = 0, st, fn;
    double max = intervals.back()->bottomRight->key();

    // set start and end points (may be swapped)
    start = firstPoint < secondPoint ? firstPoint : secondPoint;
    finish = firstPoint < secondPoint ? secondPoint : firstPoint;

    if(finish > max)
        finish = max;
    else
    {
        // trim the extra right part
        for(int i = 0; i < intervals.size(); i++)
        {
            st = intervals[i]->topLeft->key();
            fn = intervals[i]->bottomRight->key();

            if(fn < secondPoint) continue;

            else if(st > secondPoint)
            {
                if(secondPoint < intervals[i - 1]->bottomRight->key())
                    break;
                finish = intervals[i - 1]->bottomRight->key() - 1;
                break;
            }

            else break;
        }
    }

    if(time  < (*X)[0] || time > (*X).back())
        return false;

    resX.clear();
    resY.clear();
    lenghts.clear();

    // check for out-of-range bounds
    start = qMax(start, (*X)[0]);
    finish = qMin(finish, (*X)[X->size() - 1]);

    if(start == finish)
    {
        start = (*X)[0];
        finish = (*X)[(*X).size() - 1];
    }

    // get indices
    int startInd = static_cast<int>(
        std::lower_bound(X->begin(), X->end(), start) - X->begin());

    int finInd = static_cast<int>(
        std::upper_bound(X->begin(), X->end(), finish) - X->begin());

    int k = 1;
    int startIntervalInd = 0, endIntervalInd = 0;

    // iterate all intervals
    for(int i = 0; i < intervals.size(); i++)
    {
        double st = intervals[i]->topLeft->key();      // left X coordinate
        double fn = intervals[i]->bottomRight->key();  // right X coordinate

        while(k < X->size() - 1)
        {
            if((*X)[k] < st)
            {
                resY.append(len);
                resX.append((*X)[k]);
                k++;
                continue;
            }

            else if((*X)[k] >= st)
            {
                if(startIntervalInd == 0)
                    startIntervalInd = i;
                endIntervalInd = i;

                double depth1 = len;

                while(k < (*X).size() && k < Y->size() && (*X)[k] <= fn)
                {
                    len += -((*Y)[k] - (*Y)[k - 1]);
                    resY.append(len);
                    resX.append((*X)[k]);
                    k++;
                }

                lenghts.append(abs(len - depth1) * 100);

                break;
            }
        }
    }

    int refDepthInd = static_cast<int>(std::lower_bound(resX.begin(), resX.end(), time) - resX.begin());

    if(!(*X).isEmpty() && refDepthInd < resY.size())
        refDepth = depth / 100 - resY[refDepthInd];
    else
    {
        resX = resX.mid(startInd, finInd - startInd);
        resY = resY.mid(startInd, finInd - startInd);

        intervals = intervals.mid(startIntervalInd, endIntervalInd - startIntervalInd + 1);
        return true;
    }

    resX = resX.mid(startInd, finInd - startInd);
    resY = resY.mid(startInd, finInd - startInd);
    intervals = intervals.mid(startIntervalInd, endIntervalInd - startIntervalInd + 1);

    for(int i = 0; i < resY.size(); i++)
        resY[i] += refDepth;

    return true;
}

bool Gl1Manager::localPdToGl1(QVector<double> &resX,
                              QVector<double> &resY,
                              const QVector<double> *X,
                              const QVector<double> *Y,
                              const QString &direction,
                              const QString &method)
{
    if ((*X).isEmpty() || (*Y).isEmpty()) return false;

    resX.clear();
    resY.clear();

    int step = 1, startIndex = 0, finishIndex = (*Y).size() - 1, resInd;
    double delta = 0.0;

    if(direction == "Descent")
    {
        if(method == "From top")
        {
            step = 1;
            startIndex = 0;
            finishIndex = (*Y).size() - 1;
        }
        else if(method == "From bottom")
        {
            step = -1;
            startIndex = (*Y).size() - 1;
            finishIndex = 0;
        }
    }

    else if (direction == "Ascent")
    {
        if(method == "From top")
        {
            step = -1;
            startIndex = (*Y).size() - 1;
            finishIndex = 0;
        }
        else if(method == "From bottom")
        {
            step = 1;
            startIndex = 0;
            finishIndex = (*Y).size() - 1;
        }
    }

    else if(direction == "Auto")
    {
        if((*Y)[(*Y).size() - 1] - (*Y)[0] > 0)
        {
            if(method == "From top")
            {
                step = -1;
                startIndex = (*Y).size() - 1;
                finishIndex = 0;
            }
            else if(method == "From bottom")
            {
                step = 1;
                startIndex = 0;
                finishIndex = (*Y).size() - 1;
            }
        }

        else
        {
            if(method == "From top")
            {
                step = 1;
                startIndex = 0;
                finishIndex = (*Y).size() - 1;
            }
            else if(method == "From bottom")
            {
                step = -1;
                startIndex = (*Y).size() - 1;
                finishIndex = 0;
            }
        }
    }

    resY.append((*Y)[startIndex]);
    resX.append((*X)[startIndex]);

    for(int i = startIndex; i != finishIndex; i += step)
    {
        resInd = resY.size() - 1;
        delta = resY[resInd] - (*Y)[i + step];

        if(method == "From top")
        {
            if(delta < 0)
                resY.append(resY[resInd]);
            else
                resY.append((*Y)[i + step]);

            resX.append((*X)[i + step]);
        }

        else if(method == "From bottom")
        {
            if(delta > 0)
                resY.append(resY[resInd]);
            else
                resY.append((*Y)[i + step]);

            resX.append((*X)[i + step]);
        }
    }

    if(step < 0)
    {
        std::reverse(resX.begin(), resX.end());
        std::reverse(resY.begin(), resY.end());
    }

    return true;
}


void Gl1Manager::leavingCorrection(DataLoader *loader,
        const double &refTime, const double &refDepth)
{
    if(!loader->getName().contains("gl1", Qt::CaseInsensitive)
        && !loader->getName().contains("PDOL", Qt::CaseInsensitive)) 
        return;

    const QVector<double> X = loader->getX();
    const QVector<double> Y = loader->getY();

    if (X.isEmpty() || Y.isEmpty()) 
        return;

    QVector<double> resY = Y;
    int refDepthInd = 0;

    if (refTime > 0.0)
    {
        auto it = std::lower_bound(X.begin(), X.end(), refTime);
        
        if (it != X.end())
            refDepthInd = static_cast<int>(it - X.begin());

        else
            refDepthInd = X.size() - 1;
    }

    if (refDepthInd >= Y.size())
        refDepthInd = Y.size() - 1;

    double delta = refDepth - Y[refDepthInd];

    for (int i = 0; i < resY.size(); i++)
        resY[i] += delta;

    loader->setYData(resY);

    emit lengthCorrectionDone(loader);
}











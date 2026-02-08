#include "dataloader.h"
#include <cmath>
#include "dcsettings.h"

DataLoader::DataLoader(QVector<double> &masX, QVector<double> &masY, const QString &name) :
    maxSize(4000)
    , syncState(false)
{
    this->X = masX;
    this->Y = masY;
    this->name = name;

    if(name.contains("PDOL"))
        maxSize = 2000;
}

QString DataLoader::FilePath() const
{
    return this->path;
}

const QVector<double> &DataLoader::getX() const
{
    return X;
}

const QVector<double> &DataLoader::getY() const
{
    return Y;
}

void DataLoader::setPath(const QString &filePath)
{
    this->path = filePath;
    QFileInfo info(filePath);
    this->name = info.fileName();
}

double DataLoader::getStartX() const
{
    if(X.isEmpty()) return 0;

    return X[0];
}

double DataLoader::getFinishX() const
{
    if(X.isEmpty()) return 0;

    return X[X.size() - 1];
}

void DataLoader::shiftX(const double &num)
{
    shiftAmount += num;

    for (int i = 0; i < X.size(); i++)
    {
        X[i] += num;
    }

    // shift signal
    // connected to PlotWidget::dataShifted()
    emit xShifted();
}

double DataLoader::getShiftAmount()
{
    return shiftAmount;
}

void DataLoader::setYData(QVector<double> &yData)
{
    qDebug() << "setYData old:" << Y.back() - Y.front() << "new:" << yData.back() - yData.front();

    for(int k = yData.size() / 2 ; k < yData.size() / 2 + 10 && k < yData.size(); k++)
    {
        qDebug() << "old:" << Y[k] << "new:" << yData[k];
    }

    this->Y = yData; 
}

double DataLoader::yRange()
{
    return Y.back() - Y.front();
}

double DataLoader::totalLen() const
{
    return abs(Y.back() - Y.front());
}

void DataLoader::crop(const double &start, const double &finish)
{
    auto itMin = std::lower_bound(X.begin(), X.end(), start);
    auto itMax = std::upper_bound(X.begin(), X.end(), finish);

    int startInd = int(itMin - X.begin());
    int endInd   = int(itMax - X.begin());

    if (startInd >= endInd) return;

    X = X.mid(startInd, endInd - startInd);
    Y = Y.mid(startInd, endInd - startInd);
}

int DataLoader::size() const
{
    return X.size();
}

void DataLoader::stretchFromMiddle(const double &factor)
{
    if(X.size() == 0 || Y.size() == 0 || factor == 1.0) return;

    qDebug() << "X.size():" << X.size();

    int middle = X.size() / 2, k = 0;;
    double dx = X[1] - X[0], x, y;

    QVector<double> newX, newY;

    newX.reserve(std::round(X.size() * factor));
    newY.reserve(std::round(Y.size() * factor));

    if(X.size() % 2 != 0)
    {
        newX.append(X[middle]);
        newY.append(Y[middle]);
    }

    else if(X.size() % 2 == 0)
    {
        newX.append(X[middle - 1]);
        newY.append(Y[middle - 1]);

        k = -1;
    }

    for(int i = middle - 1 + k; i >= 0; i--)
    {
        x = X[middle] - (X[middle] - X[i]) * factor;
        x = std::round(x / dx) * dx;

        if(x == newX.back()) continue;

        if (x != newX[newX.size() - 1] - dx)
        {
            while (newX.back() - x > dx)
            {
                double x_ins = newX.back() - dx;

                y = newY.back() + (Y[i] - newY.back()) * ((x_ins - newX.back()) / (x - newX.back()));

                newX.append(x_ins);
                newY.append(y);
            }
        }

        if (x != newX.back())
        {
            newX.append(x);
            newY.append(Y[i]);
        }
    }

    std::reverse(newX.begin(), newX.end());
    std::reverse(newY.begin(), newY.end());

    if(X.size() % 2 == 0)
    {
        newX.append(X[middle]);
        newY.append(Y[middle]);
    }

    for (int i = middle + 1; i < X.size(); ++i)
    {
        double x = X[middle] + (X[i] - X[middle]) * factor;
        x = std::round(x / dx) * dx;

        if (x == newX.back()) continue;

        if (x != newX.back() + dx)
        {

            while (x - newX.back() > dx)
            {
                    double x_ins = newX.back() + dx;
                    y = newY.back() + (newY.back() - Y[i]) * ((x_ins - newX.back()) / (newX.back() - x));

                    newX.append(x_ins);
                    newY.append(y);
            }
        }

        if (x != newX.back())
        {
            newX.append(x);
            newY.append(Y[i]);
        }
    }

    X = std::move(newX);
    Y = std::move(newY);

    qDebug() << "newX.size():" << X.size();

    emit xShifted();
}

void DataLoader::saveShift()
{
    this->shiftAmount = 0;
}

void DataLoader::chop(int k)
{
    if (X.isEmpty() || k > X.size() || k < 1)
        return;

    X.resize(X.size() - k);
    Y.resize(Y.size() - k);

    emit xShifted();
}

void DataLoader::resample(const double &dx)
{
    QVector<double> newX, newY;
    double x, y;

    if(X.size() < 2 || !(X.size() == Y.size()) || dx <= 0) return;

    double start  = std::ceil(X.front() / dx) * dx;
    double finish = std::floor(X.back() / dx) * dx;
    int length = std::floor((finish - start) / dx) + 1;

    newX.reserve(length);
    newY.reserve(length);

    int k = 0, j = 0;

    while(start + k * dx <= finish)
    {
        x = start + k * dx;

        while(j < X.size() - 1 && x > X[j + 1])
            j++;

        if(j + 1 > X.size() - 1) break;

        if(x >= X[j] && x <= X[j + 1] && (X[j + 1] - X[j]) != 0)
            y = Y[j] + (x - X[j]) / (X[j + 1] - X[j]) * (Y[j + 1] - Y[j]);
        else
            break;

        newX.append(x);
        newY.append(y);

        k++;
    }

    X.swap(newX);
    Y.swap(newY);
}

void DataLoader::update(double min, double max, QVector<double> &vis_x, QVector<double> &vis_y) const
{
    vis_x.clear(); // очистка содержимого
    vis_y.clear();

    if (X.size() == 0) return; // выход, если данные отсутствуют

    constexpr double eps = 0.1;

    auto itMin = std::lower_bound(X.begin(), X.end(), min - eps);
    auto itMax = std::upper_bound(X.begin(), X.end(), max + eps);

    int startInd = int(itMin - X.begin());
    int endInd = int(itMax - X.begin()) - 1;

    if (startInd > endInd) return;

    // если окно вышло за пределы графика
    if (startInd > X.size()  - 1 || endInd < 0)
    {
        return;
    } // не обновляем данные

    // добавляем буфер (длина выборки)
    int buffer = int(endInd - startInd) + 1; // +1 для того, чтобы не ломалось при приближении
    startInd = startInd - buffer >= 0 ? startInd - buffer : 0;
    endInd = endInd + buffer <= X.size() - 1 ? endInd + buffer : X.size() - 1;
    int realSize = endInd - startInd + 1;

    if (realSize <= maxSize)
    {
        vis_x.reserve(realSize);
        vis_y.reserve(realSize);

        for (int i = startInd; i <= endInd; i++) // заполение массивов нужными точками
        {
            vis_x.append(X[i]);
            vis_y.append(Y[i]);
        }
    }
    else
    {
        vis_x.reserve(maxSize);
        vis_y.reserve(maxSize);

        // шаг для выбора точек
        double step = (realSize - 1) / double(maxSize - 1);

        for(int j = 0; j < maxSize; j++)
        {
            int i = int(floor(j * step)) + startInd;
            vis_x.append(X[i]);
            vis_y.append(Y[i]);
        }
    }
}

double DataLoader::min() const
{
    double min = *std::min_element(Y.begin(), Y.end());
    return min;
}

double DataLoader::max() const
{
    double max = *std::max_element(Y.begin(), Y.end());
    return max;
}

double DataLoader::min(const double &start, const double &finish) const
{ 
    double realStart = qMax(start, X.front());
    double realFinish = qMin(finish, X.back());

    if(start > X.back() || finish < X.front())
    {
        realStart = X.front();
        realFinish = X.back();
    }

    auto st = std::lower_bound(X.begin(), X.end(), realStart);
    auto fn = std::lower_bound(X.begin(), X.end(), realFinish);

    int startInd = int(st - X.begin());
    int endInd = int(fn - X.begin());

    auto minIt = std::min_element(Y.begin() + startInd,
                                  Y.begin() + endInd);

    return *minIt;
}


double DataLoader::max(const double &start, const double &finish) const
{
    double realStart = qMax(start, X.front());
    double realFinish = qMin(finish, X.back());

    auto st = std::lower_bound(X.begin(), X.end(), realStart);
    auto fn = std::lower_bound(X.begin(), X.end(), realFinish);

    int startInd = int(st - X.begin());
    int endInd = int(fn - X.begin());

    auto maxIt = std::max_element(Y.begin() + startInd,
                                  Y.begin() + endInd);
    return *maxIt;
}


double DataLoader::range() const
{
    double maxElem = *std::max_element(Y.begin(), Y.end());
    double minElem = *std::min_element(Y.begin(), Y.end());

    return maxElem - minElem;
}

double DataLoader::range(const double &start, const double &finish) const
{
    double maxElem = max(start + X.front(), finish + X.front());
    double minElem = min(start + X.front(), finish + X.front());

    return maxElem - minElem;
}

void DataLoader::get_start_point(double &start_x, double &start_y) const // геттер начальной точки
{
    start_x = X[0];
    start_y = Y[0];
}

void DataLoader::get_start_point(double &start_y) const // геттер начальной точки
{
    start_y = Y[0];
}

void DataLoader::getTime(double &start, double &end) const
{
    start = startTime;
    end = endTime;
}

void DataLoader::setStart(double &time)
{
    for (int i = 0; i < X.size(); i++)
        X[i] += time;
}

void DataLoader::getXRange(double &start, double &end) const
{
    start = X[0];
    end = X[X.size() - 1];
}

void DataLoader::setTime(double &start, double &end, double &rStart, double &rEnd)
{
    startTime = start;
    endTime = end;
    startRef = rStart;
    endRef = rEnd;
}

void DataLoader::setSyncPoints(double start, double finish, double startRef, double finishRef)
{
    this->startTime = start;
    this->endTime = finish;
    this->startRef = startRef;
    this->endRef = finishRef;
}

void DataLoader::setDeltaTime(double const &delta)
{
    for(int i = 0; i < X.size(); i++)
        X[i] += delta;
}

double DataLoader::deltaTime() const
{
    return startTime - startRef;
}


// слот для изменения состояния синхронизации времени
void DataLoader::timeSync(const double &factor)
{
    QVector<double> newX, newY;
    double x, y, dx;

    if(X.isEmpty()) return;

    dx = 0.008;

    for(int i = 0; i < X.size(); i++)
    {
        X[i] = X.front() + (X[i] - X.front()) * factor;
    }

    double start  = std::ceil(X.front() / dx) * dx;
    double finish = std::floor(X.back() / dx) * dx;
    double length = std::floor((finish - start) / dx) + 1;

    newX.reserve(length);
    newY.reserve(length);

    int k = 0, j = 0;

    // передискретизация с шагом dx
    while(start + k * dx <= finish)
    {
        x = start + k * dx;

        while(x > X[j + 1] && j < X.size() - 1)
            j++;

        if(j + 1 > X.size() - 1) break;

        if(x >= X[j] && x <= X[j + 1])
            y = Y[j] + (x - X[j]) / (X[j + 1] - X[j]) * (Y[j + 1] - Y[j]);
        else
            break;

        newX.append(x);
        newY.append(y);

        k++;
    }

    X.swap(newX);
    Y.swap(newY);

    qDebug() << "newX.size():" << X.size();
    qDebug() << "SYNCFACTOR" << name << ":" << factor;

    syncState = true;
}


QString DataLoader::getName() const
{
    return name;
}


bool DataLoader::getSyncState()
{
    return syncState;
}



bool DataLoader::getDataPart(const double &start, const double &finish,
                             QVector<double> &xMas, QVector<double> &yMas) const
{
    if(start < X[0] || finish > X[X.size() - 1])
        return false;

    auto startIt  = std::lower_bound(X.begin(), X.end(), start);
    auto finishIt = std::upper_bound(X.begin(), X.end(), finish);

    if (startIt == X.end() || finishIt == X.begin())
        return false;

    int startIndex  = static_cast<int>(std::distance(X.begin(), startIt));
    int finishIndex = static_cast<int>(std::distance(X.begin(), finishIt));

    if (finishIndex <= startIndex)
        return false;

    xMas.clear();
    yMas.clear();

    for(int i = startIndex; i <= finishIndex; i++)
    {
        xMas.append(X[i]);
        yMas.append(Y[i]);
    }

    if(!xMas.isEmpty() && !yMas.isEmpty())
    {
        return true;
    }
    else
        return false;
}

bool DataLoader::getDataPart(const double &start, const double &finish, QVector<double> &yMas) const
{
    int startIndex = X.indexOf(start);
    int finishIndex = X.indexOf(finish);

    if(startIndex == -1 || finishIndex == -1 || startIndex >= finishIndex)
    {
        qWarning() << "Ошибка получения части массива:" << name;
        return false;
    }

    yMas.clear();

    for(int i = startIndex; i <= finishIndex; i++)
    {
        yMas.append(Y[i]);
    }

    if(!yMas.isEmpty())
    {
        return true;
    }
    else
        return false;
}


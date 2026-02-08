#include "przmanager.h"
#include <cmath>
#include <numbers>


PrzManager &PrzManager::instance()
{
    static PrzManager obj;
    return obj;
}

void PrzManager::setLoaders(const QVector<DataLoader *> &list)
{
    this->loaders = list;
}

void PrzManager::setFirstPoint(const double &shift1_1,
                               const double &shift2_1,
                               const double &time1)
{
    this->shift1_1 = shift1_1;
    this->shift2_1 = shift2_1;
    this->time1 = time1;

    qDebug() << "shift1_1:" << shift1_1 << "shift2_1:" << shift2_1 << "time1:" << time1;

    emit debug("Point 1. Shift dvl1: " + QString::number(shift1_1) + " s; Shift dvl2: "
               + QString::number(shift2_1) + " s");
}

void PrzManager::setSecondPoint(const double &shift1_2, const double &shift2_2, const double &time2)
{
    this->shift1_2 = shift1_2;
    this->shift2_2 = shift2_2;
    this->time2 = time2;

    qDebug() << "shift1_2:" << shift1_2 << "shift2_2:" << shift2_2 << "time2:" << time2;

    emit debug("Point 2. Shift dvl1: " + QString::number(shift1_2) + " s; Shift dvl2: "
               + QString::number(shift2_2) + " s");
}

void PrzManager::timeCorrection()
{
    // 1. reset shifts
    dvl1_X->shiftX(-dvl1_X->getShiftAmount());
    dvl1_Z->shiftX(-dvl1_Z->getShiftAmount());

    dvl2_X->shiftX(-dvl2_X->getShiftAmount());
    dvl2_Z->shiftX(-dvl2_Z->getShiftAmount());

    double point1_1, point1_2, point2_1, point2_2;

    // 2. compute sensor times for the [time1, time2] interval
    point1_1 = time1 - shift1_1;
    point1_2 = time2 - shift1_2;

    point2_1 = time1 - shift2_1;
    point2_2 = time2 - shift2_2;

    // 3. compute the sensor operating time interval
    double t_cor = (point1_2 - point1_1 + point2_2 - point2_1) / 2;

    // 4. compute correction coefficients for each sensor
    double k1 = t_cor / (point1_2 - point1_1);
    double k2 = t_cor / (point2_2 - point2_1);

    // 5. keep only the required interval
    dvl1_X->crop(point1_1, point1_2);
    dvl1_Z->crop(point1_1, point1_2);

    dvl2_X->crop(point2_1, point2_2);
    dvl2_Z->crop(point2_1, point2_2);

    qDebug() << "dvl1.size():" << dvl1_X->size() << dvl1_Z->size();
    qDebug() << "dvl2.size():" << dvl2_X->size() << dvl2_Z->size();

    // 6. align curve starts
    double shift = abs(shift1_1) + abs(shift2_1);

    dvl1_X->shiftX(shift / 2 * signShift(shift1_1));
    dvl1_Z->shiftX(shift / 2 * signShift(shift1_1));

    dvl2_X->shiftX(shift / 2 * signShift(shift2_1));
    dvl2_Z->shiftX(shift / 2 * signShift(shift2_1));

    // 7. stretch/compress
    dvl1_X->timeSync(k1);
    dvl1_Z->timeSync(k1);

    dvl2_X->timeSync(k2);
    dvl2_Z->timeSync(k2);

    dvl1_X->saveShift();
    dvl1_Z->saveShift();

    dvl2_X->saveShift();
    dvl2_Z->saveShift();

    if(dvl1_Z->size() > dvl2_Z->size())
    {
        double d = dvl1_Z->size() - dvl2_Z->size();

        dvl1_Z->chop(d);
        dvl1_X->chop(d);
    }

    else if(dvl2_Z->size() > dvl1_Z->size())
    {
        double d = dvl2_Z->size() - dvl1_Z->size();

        dvl2_Z->chop(d);
        dvl2_X->chop(d);
    }

    double delta = dvl2_Z->getStartX() - dvl1_Z->getStartX();

    if(delta != 0)
    {
        dvl1_Z->shiftX(delta);
        dvl1_X->shiftX(delta);
    }

    qDebug() << "k1:" << QString::number(k1, 'f', 10) << "k2:" << QString::number(k2, 'f', 10);

    emit debug("k1 = " + QString::number(k1, 'f', 10) + ";  k2 = " + QString::number(k2, 'f', 10));

    qDebug() << "SIZE: " << dvl1_Z->size() << dvl2_Z->size();
}

void PrzManager::medianFilter(QVector<double> &data, int n)
{
    const int N = data.size();
    if (N == 0 || n <= 0) return;

    QVector<double> out(N);
    std::vector<double> buf;
    buf.reserve(2 * n + 1);

    for (int i = 0; i < N; ++i)
    {
        const int L = std::max(0, i - n);
        const int R = std::min(N - 1, i + n);
        const int M = R - L + 1;

        buf.assign(data.begin() + L, data.begin() + R + 1);

        auto mid = buf.begin() + (M / 2);
        std::nth_element(buf.begin(), mid, buf.end());

        if (M % 2 == 1)
        {
            out[i] = *mid;
        }

        else
        {
            const double m1 = *mid;
            const double m0 = *std::max_element(buf.begin(), mid);
            out[i] = 0.5 * (m0 + m1);
        }
    }

    data.swap(out);
}

void PrzManager::expFilter(QVector<double> &data, double alpha)
{
    const int N = data.size();
    if (N <= 1) return;
    if (!(alpha > 0.0 && alpha <= 1.0)) return;

    double y = data[0];
    data[0] = y;

    const double a = 1.0 - alpha;
    for (int i = 1; i < N; ++i)
    {
        y = alpha * data[i] + a * y;
        data[i] = y;
    }
}

void PrzManager::przCreate()
{
    QString firstName = "";

    for(int i = 0; i < loaders.size(); i++)
    {
        QString name = loaders[i]->getName();

        if(name.contains("DV", Qt::CaseInsensitive))
        {
            QString axis = name.right(2);
            name.chop(2);

            if(dvl1_X == nullptr || dvl1_Z == nullptr)
            {
                firstName = name;
                if(axis == "_X")
                    dvl1_X = loaders[i];
                else if (axis == "_Z")
                    dvl1_Z = loaders[i];

                continue;
            }

            if((dvl2_X == nullptr || dvl2_Z == nullptr) && name != firstName)
            {
                if(axis == "_X")
                    dvl2_X = loaders[i];
                else if (axis == "_Z")
                    dvl2_Z = loaders[i];
            }
        }
    }

    if(   dvl1_X == nullptr
       || dvl1_Z == nullptr
       || dvl2_X == nullptr
       || dvl2_Z == nullptr
       || time1 == time2) return;

    timeCorrection();

    QVector<double> time, oX1, oZ1, oX2, oZ2;

    time.reserve(dvl1_X->size());
    oX1.reserve(dvl1_X->size());
    oZ1.reserve(dvl1_X->size());
    oX2.reserve(dvl1_X->size());
    oZ2.reserve(dvl1_X->size());

    oX1 = dvl1_X->getY();
    oX2 = dvl2_X->getY();

    oZ1 = dvl1_Z->getY();
    oZ2 = dvl2_Z->getY();

    time = dvl1_X->getX();

    qDebug() << "CREATE SIZE:" << oX1.size() << oX2.size() << oZ1.size() << oZ2.size();

    if(time.size() != oX1.size() || time.size() != oX2.size())
        return;

    for(int i = 0; i < time.size(); i++)
    {
        oX1[i] -= oX2[i];
        oZ1[i] -= oZ2[i];
    }

    oX2.clear();
    oZ2.clear();

    medianFilter(oX1, DCSettings::instance().getDvMed());
    expFilter(oX1, DCSettings::instance().getDvExp());

    medianFilter(oZ1, DCSettings::instance().getDvMed());
    expFilter(oZ1, DCSettings::instance().getDvExp());

    QVector<double> resY;

    resY.reserve(oX1.size());

    double delta = 0.0;
    constexpr double pi = 3.14159265358979323846;

    for(int i = 0; i < oX1.size(); i++)
    {
        resY.append(std::atan2(oX1[i], oZ1[i]));

        if(i > 0)
        {
            resY[i] += delta;

            const double diff = resY[i] - resY[i - 1];

            if (diff > pi)
            {
                delta -= 2.0 * pi;
                resY[i] -= 2.0 * pi;

                if(i == 1)
                    resY[i - 1] -= 2.0 * pi;
            }

            else if (diff < -pi)
            {
                delta += 2.0 * pi;
                resY[i] += 2.0 * pi;

                if(i == 1)
                    resY[i - 1] += 2.0 * pi;
            }
        }
    }

    double minVal = *std::min_element(resY.begin(), resY.end());

    for(int i = 1; i < resY.size(); i++)
        resY[i] -= minVal;


    emit przCreated(time, resY);

    oX1.clear();
    oZ1.clear();
}

void PrzManager::clear()
{
    loaders.clear();

    shift1_1 = 0.0;

    shift1_2 = 0.0;

    shift2_1 = 0.0;

    shift2_2 = 0.0;

    time1 = 0.0;

    time2 = 0.0;

    dvl1_X = nullptr;

    dvl1_Z = nullptr;

    dvl2_X = nullptr;

    dvl2_Z = nullptr;
}


#include "FileConverter.h"
#include "QFile"
#include "QTemporaryFile"
#include <QDir>
#include <dcsettings.h>

// implementation of FileConverter methods

FileConverter::FileConverter() : dat(QDir::tempPath() + "/DCtemp_XXXXXX.dat")
{
    this->file_path = "";
    window = 0;
}

void FileConverter::setWindow(int width)
{
    this->window = width;
}

FileConverter::FileConverter(const QString &file_path)
{
    this->file_path = file_path;
}

FileConverter::FileConverter(const QString &file_path, int window)
{
    this->file_path = file_path;
    this->window = window;
}

void FileConverter::set_file_path(const QString &file_path) // method for setting the file path
{
    this->file_path = file_path;
    QFileInfo info(file_path);

    fileName = info.fileName();
}

QString FileConverter::datConvert() // method for converting files to dat. Returns the new file path
{
    // file extension checks
    if (file_path.endsWith(".prz", Qt::CaseInsensitive))
    {
        return prz_to_dat();
    }

    if (file_path.endsWith(".ifh", Qt::CaseInsensitive)
    && file_path.contains("DN", Qt::CaseInsensitive))

    {
        return ifh1_to_dat();
    }

    if (file_path.endsWith(".ifh", Qt::CaseInsensitive)
    && (file_path.contains("MK", Qt::CaseInsensitive)
    || file_path.contains("KM", Qt::CaseInsensitive)))

    {
        return ifh1_to_dat();
    }

    return "";
}

void FileConverter::loadFiles()
{
    this->fileName = QFileInfo(file_path).fileName();
    QVector<double> X, Y, addY;

    if (fileName.endsWith(".prz", Qt::CaseInsensitive))
    {
        loadPRZ(file_path, X, Y);

        if(refFinishPoint - refStartPoint != 0)
        {
            syncFactor = (finishPoint - startPoint)/(refFinishPoint - refStartPoint);
        }

        double delta = startPoint - refStartPoint;
        emit progressUpdated(fileName, 100);
        emit finished(fileName, X, Y, syncFactor, delta);
    }

    else if (fileName.endsWith(".ifh", Qt::CaseInsensitive)
        && fileName.contains("DN", Qt::CaseInsensitive))

    {
        loadIFH1(file_path, X, Y);
        medianFilter(Y, DCSettings::instance().getDnMed());

        if(refFinishPoint - refStartPoint != 0)
        {
            syncFactor = (finishPoint - startPoint)/(refFinishPoint - refStartPoint);
        }

        double delta = startPoint - refStartPoint;
        emit progressUpdated(fileName, 100);
        emit finished(fileName, X, Y, syncFactor, delta);
    }

    else if (fileName.endsWith(".ifh", Qt::CaseInsensitive)
             && fileName.contains("DV", Qt::CaseInsensitive))
    {
        loadIFHdvl(file_path, X, Y, addY);

        if(refFinishPoint - refStartPoint != 0)
        {
            syncFactor = (finishPoint - startPoint)/(refFinishPoint - refStartPoint);
        }

        double delta = startPoint - refStartPoint;

        emit progressUpdated(fileName, 100);

        emit finished(fileName + "_Z", X, Y, syncFactor, delta);
        emit finished(fileName + "_X", X, addY, syncFactor, delta);
    }

    else if (fileName.endsWith(".ifh", Qt::CaseInsensitive)
        && (fileName.contains("MK", Qt::CaseInsensitive)
            || fileName.contains("KM", Qt::CaseInsensitive)))

    {
        loadIFH1(file_path, X, Y);

        if(refFinishPoint - refStartPoint != 0)
        {
            syncFactor = (finishPoint - startPoint)/(refFinishPoint - refStartPoint);
        }

        emit progressUpdated(fileName, 100);
        
        double delta = startPoint - refStartPoint;
        emit finished(fileName, X, Y, syncFactor, delta);
    }

    else if(fileName.endsWith("fs")
             || fileName.endsWith("sc"))
    {
        loadStageFile(file_path, X, Y);

        if(refFinishPoint - refStartPoint != 0)
        {
            syncFactor = (finishPoint - startPoint)/(refFinishPoint - refStartPoint);
        }

        emit progressUpdated(fileName, 100);

        double delta = startPoint - refStartPoint;
        emit finished(fileName, X, Y, syncFactor, delta);
    }

    else
    {
        emit errorOccurred(fileName, "Error: file not recognized");
        return;
    }

}

void FileConverter::loadPRZ(const QString &file_path, QVector<double> &X, QVector<double> &Y)
{
    if (file_path == ""){       // if the path is empty
        qWarning() << "Empty path passed to FileConverter";      // error signal is sent (can be replaced by qWarning)
        return;                 // exit the method
    }

    QFile prz(file_path); // create an object for the source prz file
    QString fileName = QFileInfo(file_path).fileName();

    emit progressUpdated(fileName, 0);

    if (!prz.open(QIODevice::ReadOnly)) // open file
    {
        qWarning() << "Failed to open file:" << file_path << prz.errorString();
        return;
    }

    else {

        QFileInfo fi(file_path);
        int approxPoints =  fi.size() / 39;

        X.reserve(approxPoints);
        Y.reserve(approxPoints);

        QString line; // variable for reading a line from the prz file
        QStringList data; // list for storing numbers from a line (QString)
        double num1, num2;      // num1 - time, num2 - radians

        QTextStream in(&prz);  // stream for reading the prz file

        X.clear();
        Y.clear();

        line = in.readLine();   // read the first line
        data = line.split(" "); // split by spaces
        startPoint = data[0].toDouble(); // start time
        finishPoint = data[2].toDouble(); // finish time


        int delta = static_cast<int>(startPoint) % 1000;
        int lastSec = -1;

        int k = 0;

        while (!in.atEnd()) // until end of file
        {

            line = in.readLine();   // read a new line

            data = line.split(" ", Qt::SkipEmptyParts); // split by spaces
            num2 = data[0].replace(',', '.').toDouble();   // radians (first number)
            int relTime = data[2].toInt();
            double curSec = relTime / 1000.0 + static_cast<int>(startPoint / 1000);

            X.append(curSec);
            Y.append(num2);

            k++;

            if(k % 800 == 0)
            {
                emit progressUpdated(fileName,  k * 100 / approxPoints);
            }
        }
    }

    X.squeeze();
    Y.squeeze();

    prz.close();
    qDebug() << "file" << fileName << "loaded into FileConverter";
}

void FileConverter::loadIFH1(const QString &file_path, QVector<double> &X, QVector<double> &Y)
{
    qDebug() << "window width:" << window;

    int step = 8;

    int frame = 16;

    if (file_path == "") // if the path is empty
    {
        qWarning() << "Empty path passed to FileConverter";;      // log the error
        return;                                                  // exit the method
    }

    QString fileName = QFileInfo(file_path).fileName();

    if(fileName.contains("DN", Qt::CaseInsensitive))
        step = 100;

    emit progressUpdated(fileName, 0);

    QFile ifh(file_path); // create object for the source ifh file

    if (!ifh.open(QIODevice::ReadOnly)) // open file
    {
        qWarning() << "Failed to open file:" << ifh.errorString();
        return;
    }

    else {

        const int approxPoints = int(ifh.size() / frame);
        X.clear();
        Y.clear();
        X.reserve(approxPoints);
        Y.reserve(approxPoints);

        char buf[frame]; // buffer for reading bytes

        QDataStream in(&ifh);  // stream for reading the ifh file
        // QQueue<double> q;

        X.clear();
        Y.clear();

        int msCount = 1000;
        unsigned k = 0;
        double lastSec = 0.0;
        // QQueue<double> xq;

        while (!in.atEnd() && ifh.bytesAvailable() >= frame)
        {
            if (ifh.read(buf, frame) != frame) break;
            const quint8* b = reinterpret_cast<const quint8*>(buf);

            // [0..2] sec (3 bytes)
            quint32 sec  = (quint32(b[0]) << 16) | (quint32(b[1]) << 8) | quint32(b[2]);
            // [3..4] msec (2 bytes)
            if(k == 0)
                msCount = (quint32(b[3]) << 8)  | quint32(b[4]);

            // [5..6] skip 2 bytes
            // [10..12] skip 3 bytes
            // [13..15] Z axis (3 bytes, signed)
            qint32 numRaw = (qint32(b[13]) << 16) | (qint32(b[14]) << 8) | qint32(b[15]);
            if (b[13] & 0x80) numRaw |= 0xFF000000;


            if (std::all_of(reinterpret_cast<const quint8*>(buf),
                            reinterpret_cast<const quint8*>(buf) + frame,
                            [](quint8 v){ return v == 0xFF; }))
            {
                break;
            }

            if((double(sec) > lastSec && msCount == 1000) || k == 0)
            {
                msCount = 0;
                lastSec = double(sec);
            }

            else if(double(sec) > lastSec && msCount <= 1000)
            {
                while (msCount < 1000)
                {
                    X.append(lastSec + double(msCount) / 1000.0);
                    Y.append(Y.back());

                    msCount += step;
                }

                msCount = 0;
                lastSec = sec;
            }

            if(msCount >= 1000) continue;


            double time = double(sec) + double(msCount) / 1000.0;
            double num = double(numRaw);

            k++;

            X.append(time);  // store without filtering
            Y.append(num);

            msCount += step;

            if(k % 100000 == 0)
            {
                if(fileName.contains("DN", Qt::CaseInsensitive))
                    emit progressUpdated(fileName,  k * 50.0 / approxPoints);
                else
                    emit progressUpdated(fileName,  k * 100.0 / approxPoints);
            }
                
        }

        X.squeeze();
        Y.squeeze();

        bool flag = true;
        ifh.seek(ifh.size() - 61);
        for (int i = 0; i < 5; i++){
            if(!in.atEnd()){
                in >> buf[i];
            }
            else break;
            if (buf[i] != '#') {flag = false; break;}
        }

        if (flag)
        {
            QRegularExpression datePattern(R"((\d{2}\.\d{2}\.\d{4} \d{2}:\d{2}:\d{2}))"); // date/time pattern
            for (int i = 0; i < 2; i ++){
                QByteArray data = ifh.read(20); // read 20 bytes into an array
                QString text = QString::fromLatin1(data); // convert to string

                if(i == 0)
                    qDebug() << "START TIME:" << text;
                else
                    qDebug() << "END TIME:" << text;

                QRegularExpressionMatch match = datePattern.match(text); // find matches
                if (match.hasMatch())
                {
                    QString dateString = match.captured(1);
                    QDateTime dateTime = QDateTime::fromString(dateString, "dd.MM.yyyy HH:mm:ss");
                    dateTime = dateTime.toUTC();  // convert to UTC

                    if (i == 0)
                        startPoint = dateTime.toSecsSinceEpoch();
                    else if (i == 1)
                        finishPoint = dateTime.toSecsSinceEpoch();

                    ifh.seek(ifh.pos() + 2);
                    data = ifh.read(6);
                    text = QString::fromLatin1(data);
                    bool ok;
                    if (i == 0)
                        refStartPoint = text.toUInt(&ok,16);
                    else
                        refFinishPoint = text.toUInt(&ok,16);
                }
            }
        }
    }

    ifh.close();
    qDebug() << "файл" << fileName << "загружен в DataLoader";
    qDebug() << "X.size():" << X.size();
    qDebug() << "X.back() - X.front():" << X.back() - X.front();
}

void FileConverter::loadIFHdvl(const QString &file_path, QVector<double> &X, QVector<double> &Y, QVector<double> &addY)
{
    int dvlWindow = 0, frame = 16;

    if (file_path == "") // if the path is empty
    {
        qWarning() << "В FileConverter передан пустой путь";;      // log the error
        return;                                                  // exit the method
    }

    QString fileName = QFileInfo(file_path).fileName();

    emit progressUpdated(fileName, 0);

    QFile ifh(file_path); // create object for the source ifh file

    if (!ifh.open(QIODevice::ReadOnly)) // open file
    {
        qWarning() << "Не удалось открыть файл:" << ifh.errorString();
        return;
    }

    else {

        const int approxPoints = int(ifh.size() / frame);
        X.clear();
        Y.clear();
        X.reserve(approxPoints);
        Y.reserve(approxPoints);
        addY.reserve(approxPoints);

        char buf[frame]; // buffer for reading bytes

        QDataStream in(&ifh);  // stream for reading the ifh file
       // QQueue<double> q;

        X.clear();
        Y.clear();

        int msCount = 1000;
        unsigned k = 0;
        double lastSec = 0.0;
       // QQueue<double> xq;

        while (!in.atEnd() && ifh.bytesAvailable() >= frame)
        {
            if (ifh.read(buf, frame) != frame) break;
            const quint8* b = reinterpret_cast<const quint8*>(buf);

            // [0..2] sec (3 bytes)
            quint32 sec  = (quint32(b[0]) << 16) | (quint32(b[1]) << 8) | quint32(b[2]);
            // [3..4] msec (2 bytes)
            if(k == 0)
                msCount = (quint32(b[3]) << 8)  | quint32(b[4]);

            // [5..6] skip 2 bytes
            // [7..9] X axis (3 bytes, signed)
            qint32 xNumRaw = (qint32(b[7]) << 16) | (qint32(b[8]) << 8) | qint32(b[9]);
            if (b[7] & 0x80) xNumRaw |= 0xFF000000;
            // [10..12] skip 3 bytes
            // [13..15] Z axis (3 bytes, signed)
            qint32 zNumRaw = (qint32(b[13]) << 16) | (qint32(b[14]) << 8) | qint32(b[15]);
            if (b[13] & 0x80) zNumRaw |= 0xFF000000;


            if (std::all_of(reinterpret_cast<const quint8*>(buf),
                            reinterpret_cast<const quint8*>(buf) + frame,
                            [](quint8 v){ return v == 0xFF; }))
            {
                break;
            }

            if((double(sec) > lastSec && msCount == 1000) || k == 0)
            {
                msCount = 0;
                lastSec = double(sec);
            }

            else if(double(sec) > lastSec && msCount <= 1000)
            {
                while (msCount < 1000)
                {
                    X.append(lastSec + double(msCount) / 1000.0);

                    Y.append(Y.back());
                    addY.append(addY.back());

                    msCount += 8;
                }

                msCount = 0;
                lastSec = sec;
            }

            if(msCount >= 1000) continue;


            double time = double(sec) + double(msCount) / 1000.0;
            double xNum = double(xNumRaw);
            double zNum = double(zNumRaw);

            k++;

            if (dvlWindow == 0)
            {
                X.append(time);  // store without filtering
                Y.append(zNum);
                addY.append(xNum);

                msCount += 8;

                if(k % 100000 == 0)
                {
                    emit progressUpdated(fileName,  k * 100 / approxPoints);
                }

                continue;
            }
        }

        X.squeeze();
        Y.squeeze();
        addY.squeeze();

        bool flag = true;
        ifh.seek(ifh.size() - 61);
        for (int i = 0; i < 5; i++){
            if(!in.atEnd()){
                in >> buf[i];
            }
            else break;
            if (buf[i] != '#') {flag = false; break;}
        }

        if (flag)
        {
            QRegularExpression datePattern(R"((\d{2}\.\d{2}\.\d{4} \d{2}:\d{2}:\d{2}))"); // date/time pattern
            for (int i = 0; i < 2; i ++){
                QByteArray data = ifh.read(20); // read 20 bytes into an array
                QString text = QString::fromLatin1(data); // convert to string

                if(i == 0)
                    qDebug() << "START TIME:" << text;
                else
                    qDebug() << "END TIME:" << text;

                QRegularExpressionMatch match = datePattern.match(text); // find matches
                if (match.hasMatch())
                {
                    QString dateString = match.captured(1);
                    QDateTime dateTime = QDateTime::fromString(dateString, "dd.MM.yyyy HH:mm:ss");
                    dateTime = dateTime.toUTC();  // convert to UTC

                    if (i == 0)
                        startPoint = dateTime.toSecsSinceEpoch();
                    else if (i == 1)
                        finishPoint = dateTime.toSecsSinceEpoch();

                    ifh.seek(ifh.pos() + 2);
                    data = ifh.read(6);
                    text = QString::fromLatin1(data);
                    bool ok;
                    if (i == 0)
                        refStartPoint = text.toUInt(&ok,16);
                    else
                        refFinishPoint = text.toUInt(&ok,16);
                }
            }
        }
    }

    ifh.close();
    qDebug() << "файл" << fileName << "загружен в DataLoader";
    qDebug() << "X.size():" << X.size();
    qDebug() << "X.back() - X.front():" << X.back() - X.front();
}

void FileConverter::medianFilter(QVector<double> &data, int radius)
{
    if (data.isEmpty() || radius < 1) return;

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

        emit progressUpdated(fileName,  50.0 + double(i) * 100 / data.size() / 2);
    }

    data.swap(result);
}


bool FileConverter::savePD(const QVector<double> &X, const QVector<double> &Y, const QString &format)
{
    if(file_path.isEmpty()
        || X.isEmpty()
        || Y.isEmpty()) return false;

    QVector<double> resX, resY;

    resample(X,Y, 1.0, resX, resY);


    QFile file(file_path);
    if(file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);

        if(format == "DATE")
        {

            out << "Дата/время\tГлубина(м.)\n";

            for(int i = 0; i < resX.size() && i < resY.size(); i++)
            {
                QDateTime dt = QDateTime::fromSecsSinceEpoch(resX[i]);
                QString date = dt.toString("dd.MM.yyyy HH:mm:ss");

                out << date << "\t" << QString::number(resY[i], 'f', 3) << "\n";
            }
        }

        else if (format == "UNIX")
        {
            for(int i = 0; i < resX.size() && i < resY.size(); i++)
            {
                out << QString::number(resX[i], 'f', 0) << "\t" << QString::number(resY[i], 'f', 3) << "\n";
            }
        }
    }

    file.close();

    return true;
}

bool FileConverter::saveGl1(const QVector<int> &X,
                            const QVector<double> &Y,
                            int startFrame,
                            const QString &path)
{
    if(path.isEmpty()) return false;

    QFile file(path);

    if(file.open(QIODevice::WriteOnly))
    {
        QDataStream out(&file);
        out.setByteOrder(QDataStream::LittleEndian);

        for(int i = 0; i < X.size() && i < Y.size(); i++)
        {
            out << X[i] + startFrame << int(Y[i] * 100);
        }

        if (file.isOpen())
            file.close();
    }

    return true;
}

QString FileConverter::getName()
{
    if(file_path == "") return "";

    QFileInfo fi(file_path);
    return fi.fileName();
}

void FileConverter::getSyncPoints(double &start,
                                  double &finish,
                                  double &startRef,
                                  double &finishRef)
{
    start = startPoint;
    finish = finishPoint;
    startRef = refStartPoint;
    finishRef = refFinishPoint;
}

void FileConverter::resample(const QVector<double> &X, const QVector<double> &Y, const double &step,
                             QVector<double> &resX, QVector<double> &resY)
{
    if(X.isEmpty() && Y.isEmpty()) return;

    resX.clear();
    resY.clear();

    double start  = std::ceil(X.front() / step) * step;
    double finish = std::floor(X.back() / step) * step;
    int length = std::floor((finish - start) / step) + 1;

    resX.reserve(length);
    resY.reserve(length);

    int k = 0, j = 0;

    while(start + k * step <= finish)
    {
        double x = start + k * step;
        double y;

        while(j < X.size() - 1 && x > X[j + 1])
            j++;

        if(j + 1 > X.size() - 1) break;

        if(x >= X[j] && x <= X[j + 1] && (X[j + 1] - X[j]) != 0)
            y = Y[j] + (x - X[j]) / (X[j + 1] - X[j]) * (Y[j + 1] - Y[j]);
        else
            break;

        resX.append(x);
        resY.append(y);

        k++;
    }
}


QString FileConverter::prz_to_dat() // convert from prz to dat
{
    if (file_path == ""){       // if the path is empty
        qWarning() << "В FileConverter передан пустой путь";      // error signal is sent (can be replaced by qWarning)
        return"";                 // exit the method
    }


    QFile prz(file_path); // create object for the source prz file
    QString fileName = QFileInfo(file_path).fileName();

    if (!prz.open(QIODevice::ReadOnly)) // open file
    {
        qWarning() << "Не удалось открыть файл:" << file_path << prz.errorString();
        return "";
    }

    else {

        QString line; // variable for reading a line from the prz file
        QStringList data; // list for storing numbers from a line (QString)
        double num1, num2;      // num1 - time, num2 - radians
        double start = 0.0, finish = 0.0; // temporary time storage

        if (!dat.open()) { // open dat file with check
            qWarning() << "Ошибка: не удалось открыть временный файл" << dat.errorString();
            return "";
        }

        QDataStream out(&dat); // stream for writing dat file in binary mode
        QTextStream in(&prz);  // stream for reading the prz file

        out.setByteOrder(QDataStream::LittleEndian); // set write order

        line = in.readLine();   // read the first line
        data = line.split(" "); // split line by spaces
        start = data[0].toDouble(); // start time
        finish = data[2].toDouble(); // finish time

        int delta = static_cast<int>(start) % 1000;
        int lastSec = -1;

        out << start << finish << start << finish;

        int k = 0;

        while (!in.atEnd()) // until end of file
        {

            line = in.readLine();   // read a new line
            if (k % 125 != 0 || k * 8 < (1000 - delta)) {k++; continue;}
            else k = 0;

            data = line.split(" ", Qt::SkipEmptyParts); // split line by spaces
            num2 = data[0].replace(',', '.').toDouble();   // radians (first number)
            int relTime = data[2].toInt();
            int curSec = relTime / 1000 + static_cast<int>(start / 1000);

            if (curSec > lastSec && relTime >= (1000 - delta)) {
                out << double(curSec) << num2;
                lastSec = curSec;
            }

            k++;
        }
    }

    dat.close(); // close files
    prz.close();

    qDebug() << "файл" << fileName << "конвертирован в dat";

    return dat.fileName();
}

QString FileConverter::ifh1_to_dat()
{
    double startTime, endTime, startRef, endRef;

    qDebug() << "window width:" << window;

    if (file_path == "") // if the path is empty
    {
        qWarning() << "В FileConverter передан пустой путь";;      // log the error
        return"";                                                  // exit the method
    }

    QString fileName = QFileInfo(file_path).fileName();

    QFile ifh(file_path); // create object for the source ifh file

    if (!ifh.open(QIODevice::ReadOnly)) // open file
    {
        qWarning() << "Не удалось открыть файл:" << ifh.errorString();
        return "";
    }

    else {

        quint8 bytes[6]; // byte buffer
        double sec, num;
        double m = 0.0;

        if (!dat.open()) { // open dat file with check
            qWarning() << "Ошибка: не удалось открыть временный файл" << dat.errorString();
            return "";
        }

        QDataStream out(&dat); // stream for writing dat file in binary mode
        QDataStream in(&ifh);  // stream for reading the ifh file
        QQueue<double> q;

        out.setByteOrder(QDataStream::LittleEndian); // set write order

        startTime = m;
        endTime = m;
        startRef = m;
        endRef = m;

        out << m << m << m << m;

        int k = 0;
        double lastSec = -1;
        QQueue<double> xq;

        while (!in.atEnd() && ifh.bytesAvailable() >= 6)
        {
            for (int i = 0; i < 3; i++){
                if(!in.atEnd())
                    in >> bytes[i];
            }

            if (ifh.bytesAvailable() >= 10) // skip 10 bytes
                in.skipRawData(10);

            for (int i = 3; i < 6; i++){
                if(!in.atEnd())
                    in >> bytes[i];
            }

            k++;

            if (std::all_of(std::begin(bytes), std::end(bytes), [](quint8 b) { return b == 0xFF; })) // check for end of data
            {
                qDebug() << "FC КОНЕЦ ФАЙЛА";
                break; // exit loop
            }

            sec  = static_cast<double>((static_cast<quint32>(bytes[0]) << 16) |
                                      (static_cast<quint32>(bytes[1]) << 8)  |
                                      static_cast<quint32>(bytes[2]));

            // read `num` as a signed 3-byte number
            qint32 rawNum = (static_cast<qint32>(bytes[3]) << 16) |
                            (static_cast<qint32>(bytes[4]) << 8)  |
                            static_cast<qint32>(bytes[5]);

            if (bytes[3] & 0x80)  // sign check
                rawNum |= 0xFF000000;  // sign extension

            num = static_cast<double>(rawNum);  // cast to double

            if (window == 0 && sec > lastSec)
            {
                out << sec << num;  // store without filtering
                lastSec = sec;
                continue;
            }

            q.enqueue(num);
            xq.enqueue(sec);

            if(q.size() > (2 * window + 1))
            {
                q.dequeue();
                xq.dequeue();
            }

            if (q.size() < (2 * window + 1)) continue;

            if(xq[window] > lastSec)
            {
                QVector<double> t = q;
                std::sort(t.begin(), t.end());
                out << xq[window] << t[window];

                lastSec = xq[window];
            }
        }

        bool flag = true;
        ifh.seek(ifh.size() - 61);
        for (int i = 0; i < 5; i++){
            if(!in.atEnd()){
                in >> bytes[i];
            }
            else break;
            if (bytes[i] != '#') {flag = false; break;}
        }
        if (flag)
        {
            QRegularExpression datePattern(R"((\d{2}\.\d{2}\.\d{4} \d{2}:\d{2}:\d{2}))"); // date/time pattern
            for (int i = 0; i < 2; i ++){
                QByteArray data = ifh.read(20); // read 20 bytes into an array
                QString text = QString::fromLatin1(data); // convert to string

                if(i == 0)
                    qDebug() << "START TIME:" << text;
                else
                    qDebug() << "END TIME:" << text;

                QRegularExpressionMatch match = datePattern.match(text); // find matches
                if (match.hasMatch())
                {
                    QString dateString = match.captured(1);
                    QDateTime dateTime = QDateTime::fromString(dateString, "dd.MM.yyyy HH:mm:ss");
                    dateTime = dateTime.toUTC();  // convert to UTC

                    if (i == 0)
                        startTime = dateTime.toSecsSinceEpoch();
                    else if (i == 1)
                        endTime = dateTime.toSecsSinceEpoch();

                    ifh.seek(ifh.pos() + 2);
                    data = ifh.read(6);
                    text = QString::fromLatin1(data);
                    bool ok;
                    if (i == 0)
                        startRef = text.toUInt(&ok,16);
                    else
                        endRef = text.toUInt(&ok,16);
                }
        }
            out << startTime << endTime << startRef << endRef;
    }


    dat.close(); // close files
    ifh.close();

    qDebug() << "файл" << fileName << "конвертирован в dat";

    return dat.fileName();
    }
}

void FileConverter::loadStageFile(const QString &file_path, QVector<double> &X, QVector<double> &Y)
{
    if (file_path == "") // if the path is empty
    {
        qWarning() << "В FileConverter передан пустой путь";;      // log the error
        return;                                                    // exit the method
    }

    QFile file(file_path);
    QString fileName = QFileInfo(file_path).fileName();

    if (!file.open(QIODevice::ReadOnly)) // open file
    {
        qWarning() << "Не удалось открыть файл:" << file.errorString();
        return;
    }

    X.clear();
    Y.clear();

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);
    double x,y;

    while(!in.atEnd() && file.bytesAvailable() >= 16)
    {
        in >> x >> y;
        X.append(x);
        Y.append(y);
    }

    file.close();

    qDebug() << "Файл" << fileName << "загружен";
}


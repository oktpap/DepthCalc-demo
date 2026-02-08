#include "dcvmanager.h"
//#include <QtConcurrent>

DCVManager &DCVManager::instance()
{
    static DCVManager obj;
    return obj;
}

void DCVManager::addSnap(const QVector<DataLoader *> &loaders)
{
    if(loaders.size() == 0) return;

    verContol version;

    // get directory

    QString filePath = loaders[0]->FilePath();
    QString number = "";
    int stage = 0;

    if(filePath.endsWith(".ifh") || filePath.endsWith(".prz"))
    {
        number = "Start";
        stage = 1;
    }
    else if(filePath.endsWith("fs"))
    {
        number = "Calibration";
        stage = 2;
    }
    else if(filePath.endsWith("sc"))
    {
        number = "Load";
        stage = 3;
    }

    QFileInfo info(filePath);
    QString folder = info.absolutePath();
    QString DCSpath = createDCS(folder);

    if (DCSpath.isEmpty()) return;

    QString saveFolder = DCSpath + "/DCS/" + number;

    if (!QDir().mkpath(saveFolder)) {
        qWarning() << "DCVM: error creating folder:" << saveFolder;
        return;
    }

    // iterate over files

    for(int i = 0; i < loaders.size(); i++)
    {
        QFileInfo fileinfo(loaders[i]->FilePath());
        QString name = fileinfo.completeBaseName();
        QString fname = loaders[i]->getName();

        QVector<double> X = loaders[i]->getX();
        QVector<double> Y = loaders[i]->getY();

        QString tempFilePath = "";

        if(stage == 1)
        {
            if(fname.contains("prz", Qt::CaseInsensitive))
                tempFilePath = saveFolder + "/" + "prz" + name + ".pfs";

            else if(fname.contains("DN", Qt::CaseInsensitive))
                tempFilePath = saveFolder + "/" + name + ".dfs";

            else if(fname.contains("MK", Qt::CaseInsensitive)
                    || fname.contains("KM", Qt::CaseInsensitive))
                tempFilePath = saveFolder + "/" + name + ".mfs";
        }

        else if(stage == 2)
        {
            if(fname.endsWith("pfs"))
                tempFilePath = saveFolder + "/" + name + ".psc";
            else if(fname.endsWith("dfs"))
                tempFilePath = saveFolder + "/" + name + ".dsc";
        }

        else
        {
            tempFilePath = saveFolder + "/" + fname + ".tmp";
        }

        if(writeData(X, Y, tempFilePath))
        {
            version.append(tempFilePath);
            loaders[i]->setPath(tempFilePath);
        }
    }

    versions.append(version);
}

DCVManager::DCVManager(QObject *parent)
    : QObject{parent}
{

}

bool DCVManager::writeData(const QVector<double> &X, const QVector<double> &Y, const QString &filePath)
{
    if(filePath == "")
        return false;

    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << "DCVM: failed to open file for writing:" << filePath;
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    for(int i = 0; i < X.size() && i < Y.size(); i++)
    {
        out << X[i] << Y[i];
    }

    file.close();

    return true;
}

QString DCVManager::createDCS(const QString &path)
{
    QDir currentDir(path);

    while(!currentDir.exists("DCS") && currentDir.cdUp()) {}

    if(currentDir.exists("DCS")) // папка DCS найдена
    {}
    else
    {
        currentDir.setPath(path);

        if (!currentDir.mkpath("DCS"))
        {
            qWarning() << "DCVM: не удалось создать папку DCS в:" << path;
            return QString(); // ошибка
        }
    }

    qDebug() << "DCS PATH" << currentDir.absolutePath();

    return currentDir.absolutePath();
}


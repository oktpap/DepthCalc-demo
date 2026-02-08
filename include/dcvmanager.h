#ifndef DCVMANAGER_H
#define DCVMANAGER_H

#include <QObject>
#include "dataloader.h"

class DCVManager : public QObject
{
    Q_OBJECT
public:

    static DCVManager& instance();

    void addSnap(const QVector<DataLoader*> &loaders);

private:

    explicit DCVManager(QObject *parent = nullptr);

    int currentStep{0};

    struct verContol
    {
        QVector<QString> files;

        void setFiles(QVector<QString> f)
        {
            this->files = f;
        }

        void append(QString f)
        {
            this->files.append(f);
        }

        QVector<QString> getFiles()
        {
            return files;
        }

        void clear()
        {
            for(int i = 0; i < files.size(); i++)
            {
                QFile file(files[i]);
                file.remove();
            }
        }
    };

    QVector<verContol> versions;

    QString tempPath; // path for saving temporary files

    bool writeData(const QVector<double>& X, const QVector<double> &Y, const QString &filePath);

    QString createDCS(const QString &path);

signals:
};

#endif // DCVMANAGER_H

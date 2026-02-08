#include "mainwindow.h"
#include <QApplication>

void logMsgHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    QString logDir = QCoreApplication::applicationDirPath() + "/Log";
    QDir().mkpath(logDir);

    QDir dir(logDir);
    QFileInfoList files = dir.entryInfoList({"*.txt"}, QDir::Files);
    QDateTime now = QDateTime::currentDateTime();

    for (int i = 0; i < files.size(); i++)
    {
        QFileInfo info = files[i];

        if (info.lastModified().daysTo(now) >= 5)
            QFile::remove(info.absoluteFilePath());
    }

    QString logFilePath = logDir + "/log_" + now.toString("dd-MM-yy") + ".txt";

    QFile file(logFilePath);

    if(file.open(QIODevice::Append | QIODevice::Text))
    {
        QTextStream out(&file);
        QString typeStr;
        switch (type)
        {
            case QtDebugMsg: typeStr = "[DEBUG]"; break;
            case QtWarningMsg: typeStr = "[WARNING]"; break;
            case QtCriticalMsg: typeStr = "[CRITICAL]"; break;
            case QtFatalMsg: typeStr = "[FATAL]"; break;
            case QtInfoMsg: typeStr = "[INFO]"; break;
        }

            out << QDateTime::currentDateTime().toString("hh:mm:ss")
            << " " << typeStr << ": " << message << Qt::endl;
    }
}


int main(int argc, char *argv[])
{

    QApplication app(argc, argv);


    qInstallMessageHandler(logMsgHandler);

    qDebug() << " ";
    qDebug() << "START";

    app.setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QCoreApplication::setApplicationName("DepthCalc");
    QCoreApplication::setApplicationVersion("1.033"); // Application version

    MainWindow w;

    w.setWindowTitle("DepthCalc v.1.033");

    app.setStyle(QStyleFactory::create("Fusion"));

    w.show();
    return app.exec();
}

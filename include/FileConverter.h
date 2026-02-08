#ifndef FILECONVERTER_H
#define FILECONVERTER_H

#include <QObject>
#include <QTemporaryFile>
#include <QDir>

/*
 * The FileConverter class loads and converts input files (PRZ/IFH/DVL/stage)
 * into internal X/Y arrays, supports median filtering and resampling, and
 * saves derived PD/GL1 outputs.
 *
 * Responsibilities:
 * - Load multiple input formats and extract time series data
 * - Convert PRZ/IFH data to intermediate DAT format
 * - Apply median filtering and optional resampling
 * - Track sync points and emit progress/results
 * - Save PD and GL1 files in requested formats
 */
class FileConverter : public QObject
{
    Q_OBJECT

private:

    enum class axisTypes
    {
        X,
        Y
    };

    QString file_path; // path to the source file

    QTemporaryFile dat; // temporary file

    int window {0}; // median filter window

    QString prz_to_dat(); // method for converting prz to dat

    QString ifh1_to_dat(); // method for converting ADN to dat

    QString fileName {""};

    double startPoint {0.0};

    double finishPoint {0.0};

    double refStartPoint {0.0};

    double refFinishPoint {0.0};

    double syncFactor {0.0};

    void loadStageFile(const QString &file_path, QVector<double> &X, QVector<double> &Y);

    void loadPRZ(const QString &file_path, QVector<double> &X, QVector<double> &Y);

    void loadIFH1(const QString &file_path, QVector<double> &X, QVector<double> &Y);

    void loadIFHdvl(const QString &file_path, QVector<double> &X, QVector<double> &Y, QVector<double> &addY);

    void medianFilter(QVector<double> &data, int radius);

public:

    FileConverter(const QString &file_path); // constructor

    FileConverter(const QString &file_path, int window);

    FileConverter();

    void setWindow(int width);

    void set_file_path(const QString &file_path); // method for setting file path

    QString datConvert(); // method for converting a file. Returns the new file path

    void loadFiles();

    FileConverter(const FileConverter &) = delete;

    bool savePD(const QVector<double> &X, const QVector<double> &Y, const QString &format);

    bool saveGl1(const QVector<int> &X,
                 const QVector<double> &Y,
                 int startFrame,
                 const QString &path);

    FileConverter &operator=(const FileConverter &) = delete;

    QString getName();

    void getSyncPoints(double &start,
                       double &finish,
                       double &startRef,
                       double &finishRef);

    void resample(const QVector<double> &X, const QVector<double> &Y, const double &step,
                  QVector<double> &resX, QVector<double> &resY);

    ~FileConverter() {}

signals:

    void file_error(); // signal emitted on file error

    void conv_done();  // signal for successful conversion completion

    void sendSyncPoints(double start, double finish, double startRef, double finishRef);

    void progressUpdated(const QString &fileId, int percent);

    void finished(QString fileId, QVector<double> X, QVector<double> Y,
                  double syncFactor, double delta);

    void errorOccurred(QString fileId, QString message);


};

#endif // FILECONVERTER_H

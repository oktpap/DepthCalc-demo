#ifndef DATALOADER_H
#define DATALOADER_H
/*
 * The DataLoader class stores time series data loaded from a file and provides
 * windowed subsets for plotting and synchronization.
 *
 * Responsibilities:
 * - Store raw X/Y arrays and basic metadata (name, path)
 * - Provide min/max/range queries for full data or a sub-interval
 * - Return a visible data window for efficient plotting
 * - Support time shifting and synchronization between curves
 * - Expose helper getters for start/end points and time bounds
 */

class DataLoader : public QObject
{
    Q_OBJECT

public:

    DataLoader(QVector<double> &masX, QVector<double> &masY, const QString &name);

    void update(double min, double max, QVector<double> &vis_x, QVector<double> &vis_y) const; // method for updating the visible range on the plot

    double min() const; // getters

    double max() const;

    double min(const double &start, const double &finish) const;

    double max(const double &start, const double &finish) const;

    double range() const;

    double range(const double &start, const double &finish) const;

    void get_start_point(double &start_x, double &start_y) const; // getter for the start point

    void get_start_point(double &start_y) const;

    void getTime(double &start, double &end) const;

    void setStart(double &time);

    void getXRange(double &start, double &end) const;

    double deltaTime() const; // getter for time shift used in sync

    void setDeltaTime(double const &delta);

    void timeSync(const double &factor); // auto synchronization

    QString getName() const; // file name getter

    bool getSyncState();

    bool getDataPart(const double &start, const double &finish,
                     QVector<double> &xMas, QVector<double> &yMas) const; // getter for part of the array

    bool getDataPart(const double &start, const double &finish, QVector<double> &yMas) const;

    void loadFiles();

    QString FilePath() const;

    const QVector<double>& getX() const;

    const QVector<double>& getY() const;

    void setPath(const QString &filePath);

    double getStartX() const;

    double getFinishX() const;

    void shiftX(const double &num); // time shift for the curve

    double getShiftAmount();

    void setYData( QVector<double> &yData);

    double yRange();

    double totalLen() const;

    void crop(const double &start, const double &finish);

    int size() const;

    void stretchFromMiddle(const double &factor);

    void saveShift();

    void chop(int k);

    void resample(const double &dx);

private:

    QVector<double> X, Y; // arrays storing all points in memory

    QString path; // path to the source file

    QString name; // file name

    double min_X, max_X, startTime, endTime, startRef, endRef; // bounds for non-render area tracking and sync time

    bool syncState;

    int index;

    int maxSize;

    void setTime(double &start, double &end, double &rStart, double &rEnd); // method for saving times

    double shiftAmount {0}; // time shift amount

signals:

    void dataLoaderChanged(int const &index); // signal when data changes

    void przConverted();

    void xShifted(); // signal for curve time shift

    void initDebug(const QString &text);

private slots:

    void setSyncPoints(double start, double finish, double startRef, double finishRef);

public slots:

    // void convertPrz(const double &A, const double &B);

};

#endif // DATALOADER_H

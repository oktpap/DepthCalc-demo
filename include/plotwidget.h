#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

/*
 * The PlotWidget class inherits from QCustomPlot and serves as an advanced plotting component.
 * It encapsulates all visualization functionality for displaying multiple data series with synchronized axes.
 *
 * Responsibilities:
 * - Manage multiple Y-axes for different data sources (PRZ, DN, MK, DV, etc.)
 * - Handle interactive zooming and panning with synchronized axis scaling
 * - Display and manipulate drill movement intervals as rectangles with labels
 * - Support cursor lines (vertical/horizontal) for data inspection
 * - Manage drill load visualization with load lines
 * - Provide data windowing and downsampling for performance optimization
 * - Handle keyboard shortcuts (Ctrl for line mode toggle) and mouse interactions
 *
 * Key features:
 * - Multiple independent Y-axes that maintain scale ratios during zoom/pan
 * - Layer management for organizing rectangles, plots, and markers
 * - Signal-based architecture for communicating plot changes to other components
 * - Support for inverting PRZ axis and toggling graph visibility
 */

#include <QWidget>
#include "qcustomplot.h"
#include "dataloader.h"
#include <QObject>
#include <QColor>

class PlotWidget : public QCustomPlot   //  inherits from QWidget
{

    Q_OBJECT

public:

    explicit PlotWidget(QWidget *parent = nullptr); // constructor

    ~PlotWidget();    // destructor

    QVector<QCPAxis*> axes; // array for storing y axes

    // void init(QVector<QString> const &opened_files);   // example method for changing the plot (prototype)

    void init();

    void initPlot(const QVector<const DataLoader*> &loaders);

    void getXRange(double &start, double &end);

    void lineMarker(const bool &state); // method for toggling line display mode (on/off)

    void plotChoosing(const bool &state);

    void invertPRZAxis(int state);

    void clean();

    void setTickStep(const char &axis, const int &num);

    void updateScaleFactors();

    void verticalLine(const bool &state);

    void horizontalLine(const bool &state);

    void showLoad(const double &lvl, DataLoader *loader, const QString &type);

    void addLoader(const DataLoader *loader);

    void getPDIntervals(QVector<QCPItemRect*> &intr);

    void getPDIntervals(QVector<QCPItemRect*> &intr, QVector<QCPItemText*> &lbls);

    void deleteLoader(const QString &lName);

    void hideGraph(QString name, bool state);

    void updateIntervalsView(const QString &type);

    // method for removing a movement interval from the plot (rectangle + label)
    void deleteInterval(const double &start, const double &finish);

    // method for adding a movement interval to the plot (rectangle + label)
    void addInterval(const double &start, const double &finish);

    // method for removing extra intervals (outside the selected area)
    void cropIntervals(const double &start, const double &finish);

    const DataLoader* activeGraph();

    void resetActiveGraph();

    void setAtStartPosition();

    QColor getCurrentGraphColor();

    void updateColor(const QColor &c);

    void updateGraphColors();

private:

    QVector<const DataLoader*> files; // pointer to the array of DataLoader objects

    QVector<double> scaleFactors; // axis ratios

    bool activeplot;

    int activeGraphIndex {-1};

    QCPRange lastMainRange; // previous main axis bounds

    QVector<QCPRange> lastAxesRange; // previous curve axis bounds

    QCPRange lastXRange; // previous x-axis bounds

    QColor setColor(const QString &path); // method for setting curve colors

    double maxXRange;

    int refIndex;

    QString setName(const QString &path);

    bool line = true; // line display toggle marker (Ctrl)

    bool ctrlPressed;

    bool chooseState = true;

    bool loadChooseState = false;

    QCPItemLine *cursorLine = nullptr;

    QCPItemLine *horCursorLine = nullptr;

    QCPItemLine *loadLine = nullptr;

    QPoint lastMousePos;

    bool przInverted;

    void update(const QCPRange &range);

    QVector<QCPItemRect*> rectangles;

    QVector<QCPItemText*> recLabels;

    // private method for adding an interval
    void setRectangle(QCPItemRect *rec, QCPItemText *label);

    double deltaPD = 0.0;

    double deltaGl = 0.0;

    void clearGraphFocus();

    double refRange {0};


protected:

    void leaveEvent(QEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;

    void keyReleaseEvent(QKeyEvent *event) override;

    void enterEvent(QEnterEvent *event) override;

signals:

    void loadFinished();

    // signal for updating the plot slider position
    void xRangeChanged(int value);

    void loaderDeleted(QString lName);

    void loaderAdded(QString lName);

    void intervalsCreated(QVector<QCPItemRect*> *rectangles);

    // signal notifying about movement interval changes
    // connected to DCController::intervalsChanged
    void intervalsChanged(QVector<QCPItemRect *> intervals);

    // signal about curve selection
    void graphSelected(const bool &state, const QString &type);

    void sendXRange(const double &start, const double &finish);

private slots:

    void updatePlot(const QCPRange &newRange); // slot for updating the plot

    void graphClicked(QCPAbstractPlottable *plottable, int dataIndex, QMouseEvent *event); // slot for curve click

    void plotClick(QMouseEvent *event); // slot for handling plot click

    void mainAxisChanged(const QCPRange &newRange);

    void axisChanged(const QCPRange &newRange);

    void xRangeControl(const QCPRange &newRange); // slot for limiting X-axis scaling

    void updateGraph(const int &index); // slot for updating a single plot

    void przCalibrated();

    void labelsDownsampling();

    void movePD(const QCPRange &range);

    void moveGl1(const QCPRange &range);

public slots:

    void updateXAxis(int value);

    void setCalGraph(QVector<double> x, QVector<double> y);

    void cleanLoad();

    void setLoadLine(double lvl);

    void dataShifted(); // slot for redraw after time shift (not used)

    void updateGraph(DataLoader* loader);

    void removeLoader(const DataLoader* loader);

    void cleanAll();

};

#endif // PLOTWIDGET_H

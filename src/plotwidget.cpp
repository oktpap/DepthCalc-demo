#include "plotwidget.h"
#include "dataloader.h"
#include "qcustomplot.h"
#include <qregularexpression.h>
#include "dcsettings.h"

PlotWidget::PlotWidget(QWidget *parent) // constructor
    : QCustomPlot(parent)
    , activeplot(false)
    , maxXRange(8000.0)
    , refIndex(0)
    , line(false)
    , ctrlPressed(false)
    , przInverted(false)
{
    this->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);      // enable plot interactivity
    this->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical); // enable mouse dragging
    this->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical); // enable mouse wheel zoom
    this->setOpenGl(false);  // OpenGL hardware acceleration

    setMouseTracking(true); // track mouse movement without pressing

    cursorLine = new QCPItemLine(this);
    cursorLine->setPen(QPen(Qt::gray, 1, Qt::DashLine));
    cursorLine->setVisible(false);

    horCursorLine = new QCPItemLine(this);
    horCursorLine->setPen(QPen(Qt::gray, 1, Qt::DashLine));
    horCursorLine->setVisible(false);

    loadLine = new QCPItemLine(this);
    loadLine->setPen(QPen(Qt::red, 0.5, Qt::DashLine));
    loadLine->setVisible(false);

    if(!this->layer("rectangles")) // create a layer for interval rectangles
        this->addLayer("rectangles", 0); // under plots

    setFocusPolicy(Qt::StrongFocus);

    connect(this, &QCustomPlot::plottableClick, this, &PlotWidget::graphClicked);
    connect(this, &QCustomPlot::mousePress, this, &PlotWidget::plotClick);

    connect(static_cast<QCPAxis *>(xAxis),
            static_cast<void (QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
            this,
            &PlotWidget::updatePlot);

    connect(static_cast<QCPAxis *>(xAxis),
            static_cast<void (QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
            this,
            &PlotWidget::movePD);

    connect(static_cast<QCPAxis *>(xAxis),
            static_cast<void (QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
            this,
            &PlotWidget::moveGl1);


    connect(this->yAxis, static_cast<void (QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
            this, &PlotWidget::mainAxisChanged);
}

void PlotWidget::init()
{
    this->clean();
    this->legend->setVisible(false);
    this->replot();
}

void PlotWidget::initPlot(const QVector<const DataLoader*> &loaders)
{
    double refRange = 0, start_x = 0, start_y = 0;

    this->files = loaders;

    for(int i = 0; i < files.size(); i++)
    {
        QString fileName = files[i]->getName();
        emit loaderAdded(fileName);

        if (fileName.contains("prz", Qt::CaseInsensitive)
            || fileName.contains("DV", Qt::CaseInsensitive))
        {
            refRange = files[i]->range(0, 240); // main range for scaling all curves
            refIndex = i;
        }

        connect(files[i], &DataLoader::xShifted, this, &PlotWidget::dataShifted);
        connect(files[i], &DataLoader::dataLoaderChanged, this,
            static_cast<void (PlotWidget::*)(const int &ndex)>(&PlotWidget::updateGraph)); // signal when DataLoader changes
        connect(files[i], &DataLoader::przConverted, this, &PlotWidget::updateScaleFactors);
        connect(files[i], &DataLoader::przConverted, this, &PlotWidget::przCalibrated);
    }

    // if prz is not loaded, use the first curve as the reference
    if (refRange == 0 && files.size() != 0)
    {
        refRange = files[0]->range();
        refIndex = 0;
    }

    // add plots (their count equals the number of files)
    for (int i = 0; i < files.size(); i++)
    {
        QCPAxis *newYAxis = this->axisRect()->addAxis(QCPAxis::atLeft); // create new Y axis
        newYAxis->setTickLabels(true);
        newYAxis->setBasePen(QPen(setColor(files[i]->getName()), 2)); // change axis color
        newYAxis->setTickPen(QPen(setColor(files[i]->getName())));
        newYAxis->setSubTickPen(QPen(setColor(files[i]->getName())));
        newYAxis->setTickLabelColor(setColor(files[i]->getName()));
        newYAxis->setVisible(false);
        connect(newYAxis, static_cast<void (QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
                this, &PlotWidget::axisChanged);

        axes.append(newYAxis);          // axis storage

        if (refRange != 0 && !files[i]->getName().contains("DV", Qt::CaseInsensitive))
            scaleFactors.append(files[i]->range() / refRange); // store ratio
        else
            scaleFactors.append(1.0);

        lastAxesRange.append(newYAxis->range()); // save initial axis limits

        this->addGraph(this->xAxis, axes[i]); // add plot
        this->graph(i)->setPen(QPen(setColor(files[i]->getName()))); // change curve color
        this->graph(i)->setName(setName(files[i]->getName()));
        graph(i)->setLayer("main"); // place plot on the main layer
    }


    // set axis ranges
    if(files.size() != 0){
        // coordinates of the initial prz point
        files[refIndex]->get_start_point(start_x, start_y); // get coordinates

        if(xAxis->range().upper < start_x)
        {
            this->xAxis->setRange(start_x, start_x + 240); // set initial axis ranges
            this->yAxis->setRange(0, files[refIndex]->max());
        }
        

        this->updatePlot(this->xAxis->range()); // render plots

        for(int k = 0; k < axes.size() && k < files.size(); k++)
            axes[k]->setRange(0, files[k]->max());
    }

    else // if files are not loaded, set default axis ranges
    {
        this->xAxis->setRange(0, 3600);
        this->yAxis->setRange(0, 10);
    }

    lastMainRange = this->yAxis->range(); // store main axis bounds (Y)
    lastXRange = QCPRange(0,1); // store X-axis bounds

    QSharedPointer<QCPAxisTickerDateTime> dateTimeTicker(new QCPAxisTickerDateTime);
    QDateTime startDate = QDateTime::fromString("01.01.2025 00:00:00", "dd.MM.yyyy HH:mm:ss");
    startDate = startDate.toUTC();
    dateTimeTicker->setTickOrigin(startDate.toSecsSinceEpoch());
    this->xAxis->setTicker(dateTimeTicker);

    // X-axis label format
    dateTimeTicker->setDateTimeFormat("hh:mm:ss\ndd.MM.yyyy");
    this->legend->setFont(QFont("Segoe UI", 8));
    this->legend->setVisible(true);

    double rMin = files[0]->min(start_x, start_x + 240) / scaleFactors[0]
        , rMax = files[0]->max(start_x, start_x + 240) / scaleFactors[0];

    qDebug() << rMin << rMax;

    for(int k = 0; k < files.size(); k++)
    {
        rMin = qMin(rMin, files[k]->min(start_x, start_x + 240) / scaleFactors[k]);
        rMax = qMax(rMax, files[k]->max(start_x, start_x + 240) / scaleFactors[k]);
    }

    yAxis->setRange(rMin, rMax);

    for(int k = 0; k < axes.size(); k++)
        axes[k]->setRange(rMin * scaleFactors[k], rMax * scaleFactors[k]);

    this->replot();

    qDebug() << "Main tablet initialization completed";
    qDebug() << "Curves loaded onto tablet:" << files.size();

    double a, b;
    if(!files.isEmpty())
    {
        files[refIndex]->getXRange(a,b);
    }

    emit sendXRange(a,b);
}


void PlotWidget::updatePlot(const QCPRange &newRange) // slot for updating the buffer while moving the plot
{
    double currentUpper = this->xAxis->range().upper;
    double currentLower = this->xAxis->range().lower;
    double currentSize = this->xAxis->range().size();
    double lastUpper = lastXRange.upper;
    double lastLower = lastXRange.lower;
    double lastSize = lastXRange.size();

    // check criteria for data update
    if ((currentLower <= lastLower - lastSize * 0.5
        || currentUpper >= lastUpper + lastSize * 0.5)
        || lastSize / currentSize >= 1.2
        || currentSize / lastSize >= 1.2)
    {
        labelsDownsampling();
        update(newRange);
    }

    emit xRangeChanged(static_cast<int>(newRange.lower));
}

void PlotWidget::graphClicked(QCPAbstractPlottable *plottable, int dataIndex, QMouseEvent *event) // slot for clicking on the plot
{
    if (!plottable || !chooseState || event->button() != Qt::LeftButton || ctrlPressed)
        return;

    activeplot = true; // plot activity flag
    QCPGraph *graph = qobject_cast<QCPGraph *>(plottable);
    int index = 0;

    // determining the plot index
    for(int i = 0; i < this->graphCount(); i++)
    {
        if (this->graph(i) == graph)
        {
            index = i;
            activeGraphIndex = i;
            break;
        }
    }

    for(int i = 0; i < this->graphCount(); i++)
    {
        axes[i]->setVisible(false);
    }

    this->yAxis->setVisible(false);
    axes[index]->setVisible(true);
    this->axisRect()->setRangeZoomAxes(nullptr, axes[index]);
    this->axisRect()->setRangeDragAxes(this->xAxis, axes[index]);
    axes[index]->grid()->setVisible(true);

    this->replot();

    // connected to MainWindow::graphSelected
    emit graphSelected(true, files[index]->getName());
}

void PlotWidget::plotClick(QMouseEvent *event) // slot for checking click position
{
    if (!activeplot) return; // exit if plot is not selected
    if (event->button() != Qt::RightButton) return; // exit if click is NOT with right button
    QCPAbstractPlottable *clickedGraph = nullptr; // variable to store pointer to clicked curve

    for (int i = 0; i < this->graphCount(); i++) {

        clickedGraph = this->graph(i);
        double distance = clickedGraph->selectTest(event->pos(), false, nullptr);

        // if (distance >= 0 && distance < 5) { // if click hit the plot (accuracy 5px)
        //     return; // exit
        // }
    }

    for(int i = 0; i < this->graphCount(); i++)
    {
        scaleFactors[i] = (axes[i]->range().size()) / (this->yAxis->range().size()); // ratio of current axis size to main axis size
        axes[i]->setVisible(false);
    }

    this->yAxis->setVisible(true);

    this->axisRect()->setRangeZoomAxes(this->xAxis, this->yAxis);
    this->axisRect()->setRangeDragAxes(this->xAxis, this->yAxis);

    this->replot();
    activeplot = false;
    activeGraphIndex = -1;

    // connected to MainWindow::graphSelected
    emit graphSelected(false, "");
}

void PlotWidget::mainAxisChanged(const QCPRange &newRange)
{
    if (activeplot) return;
    double oldCenter = lastMainRange.center(); // previous main axis center
    double newCenter = newRange.center(); // main axis center
    double newSize = newRange.size(); // size (length) of main axis
    double deltaCenter = newCenter - oldCenter; // distance the main axis center has shifted

    double m = 1.0;

    for (int i = 0; i < axes.size(); ++i)
    {
        if(przInverted && files[i]->getName().contains("prz", Qt::CaseInsensitive))
            m = -1.0;
        else
            m = 1.0;

        double oldAxisCenter = lastAxesRange[i].center();
       // double oldAxisSize = lastAxesRange[i].size();

        double newAxisCenter = oldAxisCenter + m * deltaCenter * scaleFactors[i]; // shift axis center by the same distance
        double newAxisSize = newSize * scaleFactors[i]; // rescale

        double newMin = newAxisCenter - newAxisSize / 2.0;
        double newMax = newAxisCenter + newAxisSize / 2.0;

        axes[i]->setRange(newMin, newMax);
        lastAxesRange[i] = QCPRange(newMin, newMax);
    }
    lastMainRange = newRange;
    this->replot();
}

void PlotWidget::axisChanged(const QCPRange &newRange)
{
    QCPAxis *senderAxis = qobject_cast<QCPAxis *>(sender()); // Get the axis that triggered the signal
    int index = axes.indexOf(senderAxis);
    lastAxesRange[index] = newRange;
}


PlotWidget::~PlotWidget() // destructor
{
    files.clear(); // clearing QVector object
}

// method for setting curve colors
QColor PlotWidget::setColor(const QString &path)
{
    QString name = path;

    if(name.endsWith("psc"))
        return QColor(Qt::darkRed);
    if (name.contains("prz", Qt::CaseInsensitive))
        return DCSettings::instance().getPrzColor();
    if (name.contains("DN", Qt::CaseInsensitive))
        return DCSettings::instance().getDnColor();
    if (name.contains("MK", Qt::CaseInsensitive) || name.contains("KM", Qt::CaseInsensitive))
        return DCSettings::instance().getMkColor();
    if(name.contains("PDOL"))
        return DCSettings::instance().getPdColor();
    if(name.contains("gl1"))
        return DCSettings::instance().getGl1Color();
    if(name.contains("DV"))
    {
        bool dvlFlag = false;
        QString axis = name.right(2);

        for(int i = 0; i < files.size(); i++)
        {
            QString fname = files[i]->getName();

            if(fname.contains("DV", Qt::CaseInsensitive))
            {
                fname.chop(2);

                if(name.contains(fname) && dvlFlag == false)
                {
                    if(axis == "_Z")
                        return DCSettings::instance().getDv1ZColor();
                    else if(axis == "_X")
                        return DCSettings::instance().getDv1XColor();
                }

                if(name.contains(fname) && dvlFlag == true)
                {

                    if(axis == "_Z")
                        return DCSettings::instance().getDv2ZColor();
                    else if(axis == "_X")
                        return DCSettings::instance().getDv2XColor();
                }

                dvlFlag = true;
            }
        }
    }

    return QColor(Qt::darkYellow);
}


// method for cleaning all plot data
void PlotWidget::clean()
{
    cleanLoad();
    this->clearGraphs();
    refIndex = 0;
    przInverted = false;
    deltaPD = 0.0;

    // removing axes
    qDebug() << files.size();
    for (int i = 0; i < axes.size(); ++i)
    {
        this->axisRect()->removeAxis(axes[i]);
    }

    axes.clear();
    files.clear();

    scaleFactors.clear();
    activeplot = false;
    lastAxesRange.clear();
    lastXRange = QCPRange();

    this->replot();

    qDebug() << "Tablet cleared";
}


/**
 * @brief PlotWidget::setTickStep set axis step
 * @param axis selected axis
 * @param num selected step
 */

void PlotWidget::setTickStep(const char &axis, const int &num)
{
    QCPAxis *ax;

    if (axis == 'x')
        ax = this->xAxis;
    else if( axis == 'y')
        ax = this->yAxis;
    else
        return;

    auto ticker = QSharedPointer<QCPAxisTickerFixed>(new QCPAxisTickerFixed);
    ticker->setTickStep(num);
    ticker->setScaleStrategy(QCPAxisTickerFixed::ssNone);
    ax->setTicker(ticker);
}

void PlotWidget::updateScaleFactors()
{
    double range_prz = 1.0, prz_max = 0.0;

    for(int i = 0; i < files.size(); i++)
    {
        if(files[i]->getName().contains("prz", Qt::CaseInsensitive)
            || files[i]->getName().contains("psc", Qt::CaseInsensitive))
        {
            axes[i]->setRangeReversed(false);
            przInverted = false;
            range_prz = files[i]->range();
            prz_max = files[i]->max();
            yAxis->setRange(0, files[i]->max());
        }
    }

    qDebug() << "RANGE PRZ:" << range_prz;

    if(range_prz == 0 || files.isEmpty())
        return;

    if(range_prz == 1.0)
        yAxis->setRange(0, files[0]->max());

    for(int i = 0; i < files.size(); i++)
    {
        if(files[i]->getName().contains("PDOL")
            || files[i]->getName().contains("gl1"))
            scaleFactors[i] = 1;
        else
            scaleFactors[i] = files[i]->range() / range_prz;
    }

    update(xAxis->range());

    for(int i = 0; i < files.size() && i < axes.size(); i++)
    {
        if(files[i]->getName().contains("PDOL"))
        {
            axes[i]->setRange(0,prz_max);
            deltaPD = 0;
            movePD(xAxis->range());

            // axes[i]->setRange(files[i]->min(), files[i]->max());
            // this->xAxis->setRange(files[i]->getStartX(), files[i]->getFinishX());
        }

        else if(files[i]->getName().contains("gl1"))
        {
            axes[i]->setRange(0,prz_max);
            deltaGl = 0;
            moveGl1(xAxis->range());

            // axes[i]->setRange(files[i]->min(), files[i]->max());
            // this->xAxis->setRange(files[i]->getStartX(), files[i]->getFinishX());
        }
        else
            axes[i]->setRange(0,files[i]->max());
    }

    replot();

    qDebug() << "ScaleFactors updated";
}

void PlotWidget::verticalLine(const bool &state)
{
    this->line = state;
}

void PlotWidget::horizontalLine(const bool &state)
{
    this->loadChooseState = state;
}

void PlotWidget::showLoad(const double &lvl, DataLoader *loader, const QString &type)
{
    int index = -1;
    const QVector<double> *X, *Y, *przY = nullptr;
    double maxPrz = 0;

    for(int i = 0; i < files.size(); i++)
    {
        if(files[i]->getName().contains("prz", Qt::CaseInsensitive))
        {
            przY = &files[i]->getY();
            break;
        }
    }

    cleanLoad();

    X = &loader->getX();
    Y = &loader->getY();

    double maxY = loader->max();
    double minCandleLen = DCSettings::instance().getMinCandleLen() / 100.0;

    bool st = false;
    int stX = 0, fnX = 0;

    for(int i = 0; i < X->size(); i++)
    {
        if((*Y)[i] >= lvl)
        {
            if(st)
            {
                fnX = i;
                continue;
            }
            else
            {
                st = true;
                stX = i;
            }
        }
        else
        {
            if(!st) continue;
            else
            {
                if(abs((*X)[fnX] - (*X)[stX]) > 3)
                {
                    bool shouldAdd = (przY == nullptr) 
                     || (fnX < przY->size() && abs((*przY)[fnX] - (*przY)[stX]) > minCandleLen);
    
                    if(shouldAdd)
                    {
                        QCPItemRect *pd = new QCPItemRect(this);
                        QCPItemText *pdLabel = new QCPItemText(this);
                        setRectangle(pd, pdLabel);

                        double left = (*X)[stX];
                        double right = (*X)[fnX];

                        pd->topLeft->setCoords(left, maxY);
                        pd->bottomRight->setCoords(right, -maxY);
                        rectangles.append(pd);

                        pdLabel->position->setCoords((left + right) / 2.0, 0.95);
                        pdLabel->setText(QString::number(rectangles.size()));
                        pdLabel->setPen(Qt::NoPen);
                        recLabels.append(pdLabel);
                    }
                }

                st = false;
            }
        }
    }

    emit intervalsCreated(&rectangles);

    labelsDownsampling();

    replot();
}

void PlotWidget::addLoader(const DataLoader *loader)
{
    files.append(loader);
    QString lName = loader->getName();

    if(!lName.isEmpty())
    {
        connect(loader, &DataLoader::dataLoaderChanged, this, static_cast<void (PlotWidget::*)(const int &index)>(&PlotWidget::updateGraph));
        connect(loader, &DataLoader::xShifted, this, &PlotWidget::dataShifted);

        QCPAxis *newYAxis = this->axisRect()->addAxis(QCPAxis::atLeft); // creating new y axis
        newYAxis->setTickLabels(true);
        newYAxis->setBasePen(QPen(setColor(lName), 2)); // changing axis color
        newYAxis->setTickPen(QPen(setColor(lName)));
        newYAxis->setSubTickPen(QPen(setColor(lName)));
        newYAxis->setTickLabelColor(setColor(lName));
        newYAxis->setVisible(false);
        connect(newYAxis, static_cast<void (QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
                this, &PlotWidget::axisChanged);

        axes.append(newYAxis);          // saving axis
        scaleFactors.append(1);

        lastAxesRange.append(newYAxis->range()); // saving initial axis limits

        this->addGraph(this->xAxis, newYAxis); // adding plot
        this->graph(graphCount() - 1)->setPen(QPen(setColor(lName))); // changing curve color
        this->graph(graphCount() - 1)->setName(setName(lName));
        graph(graphCount() - 1)->setLayer("main");

        updateScaleFactors();

        // double rMin, rMax, start_x, finish_x;

        // start_x = xAxis->range().lower;
        // finish_x = xAxis->range().upper;

        // qDebug() << "files.size():" << files.size();
        // qDebug() << "scaleFactors.size(): " <<  scaleFactors.size();

        // for(int k = 0; k < files.size(); k++)
        // {
        //     qDebug() << k;
        //     qDebug() << k << files[k]->getName();

        //     if(!(files[k]->getName().contains("gl1") || files[k]->getName().contains("PD", Qt::CaseInsensitive)))
        //     {
        //         rMin = qMin(rMin, files[k]->min(start_x, finish_x) / scaleFactors[k]);
        //         rMax = qMax(rMax, files[k]->max(start_x, finish_x) / scaleFactors[k]);
        //     }
        // }

        // yAxis->setRange(rMin, rMax);

        // for(int k = 0; k < axes.size(); k++)
        // {
        //     if(!(files[k]->getName().contains("gl1") || files[k]->getName().contains("PD", Qt::CaseInsensitive)))
        //         axes[k]->setRange(rMin * scaleFactors[k], rMax * scaleFactors[k]);
        // }

        this->replot();
    }
}

void PlotWidget::getPDIntervals(QVector<QCPItemRect *> &intr)
{
    intr = rectangles;
}

void PlotWidget::getPDIntervals(QVector<QCPItemRect *> &intr, QVector<QCPItemText *> &lbls)
{
    intr = rectangles;
    lbls = recLabels;
}

void PlotWidget::getXRange(double &start, double &end)
{
    double a, b;
    if(!files.isEmpty())
    {
        files[refIndex]->getXRange(a,b);
        start = a;
        end = b;
    }
}

// slot for updating X axis
void PlotWidget::updateXAxis(int value)
{
    double range_size = this->xAxis->range().size(); // current width
    this->xAxis->setRange(value, value + range_size); // shift axis

    replot();
}

void PlotWidget::xRangeControl(const QCPRange &newRange)
{
    // maxWidth — maximum allowed X axis width
    double currentWidth = newRange.size();
    if (currentWidth > maxXRange)
    {
        // preserve center so user can zoom back in
        double center = newRange.center();
        double half   = maxXRange / 2.0;

        // forcibly limit (trim) X range
        this->xAxis->setRange(center - half, center + half);

        this->replot();
    }
}

// slot for updating a single plot
void PlotWidget::updateGraph(const int &index)
{
    if (index < 0 || index >= this->graphCount() || index >= files.size())
        return;

    double currentUpper = this->xAxis->range().upper;
    double currentLower = this->xAxis->range().lower;

    QVector<double> xData, yData; // arrays for storing new data
    files[index]->update(currentLower, currentUpper, xData, yData); // calling DataLoader class method

    this->graph(index)->data()->clear();

    if (!xData.isEmpty() && !yData.isEmpty()) { // redrawing plot
        this->graph(index)->setData(xData, yData);
    }
    this->replot();

    qDebug() << "curve with index" << index << "updated";
}

void PlotWidget::przCalibrated()
{
    for(int i = 0; i < files.size(); i++)
    {
        QString lName = files[i]->getName();

        if(lName.contains("MK", Qt::CaseInsensitive)
            || lName.contains("KM", Qt::CaseInsensitive))
        {
            deleteLoader(lName);
            emit loaderDeleted(lName);
            break;
        }
    }

    for(int i = 0; i < files.size(); i++)
    {
        QString lName = files[i]->getName();
        if(lName.contains("prz", Qt::CaseInsensitive))
        {
            this->graph(i)->setPen(QPen("darkRed")); // changing curve color
            axes[i]->setBasePen(QPen("darkRed"));
            axes[i]->setTickLabelColor(QColor("darkRed"));
            this->graph(i)->setName("Bit position");

            emit loaderDeleted("prz");
            emit loaderAdded(lName);

            break;
        }
    }

    replot();
}


// slot for reducing the number of candle numbers on the plot
// (threshold loading stage)
void PlotWidget::labelsDownsampling()
{
    // exit if no numbers
    if(recLabels.isEmpty()) return;

    QVector<QCPItemText*> *refLabels = &recLabels;

    int refPos = 0, k = 1, pos = 0, i = 0;

    while(i < refLabels->size())
    {
        // coordinate of main label
        refPos = (*refLabels)[i]->position->pixelPosition().x();
        k = 1;

        // iterate over all following labels
        while(k + i < refLabels->size())
        {
            // coordinate of label
            pos = (*refLabels)[k + i]->position->pixelPosition().x();

            // distance between main and current labels
            if(abs(refPos - pos) >= 50)
            {
                (*refLabels)[k + i]->setVisible(true);
                break;
            }

            else
            {
                (*refLabels)[k + i]->setVisible(false);
                k++;
            }
        }

        // make next visible label the main one
        i += k;
    }
}

void PlotWidget::movePD(const QCPRange &range)
{
    if(files.size() == 0) return;

    for (int i = 0; i < graphCount() && i < files.size(); i++)
    {
        if(files[i]->getName().contains("PDOL"))
        {
            double center = range.lower + 0.35 * range.size(), refY = 0.0;
            double minDist = std::numeric_limits<double>::max();

            auto data = graph(i)->data();
            for(auto it = data->constBegin(); it != data->constEnd(); it++)
            {
                double dist = std::abs(it->key - center);
                if (dist < minDist)
                {
                    minDist = dist;
                    refY = it->value;
                }
            }

            QCPRange current = axes[i]->range();

            // New range: refY -> 0
            axes[i]->setRange(current.lower + (refY - deltaPD), current.upper + (refY - deltaPD));

            deltaPD += refY - deltaPD;
            return;
        }
    }
}

void PlotWidget::moveGl1(const QCPRange &range)
{
    if(files.size() == 0) return;

    for (int i = 0; i < graphCount() && i < files.size(); i++)
    {
        if(files[i]->getName().contains("gl1", Qt::CaseInsensitive))
        {
            double center = range.lower + 0.25 * range.size(), refY = 0.0;
            double minDist = std::numeric_limits<double>::max();

            auto data = graph(i)->data();
            for(auto it = data->constBegin(); it != data->constEnd(); it++)
            {
                double dist = std::abs(it->key - center);
                if (dist < minDist)
                {
                    minDist = dist;
                    refY = it->value;
                }
            }

            QCPRange current = axes[i]->range();

            // New range: refY -> 0
            axes[i]->setRange(current.lower + (refY - deltaGl), current.upper + (refY - deltaGl));

            deltaGl += refY - deltaGl;

            return;
        }
    }
}


void PlotWidget::cleanLoad()
{
    for(int i = 0; i < rectangles.size(); i++)
    {
        this->removeItem(rectangles[i]);
    }

    for(int i = 0; i < recLabels.size(); i++)
    {
        this->removeItem(recLabels[i]);
    }

    rectangles.clear();
    recLabels.clear();

    loadLine->setVisible(false);

    replot();
}

void PlotWidget::setLoadLine(double lvl)
{
    double st = 0, fn = 0;

    for(int i = 0; i < files.size(); i++)
    {
        if(files[i]->getName().contains("DN", Qt::CaseInsensitive))
        {
            st = files[i]->getStartX();
            fn = files[i]->getFinishX();

            loadLine->start->setAxes(xAxis, axes[i]);
            loadLine->end->setAxes(xAxis, axes[i]);
            break;
        }
    }

    if(st == fn) return;

    loadLine->start->setCoords(st, lvl);
    loadLine->end->setCoords(fn, lvl);
    loadLine->setVisible(true);
}


// slot for redrawing plot when curve is shifted
// connected to DataLoader::xShifted() signal
void PlotWidget::dataShifted()
{
    QCPRange range = this->xAxis->range();

    update(range);
}

void PlotWidget::updateGraph(DataLoader *loader)
{
    qDebug() << "GRAPH UPDATED:" << loader->getName();

    int index = -1;

    for(int i = 0; i < files.size(); i++)
    {
        if(loader == files[i])
        {
            index = i;
            break;
        }
    }

    if(index < 0) return;

    double currentUpper = this->xAxis->range().upper;
    double currentLower = this->xAxis->range().lower;

    QVector<double> xData, yData; // arrays for storing new data
    files[index]->update(currentLower, currentUpper, xData, yData); // call DataLoader class method

    this->graph(index)->data()->clear();

    if (!xData.isEmpty() && !yData.isEmpty()) { // redraw plot
        this->graph(index)->setData(xData, yData);
    }

    this->replot();
}

void PlotWidget::removeLoader(const DataLoader *loader)
{
    int index = -1;

    for(int i = 0; i < files.size(); i++)
    {
        if(loader == files[i])
        {
            index = i;
            break;
        }
    }

    if(index < 0) return;

    clearGraphFocus(); // clearing selected curve
    this->axisRect()->removeAxis(axes[index]);

    files.remove(index);
    axes.remove(index);
    scaleFactors.remove(index);
    lastAxesRange.remove(index);
    removeGraph(index);

    if(loader->getName().contains("PDOL"))
        deltaPD = 0;
    else if(loader->getName().contains("gl1"))
        deltaGl = 0;

    disconnect(loader, nullptr, this, nullptr);

    replot();
}

void PlotWidget::cleanAll()
{
    this->init();
    xAxis->setRange(0, 240);
}

void PlotWidget::setCalGraph(QVector<double> x, QVector<double> y)
{
    if (x.isEmpty() || y.isEmpty())
    {
        qWarning() << "setCalGraph: empty data!";
        return;
    }

    disconnect(this, &QCustomPlot::plottableClick, this, &PlotWidget::graphClicked);
    disconnect(this, &QCustomPlot::mousePress, this, &PlotWidget::plotClick);
    disconnect(this->xAxis,
               static_cast<void (QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
               this,
               &PlotWidget::updatePlot);
    disconnect(this->yAxis,
               static_cast<void (QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
               this,
               &PlotWidget::mainAxisChanged);


    this->addGraph(this->xAxis, this->yAxis);
    graph(graphCount() - 1)->setLayer("main");

    int index = this->graphCount() - 1;
    this->graph(index)->setData(x, y);

    if(index == 0)
    {
        this->graph(index)->setPen(QPen(QColor("purple")));
        auto [minY, maxY] = std::minmax_element(y.begin(), y.end());
        this->yAxis->setRange(*minY, *maxY);
    }

    else if(index == 1)
         this->graph(index)->setPen(QPen(QColor("red")));

    this->xAxis->setRange(x.first(), x.last());

    this->replot();
}


QString PlotWidget::setName(const QString &path)
{
    QString name = QFileInfo(path).fileName();

    if(name.endsWith("psc"))
        return "Bit position";
    if (name.contains("prz", Qt::CaseInsensitive))
        return "Transformed curve";
    if (name.contains("DN", Qt::CaseInsensitive))
        return "Load sensor";
    if (name.contains("MK", Qt::CaseInsensitive) || name.contains("KM", Qt::CaseInsensitive))
        return "Measuring wheel";
    if(name.contains("PDOL"))
        return "Bit position";
    if(name.contains("gl1"))
        return "Depth";
    if(name.contains("DV"))
        return name;

    return "Curve";
}

void PlotWidget::update(const QCPRange &range)
{
    for (int i = 0; i < this->graphCount(); i++){
        QVector<double> xData, yData; // arrays for storing new data
        files[i]->update(range.lower, range.upper, xData, yData); // calling DataLoader class method

        if (!xData.isEmpty() && !yData.isEmpty())
        { // redrawing plot
            this->graph(i)->setData(xData, yData);
        }
        else
        {
            this->graph(i)->data()->clear();
        }
    }

    this->replot();
    lastXRange = this->xAxis->range();
}

void PlotWidget::setRectangle(QCPItemRect *rec, QCPItemText *label)
{
    rec->setBrush(QColor(255, 200, 200, 100));
    rec->setPen(Qt::NoPen);
    rec->setLayer("rectangles");
    rec->setSelectable(false);

    label->setPositionAlignment(Qt::AlignCenter);
    label->position->setAxes(xAxis, nullptr);
    label->position->setType(QCPItemPosition::ptPlotCoords);
    label->position->setTypeY(QCPItemPosition::ptAxisRectRatio);
    label->setFont(QFont(font().family(), 10));
    label->setPen(Qt::NoPen);
    label->setLayer("rectangles");
}


// method for removing movement interval from plot
void PlotWidget::deleteInterval(const double &start, const double &finish)
{
    QVector<QCPItemRect*> *intervals;
    QVector<QCPItemText*> *labels;

    intervals = &rectangles;
    labels = &recLabels;

    double st, fn;
    int i = 0;

    // removing specified interval
    while (i < intervals->size() && i < labels->size())
    {
        st = (*intervals)[i]->topLeft->key();
        fn = (*intervals)[i]->bottomRight->key();

        if(start > fn)
        {
            i++;
            continue;
        }

        if(st > finish) break;

        if(start > st && start <= fn && finish > fn)
        {
            (*intervals)[i]->bottomRight->setCoords(start, - (*intervals)[i]->topLeft->value());
            (*labels)[i]->position->setCoords((st + start) / 2.0, 0.95);
            i++;
            continue;
        }

        else if(finish > st && finish < fn && (start + 0.008 <= st || start - 0.008 < st))
        {
            (*intervals)[i]->topLeft->setCoords(finish, (*intervals)[i]->topLeft->value());
            (*labels)[i]->position->setCoords((finish + fn) / 2.0, 0.95);

            i++;
            continue;
        }

        else if(start <= st && finish >= fn)
        {
            this->removeItem((*intervals)[i]);
            this->removeItem((*labels)[i]);

            intervals->remove(i);
            labels->remove(i);

            continue;
        }

        else if(start > st && finish < fn)
        {
            QCPItemRect *rect1 = new QCPItemRect(this);
            QCPItemRect *rect2 = new QCPItemRect(this);

            QCPItemText *recLabel1 = new QCPItemText(this);
            QCPItemText *recLabel2 = new QCPItemText(this);

            setRectangle(rect1, recLabel1);
            setRectangle(rect2, recLabel2);

            rect1->topLeft->setCoords(st, (*intervals)[i]->topLeft->value());
            rect1->bottomRight->setCoords(start, -(*intervals)[i]->topLeft->value());

            recLabel1->position->setCoords((st + start) / 2.0, 0.95);
            recLabel1->setText(QString::number(i + 1));

            rect2->topLeft->setCoords(finish, (*intervals)[i]->topLeft->value());
            rect2->bottomRight->setCoords(fn, -(*intervals)[i]->topLeft->value());

            recLabel2->position->setCoords((finish + fn) / 2.0, 0.95);
            recLabel2->setText(QString::number(i + 2));

            this->removeItem((*intervals)[i]);
            this->removeItem((*labels)[i]);

            intervals->remove(i);
            labels->remove(i);

            intervals->insert(i, rect2);
            intervals->insert(i, rect1);
            labels->insert(i, recLabel2);
            labels->insert(i, recLabel1);

            i += 2;

            continue;
        }

        i++;
    }

    // renumbering labels
    for (int j = 0; j < labels->size(); ++j)
        (*labels)[j]->setText(QString::number(j + 1));

    replot();

    // signal for notifying about changes in movement intervals
    // connected to DCController::intervalsChanged
    emit intervalsChanged(*intervals);
}


// method for adding movement interval
void PlotWidget::addInterval(const double &start, const double &finish)
{
    QVector<QCPItemRect*> *intervals;
    QVector<QCPItemText*> *labels;

    double st, fn, resStart = start;
    int i = 0;

    intervals = &rectangles;
    labels = &recLabels;

    if((*intervals).isEmpty() || (*labels).isEmpty()) return;

    if(start > intervals->back()->bottomRight->key())
    {
        QCPItemRect *rect = new QCPItemRect(this);
        QCPItemText *recLabel = new QCPItemText(this);

        setRectangle(rect, recLabel);

        rect->topLeft->setCoords(start, (*intervals)[i]->topLeft->value());
        rect->bottomRight->setCoords(finish, -(*intervals)[i]->topLeft->value());

        recLabel->position->setCoords((start + finish) / 2.0, 0.95);
        recLabel->setText(QString::number(i + 1));

        intervals->append(rect);
        labels->append(recLabel);
    }

    else
    {
        while (i < intervals->size() && i < labels->size())
        {
            st = (*intervals)[i]->topLeft->key();
            fn = (*intervals)[i]->bottomRight->key();

            if(start > fn)
            {
                i++;
                continue;
            }

            if(start >= st && finish <= fn) return;

            if(start >= st && start <= fn) // first intersection
            {
                resStart = st;

                this->removeItem((*intervals)[i]);
                this->removeItem((*labels)[i]);

                intervals->remove(i);
                labels->remove(i);

                continue;
            }

            if(st > finish)
            {
                QCPItemRect *rect = new QCPItemRect(this);
                QCPItemText *recLabel = new QCPItemText(this);

                setRectangle(rect, recLabel);

                rect->topLeft->setCoords(resStart, (*intervals)[i]->topLeft->value());
                rect->bottomRight->setCoords(finish, -(*intervals)[i]->topLeft->value());

                recLabel->position->setCoords((resStart + finish) / 2.0, 0.95);
                recLabel->setText(QString::number(i + 1));

                intervals->insert(i, rect);
                labels->insert(i,recLabel);

                break;
            }

            if(finish >= st && finish <= fn)
            {
                double topLeft = (*intervals)[i]->topLeft->value();

                this->removeItem((*intervals)[i]);
                this->removeItem((*labels)[i]);

                intervals->remove(i);
                labels->remove(i);

                QCPItemRect *rect = new QCPItemRect(this);
                QCPItemText *recLabel = new QCPItemText(this);

                setRectangle(rect, recLabel);

                rect->topLeft->setCoords(resStart, topLeft);
                rect->bottomRight->setCoords(fn, -topLeft);

                recLabel->position->setCoords((resStart + fn) / 2.0, 0.95);
                recLabel->setText(QString::number(i + 1));

                intervals->insert(i, rect);
                labels->insert(i,recLabel);

                break;
            }

            if(finish > fn)
            {
                this->removeItem((*intervals)[i]);
                this->removeItem((*labels)[i]);

                intervals->remove(i);
                labels->remove(i);
            }
        }
    }



    for (int j = 0; j < labels->size(); ++j)
        (*labels)[j]->setText(QString::number(j + 1));

    replot();

    // signal for notifying about changes in movement intervals
    // connected to DCController::intervalsChanged
    emit intervalsChanged(*intervals);
}

void PlotWidget::cropIntervals(const double &start, const double &finish)
{
    if(rectangles.isEmpty()) return;

    int i = 0;
    double st1, fn1, st2, fn2; // for storing interval start and end

    st1 = rectangles[0]->topLeft->key();
    fn2 = rectangles[rectangles.size() - 1]->bottomRight->key();

    if(st1 < start)
    {
        fn1 = start;
        deleteInterval(st1, fn1);
    }

    if(fn2 > finish)
    {
        st2 = finish;
        deleteInterval(st2, fn2);
    }
}

const DataLoader* PlotWidget::activeGraph()
{
    return files[activeGraphIndex];
}

void PlotWidget::resetActiveGraph()
{
    if (!activeplot) return; // exit if plot is not selected
    QCPAbstractPlottable *clickedGraph = nullptr; // variable to store pointer to clicked curve

    for(int i = 0; i < this->graphCount(); i++)
    {
        scaleFactors[i] = (axes[i]->range().size()) / (this->yAxis->range().size()); // ratio of current axis size to main axis size
        axes[i]->setVisible(false);
    }

    this->yAxis->setVisible(true);

    this->axisRect()->setRangeZoomAxes(this->xAxis, this->yAxis);
    this->axisRect()->setRangeDragAxes(this->xAxis, this->yAxis);

    this->replot();
    activeplot = false;
    activeGraphIndex = -1;

    // connected to MainWindow::graphSelected
    emit graphSelected(false, "");
}

void PlotWidget::setAtStartPosition()
{
    if(files.size() == 0) return;

    double startPoint = 0.0;

    for(int i = 0; i < files.size(); i++)
    {
        if(files[i]->getName().contains("prz", Qt::CaseInsensitive))
        {
            startPoint = files[i]->getStartX();
            break;
        }
    }

    if(startPoint == 0.0)
        startPoint = files.front()->getStartX();

    xAxis->setRange(startPoint, startPoint + 900);
}

QColor PlotWidget::getCurrentGraphColor()
{
    if(activeGraphIndex < 0) return Qt::black;

    return setColor(files[activeGraphIndex]->getName());
}

void PlotWidget::updateColor(const QColor &c)
{
    if(activeGraphIndex < 0) return;

    QString name = files[activeGraphIndex]->getName();

    if (name.contains("prz", Qt::CaseInsensitive))
        DCSettings::instance().setPrzColor(c);
    if (name.contains("DN", Qt::CaseInsensitive))
        DCSettings::instance().setDnColor(c);
    if (name.contains("MK", Qt::CaseInsensitive) || name.contains("KM", Qt::CaseInsensitive))
        DCSettings::instance().setMkColor(c);
    if(name.contains("PDOL"))
        DCSettings::instance().setPdColor(c);
    if(name.contains("gl1"))
        DCSettings::instance().setGl1Color(c);
    if(name.contains("DV"))
    {
        bool dvlFlag = false;
        QString axis = name.right(2);

        for(int i = 0; i < files.size(); i++)
        {
            QString fname = files[i]->getName();

            if(fname.contains("DV", Qt::CaseInsensitive))
            {
                fname.chop(2);

                if(name.contains(fname) && dvlFlag == false)
                {
                    if(axis == "_Z")
                        DCSettings::instance().setDv1ZColor(c);
                    else if(axis == "_X")
                        DCSettings::instance().setDv1XColor(c);
                }

                if(name.contains(fname) && dvlFlag == true)
                {

                    if(axis == "_Z")
                        DCSettings::instance().setDv2ZColor(c);
                    else if(axis == "_X")
                        DCSettings::instance().setDv2XColor(c);
                }

                dvlFlag = true;
            }
        }
    }

    updateGraphColors();
}

void PlotWidget::clearGraphFocus()
{
    activeplot = false;

    for(int i = 0; i < this->graphCount(); i++)
    {
        axes[i]->setVisible(false);
    }

    yAxis->setVisible(true);

    this->axisRect()->setRangeZoomAxes(this->xAxis, this->yAxis);
    this->axisRect()->setRangeDragAxes(this->xAxis, this->yAxis);

    this->replot();
}

void PlotWidget::updateGraphColors()
{
    for(int i = 0; i < files.size() && i < axes.size() && i < graphCount(); i++)
    {
        QCPAxis* axis = axes[i];
        QString name = files[i]->getName();

        axis->setBasePen(QPen(setColor(name), 2));
        axis->setTickPen(QPen(setColor(name)));
        axis->setSubTickPen(QPen(setColor(name)));
        axis->setTickLabelColor(setColor(name));

        this->graph(i)->setPen(QPen(setColor(name)));
    }

    replot();
}


void PlotWidget::deleteLoader(const QString &lName)
{
    int index = -1;

    for (int i = 0; i < files.size(); i++)
    {
        if(files[i]->getName().contains(lName, Qt::CaseInsensitive))
        {
            index = i;
            break;
        }
    }

    if(index < 0) return;

    clearGraphFocus(); // clearing selected curve

    files.remove(index);
    axes.remove(index);
    scaleFactors.remove(index);
    lastAxesRange.remove(index);
    removeGraph(index);

    if(lName.contains("PDOL"))
        deltaPD = 0;
    else if(lName.contains("gl1"))
        deltaGl = 0;

    emit loaderDeleted(lName);

    replot();
}

void PlotWidget::hideGraph(QString name, bool state)
{
    if(name == "Bit position")
    {
        for(int i = 0; i < files.size(); i++)
        {
            if(files[i]->getName().endsWith("psc"))
            {
                graph(i)->setVisible(state);

                if(state)
                    graph(i)->addToLegend();
                else
                    graph(i)->removeFromLegend();

                replot();
                return;
            }
        }

        return;
    }

    if(name == "Transformed curve")
    {
        for(int i = 0; i < files.size(); i++)
        {
            if(files[i]->getName().contains("prz", Qt::CaseInsensitive))
            {
                graph(i)->setVisible(state);

                if(state)
                    graph(i)->addToLegend();
                else
                    graph(i)->removeFromLegend();

                replot();
                return;
            }
        }

        return;
    }

    if(name == "Load sensor")
    {
        for(int i = 0; i < files.size(); i++)
        {
            if(files[i]->getName().contains("DN", Qt::CaseInsensitive))
            {
                graph(i)->setVisible(state);

                if(state)
                    graph(i)->addToLegend();
                else
                    graph(i)->removeFromLegend();

                replot();
                return;
            }
        }

        return;
    }

    if(name == "Measuring wheel")
    {
        for(int i = 0; i < files.size(); i++)
        {
            if(files[i]->getName().contains("MK", Qt::CaseInsensitive)
                || files[i]->getName().contains("KM", Qt::CaseInsensitive))
            {
                graph(i)->setVisible(state);

                if(state)
                    graph(i)->addToLegend();
                else
                    graph(i)->removeFromLegend();

                replot();
                return;
            }
        }

        return;
    }

    if(name == "Bit position")
    {
        for(int i = 0; i < files.size(); i++)
        {
            if(files[i]->getName().contains("PDOL"))
            {
                graph(i)->setVisible(state);

                if(state)
                    graph(i)->addToLegend();
                else
                    graph(i)->removeFromLegend();

                replot();
                return;
            }
        }

        return;
    }

    if(name == "Depth")
    {
        for(int i = 0; i < files.size(); i++)
        {
            if(files[i]->getName().contains("gl1"))
            {
                graph(i)->setVisible(state);

                if(state)
                    graph(i)->addToLegend();
                else
                    graph(i)->removeFromLegend();

                replot();
                return;
            }
        }

        return;
    }

    if(name.contains("DV", Qt::CaseInsensitive))
    {
        for(int i = 0; i < files.size(); i++)
        {
            if(files[i]->getName().contains(name))
            {
                graph(i)->setVisible(state);

                if(state)
                    graph(i)->addToLegend();
                else
                    graph(i)->removeFromLegend();

                replot();
                return;
            }
        }

        return;
    }

}


void PlotWidget::leaveEvent(QEvent *event)
{

    cursorLine->setVisible(false);
    horCursorLine->setVisible(false);
    replot();

    lastMousePos = QPoint(-1, -1);

    QCustomPlot::leaveEvent(event);
}

void PlotWidget::lineMarker(const bool &state)
{
    this->line = state;
}

void PlotWidget::plotChoosing(const bool &state)
{
    this->chooseState = state;
}

void PlotWidget::invertPRZAxis(int state)
{
    if ((!przInverted && state == 0) || (przInverted && state > 0)) return;

    for(int i = 0; i < files.size() && i < axes.size(); ++i)
    {
        if (files[i]->getName().contains("prz", Qt::CaseInsensitive))
        {
            QCPRange old = axes[i]->range();

            double delta = 0.0;
            if(state > 0)
            {
                axes[i]->setRangeReversed(true);
            }
            else
            {
                axes[i]->setRangeReversed(false);
            }

            double m = files[i]->max();

            axes[i]->setRange(-old.upper + m, -old.lower + m);
        }
    }

    this->replot();
    przInverted = !przInverted;
}

void PlotWidget::mouseMoveEvent(QMouseEvent *event)
{

    lastMousePos = event->pos();

    if (line && ctrlPressed)
    {
        double x = xAxis->pixelToCoord(event->pos().x());

        cursorLine->start->setCoords(x, yAxis->range().lower);
        cursorLine->end->setCoords(x, yAxis->range().upper);
        cursorLine->setVisible(true);

        this->replot();
    }

    if(loadChooseState && ctrlPressed)
    {
        double y = yAxis->pixelToCoord(event->pos().y());

        horCursorLine->start->setCoords(xAxis->range().lower, y);
        horCursorLine->end->setCoords(xAxis->range().upper, y);
        horCursorLine->setVisible(true);

        this->replot();
    }

    QCustomPlot::mouseMoveEvent(event);
}

void PlotWidget::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Control)
    {
        ctrlPressed = true;
        if (line && rect().contains(lastMousePos))
        {
            double x = xAxis->pixelToCoord(lastMousePos.x());

            cursorLine->start->setCoords(x, yAxis->range().lower);
            cursorLine->end->setCoords(x, yAxis->range().upper);
            cursorLine->setVisible(true);

            replot();
        }

        if(loadChooseState && rect().contains(lastMousePos))
        {
            double y = yAxis->pixelToCoord(lastMousePos.y());

            horCursorLine->start->setCoords(xAxis->range().lower, y);
            horCursorLine->end->setCoords(xAxis->range().upper, y);
            horCursorLine->setVisible(true);

            replot();
        }
    }

    QCustomPlot::keyPressEvent(event);
}

void PlotWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control)
    {
        ctrlPressed = false;

        cursorLine->setVisible(false);
        horCursorLine->setVisible(false);

        replot();
    }

    QCustomPlot::keyReleaseEvent(event);
}

void PlotWidget::enterEvent(QEnterEvent *event)
{
    lastMousePos = mapFromGlobal(QCursor::pos());
    QCustomPlot::enterEvent(event);
}

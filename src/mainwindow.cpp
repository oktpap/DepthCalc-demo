#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "plotwidget.h"
#include "dccontroller.h"
#include "calibrationmanager.h"
#include "dcsettings.h"
#include "snapshotmanager.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    DCController::instance().setPlot(ui->main_plot);
    DCController::instance().setMainWindow(this);

    // initial setup of element activity
    ui->splitter->setStretchFactor(0, 3);
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter->setSizes({3, 2});

    ui->initPlotButton->setEnabled(false);
    ui->cleanPlotButton->setEnabled(false);
    ui->xAxisScrollBar->setEnabled(false);
    ui->applyLoadButton->setEnabled(false);
    ui->gl1MethodComboBox->addItem("From top");
    ui->gl1MethodComboBox->addItem("From bottom");
    ui->gl1DirComboBox->addItem("Auto");
    ui->gl1DirComboBox->addItem("Descent");
    ui->gl1DirComboBox->addItem("Ascent");

    ui->gl1SpeedComboBox->addItem("Min");
    ui->gl1SpeedComboBox->addItem("Max");
    ui->gl1SpeedComboBox->setEditable(true);
    ui->gl1SpeedComboBox->lineEdit()->setReadOnly(true);
    ui->gl1SpeedComboBox->setInsertPolicy(QComboBox::NoInsert);
    ui->gl1SpeedComboBox->lineEdit()->setText("0.0");
    ui->gl1SpeedComboBox->lineEdit()->setAlignment(Qt::AlignCenter);

    ui->pdSpeedComboBox->addItem("Min");
    ui->pdSpeedComboBox->addItem("Max");
    ui->pdSpeedComboBox->setEditable(true);
    ui->pdSpeedComboBox->lineEdit()->setReadOnly(true);
    ui->pdSpeedComboBox->setInsertPolicy(QComboBox::NoInsert);
    ui->pdSpeedComboBox->lineEdit()->setText("0.0");
    ui->pdSpeedComboBox->lineEdit()->setAlignment(Qt::AlignCenter);
    

    // Container for undo/redo buttons in right corner of menubar
    QWidget *cornerWidget = new QWidget(this);
    auto *hLayout = new QHBoxLayout(cornerWidget);
    hLayout->setContentsMargins(0, 0, 10, 0);
    hLayout->setSpacing(2);

    undoButton = new QPushButton(cornerWidget);
    undoButton->setMaximumHeight(20);
    undoButton->setIcon(QIcon(":/resources/undo.png"));
    undoButton->setIconSize(QSize(16,16));
    undoButton->setToolTip("Undo");

    redoButton = new QPushButton(cornerWidget);
    redoButton->setMaximumHeight(20);
    redoButton->setIcon(QIcon(":/resources/redo.png"));
    redoButton->setIconSize(QSize(16,16));
    redoButton->setToolTip("Redo");

    hLayout->addWidget(undoButton);
    hLayout->addWidget(redoButton);

    undoButton->setEnabled(false);
    redoButton->setEnabled(false);

    // hided while snapshotManager is reworking
    undoButton->setVisible(false);
    redoButton->setVisible(false);

    connect(ui->cleanPaletteButton, &QPushButton::clicked, this, [this](){
        emit goUndo();
    });

    ui->menubar->setCornerWidget(cornerWidget, Qt::TopRightCorner);

    initTable("gl1");
    initTable("PDOL");

    enableGl1Correction(false, false);
    enablePDCorrection(false, false);

    // button to save bit position file
    ui->savePDButton->setPopupMode(QToolButton::MenuButtonPopup);
    QMenu *saveMenu = new QMenu(ui->savePDButton);

    QAction *unixFormat = saveMenu->addAction("UNIX TIME");
    QAction *dateFormat = saveMenu->addAction("Date/Time");

    connect(unixFormat, &QAction::triggered, this, &MainWindow::setUnixFormat);
    connect(dateFormat, &QAction::triggered, this, &MainWindow::setDateFormat);

    ui->savePDButton->setMenu(saveMenu);

    ui->fileTreeView->setContextMenuPolicy(Qt::CustomContextMenu);

    ui->tab_2->setEnabled(false);
    ui->tab_3->setEnabled(false);

    showShiftLayout(false);

    // status bar
    loadProgressBar = new QProgressBar(this);
    loadProgressBar->setMaximumWidth(100);
    loadProgressBar->setMaximumHeight(15);
    loadProgressBar->setTextVisible(false);
    loadProgressBar->setVisible(false);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    statusBar()->addWidget(spacer, 1);

    QWidget *wrap = new QWidget(this);
    auto *v = new QVBoxLayout(wrap);
    v->setContentsMargins(0,0,0,10);
    v->addStretch();
    v->addWidget(loadProgressBar);
    v->addStretch();

    statusBar()->addPermanentWidget(wrap);

    ui->firstPointTimeEdit->installEventFilter(this);
    ui->firstPointTimeEdit->setReadOnly(true);

    ui->secondPointTimeEdit->setReadOnly(true);
    ui->secondPointTimeEdit->installEventFilter(this);

    ui->firstDvlLineEdit->installEventFilter(this);
    ui->firstDvlLineEdit->setReadOnly(true);

    ui->secondDvlLineEdit->installEventFilter(this);
    ui->secondDvlLineEdit->setReadOnly(true);

    ui->initLogView->setReadOnly(true);

    ui->loadLineEdit->installEventFilter(this);
    ui->loadLineEdit->setReadOnly(true);
    ui->refTimeLineEdit->installEventFilter(this);
    ui->refTimeLineEdit->setReadOnly(true);
    ui->pdLogFirstLineEdit->installEventFilter(this);
    ui->pdLogFirstLineEdit->setReadOnly(true);
    ui->pdLogSecondLineEdit->installEventFilter(this);
    ui->pdLogSecondLineEdit->setReadOnly(true);

    ui->firstPointPDEdit->installEventFilter(this);
    ui->firstPointPDEdit->setReadOnly(true);
    ui->secondPointPDEdit->installEventFilter(this);
    ui->secondPointPDEdit->setReadOnly(true);

    ui->main_plot->lineMarker(true);
    ui->rawPalettePlot->xAxis->setLabel("Winch rotations count");
    ui->rawPalettePlot->yAxis->setLabel("Relative speed change, cm/rotation");
    ui->palettePlot->xAxis->setLabel("Winch rotations count");
    ui->palettePlot->yAxis->setLabel("Block position, m");
    ui->palettePlot->setTickStep('x', 10);
    ui->palettePlot->setTickStep('y', 2);
    ui->palettePlot->yAxis->setRange(0,20);
    ui->palettePlot->xAxis->setRange(0,100);
    ui->main_plot->installEventFilter(this);
    ui->rawPalettePlot->blockSignals(true);

    setupProjectTree();

    // signal and slot connections
    connect(ui->xAxisScrollBar, &QSlider::sliderMoved, ui->main_plot, &PlotWidget::updateXAxis);
    connect(ui->main_plot, &PlotWidget::xRangeChanged, this, &MainWindow::sliderUpdate);
    connect(ui->main_plot, &PlotWidget::sendXRange, this, &MainWindow::setScrollRange);
    connect(ui->fileTreeView, &QTreeView::customContextMenuRequested, this, &MainWindow::onFileTreeClicked);
    connect(ui->main_plot, &QCustomPlot::mousePress, this, &MainWindow::choosingPointPlotClick);
    connect(ui->przInvertCheckBox, &QCheckBox::stateChanged, ui->main_plot, &PlotWidget::invertPRZAxis);
    connect(&CalibrationManager::instance(), &CalibrationManager::calFinished, ui->rawPalettePlot, &PlotWidget::setCalGraph);
    connect(ui->przInvertCheckBox, &QCheckBox::stateChanged, &CalibrationManager::instance(), &CalibrationManager::setPrzInvert);
    connect(&CalibrationManager::instance(), &CalibrationManager::apprFinished, ui->palettePlot, &PlotWidget::setCalGraph);
    // connect(ui->loadStageAction, &QAction::triggered, this, &MainWindow::openStageFiles);
    connect(ui->openFilesAction, &QAction::triggered, this, &MainWindow::on_openProjectButton_clicked);
    connect(ui->cleanAllAction, &QAction::triggered, this, &MainWindow::cleanAll);
    connect(ui->openSettings, &QAction::triggered, this, &MainWindow::showSettings);
    connect(this, &MainWindow::cleanLoad, ui->main_plot, &PlotWidget::cleanLoad);
    connect(ui->refDepthLineEdit, &QLineEdit::textEdited, this, &MainWindow::loadLinesChanged);
    connect(ui->refTimeLineEdit, &QLineEdit::textChanged, this, &MainWindow::loadLinesChanged);
    connect(ui->loadLineEdit, &QLineEdit::textChanged, this, &MainWindow::loadLinesChanged);
    connect(ui->pdLogFirstLineEdit, &QLineEdit::textChanged, this, &MainWindow::loadLinesChanged);
    connect(ui->pdLogSecondLineEdit, &QLineEdit::textChanged, this, &MainWindow::loadLinesChanged);
    connect(ui->main_plot, &PlotWidget::loaderDeleted, this, &MainWindow::loaderDeleted);
    connect(ui->gl1DirComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::gl1LinesChanged);
    connect(ui->gl1MethodComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::gl1LinesChanged);
    connect(ui->firstPointPDEdit, &QLineEdit::textChanged, this, &MainWindow::editLoadLonesChahged);
    connect(ui->secondPointPDEdit, &QLineEdit::textChanged, this, &MainWindow::editLoadLonesChahged);
    connect(ui->pdTableWidget, &QTableWidget::cellChanged, this, &MainWindow::tableCellChanged);
    connect(ui->gl1TableWidget, &QTableWidget::cellChanged, this, &MainWindow::tableCellChanged);
    connect(ui->gl1SpeedComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::speedBoxChanged);
    connect(ui->pdSpeedComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::speedBoxChanged);
    connect(undoButton, &QPushButton::clicked, this, &MainWindow::undoButtonClicked);
    connect(redoButton, &QPushButton::clicked, this, &MainWindow::redoButtonClicked);
    connect(this, &MainWindow::goCleanAll, ui->main_plot, &PlotWidget::cleanAll);

    connect(ui->pdTableWidget->verticalScrollBar(), &QScrollBar::valueChanged,
            ui->gl1TableWidget->verticalScrollBar(), &QScrollBar::setValue);

    connect(ui->gl1TableWidget->verticalScrollBar(), &QScrollBar::valueChanged,
            ui->pdTableWidget->verticalScrollBar(), &QScrollBar::setValue);

    ui->main_plot->init();
}

int MainWindow::ADNWindow()
{
    return DCSettings::instance().getDnMed();
}

void MainWindow::initMainPlot(const QVector<const DataLoader *> &loaders)
{
    ui->main_plot->initPlot(loaders);

    for(int i = 0; i < loaders.size(); i++)
        addLoader(loaders[i]);
}

double MainWindow::getMkFactor()
{
    return ui->mkFactorSpinBox->value();
}

void MainWindow::getCalPoints(QString &first, QString &second)
{
    first = ui->firstPointTimeEdit->text();
    second = ui->secondPointTimeEdit->text();
}

QString MainWindow::getLoadType()
{
    if(ui->pdRadioButton->isChecked())
        return "pd";
    else if(ui->gl1RadioButton->isChecked())
        return "gl1";
    return "pd";
}

void MainWindow::addIntervalRow(const QString &type,
                                const int &number,
                                const double &lenght,
                                const double &mes,
                                const double &err,
                                const double &depth,
                                const double &speed)
{
    QTableWidget *table;

    if(type == "PDOL")
        table = ui->pdTableWidget;
    else if(type == "gl1")
        table = ui->gl1TableWidget;
    else return;

        addIntervalsTableRow(table,
                             number,
                             lenght,
                             mes,
                             err,
                             depth,
                             speed);
}


void MainWindow::clearTable(const QString &type)
{
    QTableWidget *table;

    if(type == "PDOL")
        table = ui->pdTableWidget;
    else if(type == "gl1")
        table = ui->gl1TableWidget;
    else return;

    for (int row = 0; row < table->rowCount(); row++)
    {
        for (int col = 0; col < table->columnCount(); col++)
        {
            if (col == 2)
                continue;

            QTableWidgetItem *item = table->item(row, col);
            if (item)
                item->setText("");
        }
    }
}

void MainWindow::fillMeasure(const QVector<double> &data, const double &totalLen)
{
    setMeasure(ui->pdTableWidget, data);
    setMeasure(ui->gl1TableWidget, data);
    // ui->manualMeasLineEdit->setText(QString::number(totalLen));

    enableGl1Correction(true, true);
    enablePDCorrection(true, true);
}

void MainWindow::getCandleCorWin(double &window)
{
    if(ui->candleCorrectionCheckBox->isEnabled())
    {
        window = ui->candleCorrectionSpinBox->value();
    }

    else
        window = 0;
}

void MainWindow::getPdCandleCorWin(double &window)
{
    if(ui->pdCandleCorrectionCheckBox->isEnabled())
    {
        window = ui->pdCandleCorrectionSpinBox->value();
    }

    else
        window = 0;
}

void MainWindow::getCandleLength(const QString &type, QVector<double> &mas)
{
    mas.clear();

    QTableWidget *table;

    if(type == "PDOL")
        table = ui->pdTableWidget;
    else if(type == "gl1")
        table = ui->gl1TableWidget;
    else return;

    for(int i = 0; i < table->rowCount(); i++)
    {
        QTableWidgetItem *item = table->item(i,1);
        if(isCellFull(item))
        {
            mas.append(item->text().toDouble());
        }

        else return;
    }
}

void MainWindow::getMeasure(const QString &type, QVector<double> &mas)
{
    mas.clear();
    QTableWidget* table;

    if(type == "PDOL")
        table = ui->pdTableWidget;
    else if(type == "gl1")
        table = ui->gl1TableWidget;
    else return;

    for(int i = 0; i < table->rowCount(); i++)
    {
        QTableWidgetItem *item = table->item(i,2);
        if(isCellFull(item))
        {
            mas.append(item->text().toDouble());
        }

        else return;
    }
}

void MainWindow::updateGl1()
{
    on_applyGl1PushButton_clicked();
}

void MainWindow::updatePD()
{
    on_applyLoadButton_clicked();
}

void MainWindow::getRefTotalLen(int &len)
{
    if(!ui->manualMeasLineEdit->text().isEmpty() )
        len = ui->manualMeasLineEdit->text().toDouble();
    else
        len = ui->gl1MeasBox->text().toDouble();
}

void MainWindow::getPdRefTotalLen(int &len)
{
    if(!ui->pdManualMeasLineEdit->text().isEmpty() )
        len = ui->pdManualMeasLineEdit->text().toDouble();
    else
        len = ui->pdMeasBox->text().toDouble();
}

bool MainWindow::hasMeasure(const QString &type)
{
    QTableWidget* table;

    if(type == "PDOL")
        table = ui->pdTableWidget;
    else if(type == "gl1")
        table = ui->gl1TableWidget;
    else return false;

    for(int i = 0; i < table->rowCount(); i++)
    {
        QTableWidgetItem *item = table->item(i,2);
        if(isCellFull(item))
        {
            return true;
        }
    }

    return false;
}

bool MainWindow::activeSync()
{
    return DCSettings::instance().getTimeSync();
}

void MainWindow::syncIsDone(const double &MK, const double &ADN)
{
    if(MK > 0)
        ui->initLogView->appendPlainText("K_cor (MK): " + QString::number(MK));
    else
        ui->initLogView->appendPlainText("K_cor (MK): correction not performed");


    if(ADN > 0)
        ui->initLogView->appendPlainText("K_cor (DN): " + QString::number(ADN));
    else
        ui->initLogView->appendPlainText("K_cor (DN): correction not performed");
}


MainWindow::~MainWindow()
{
    qDebug() << "MAIN WINDOW CLOSED";
    delete ui;
}

void MainWindow::on_initPlotButton_clicked()

{
    if(files.isEmpty()) return;
    progressDialog.show();
    this->setEnabled(false);

    qDebug() << "on_initPlotButton_clicked";

    ui->openProjectButton->setEnabled(false);
    ui->openFilesAction->setEnabled(false);
    ui->fileTreeView->blockSignals(true);

    setStage();

    emit goMainPlotInit(files, activeSync());

    ui->initPlotButton->setEnabled(false);
    ui->fileTreeView->setFocusPolicy(Qt::NoFocus);

    qDebug() << "main plot inited with" << files.size() << "files";
    ui->xAxisScrollBar->setEnabled(true);
}


void MainWindow::on_cleanPlotButton_clicked()
{
    qDebug() << "on_cleanPlotButton_clicked";

    // Clear progress BEFORE deleting objects
    progressDialog.clear();
    
    emit goCleanAll();
    this->clear();
}


void MainWindow::on_openProjectButton_clicked() // слот кнопки для открытия папки проекта
{
    qDebug() << "on_openProjectButton_clicked";

    QString path = lastPath(); // путь к последней открытой папке
    QStringList selectedPaths = QFileDialog::getOpenFileNames(this,
                                                              "Выберите файлы",
                                                              path,
                                                              "Все файлы (*.*)");

    if (!selectedPaths.isEmpty())
    {
        QFileInfo fileInfo(selectedPaths[0]);
        QString folderPath = fileInfo.absolutePath(); // путь к папке с файлом
        saveLastPath(folderPath);
    }
    else return;

    QStringList filters;
    filters << "*.prz" << "*.ifh"; // фильтры для поиска файлов

    QStringList filePaths; // список для хранения найденных файлов

    for (int i = 0; i < selectedPaths.size(); i++) {
        QString path = selectedPaths[i];
        QFileInfo fileInfo(path);

        if (fileInfo.isDir()) {
            // если выбрана папка, ищем в ней файлы рекурсивно
            QDirIterator it(path, filters, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                filePaths.append(it.next());
            }
        } else if (fileInfo.isFile()) {
            // если выбран файл, добавляем его, если он соответствует фильтру
            if (fileInfo.suffix().toLower() == "prz" || fileInfo.suffix().toLower() == "ifh") {
                filePaths.append(path);
            }
        }
    }

    processFiles(filePaths);
}

void MainWindow::processFiles(const QStringList &filePaths) // метод для сортировки открытых файлов
{
    bool prz = false, ADN = false, MK = false;
    int DVL = 0;

    for (int k = 0; k < files.size(); k++)
    {
        QString fileName = QFileInfo(files[k]).fileName();

        if (fileName.contains("prz", Qt::CaseInsensitive))
            prz = true;
        if (fileName.contains("DN", Qt::CaseInsensitive))
            ADN = true;
        if (fileName.contains("MK", Qt::CaseInsensitive)
             || fileName.contains("KM", Qt::CaseInsensitive))
            MK = true;
        if (fileName.contains("DV", Qt::CaseInsensitive))
            DVL += 1;
    }


    for (int i = 0; i < filePaths.size(); i++)
    {
        QString fileName = QFileInfo(filePaths[i]).fileName();

        if (fileName.contains("prz", Qt::CaseInsensitive))
        {
            if(prz) continue;

            files.append(filePaths[i]);
            ui->initPlotButton->setEnabled(true);
            ui->cleanPlotButton->setEnabled(true);
            projectTreeAppend(fileName, true, filePaths[i]);
            prz = true;
            continue;
        }

        if (fileName.contains("DN", Qt::CaseInsensitive))
        {       
            if(ADN) continue;

            files.append(filePaths[i]);
            ui->initPlotButton->setEnabled(true);
            ui->cleanPlotButton->setEnabled(true);
            projectTreeAppend(fileName, true, filePaths[i]);
            ADN = true;
        }

        if ((fileName.contains("MK", Qt::CaseInsensitive)
                  || fileName.contains("KM", Qt::CaseInsensitive)) && !MK)
        {
            if(MK) continue;

            files.append(filePaths[i]);
            ui->initPlotButton->setEnabled(true);
            ui->cleanPlotButton->setEnabled(true);
            projectTreeAppend(fileName, true, filePaths[i]);
            MK = true;
        }

        if (fileName.contains("DV", Qt::CaseInsensitive))
        {
            if(DVL >= 2) continue;

            DVL++;
            bool ok = true;

            for(int k = 0; k < files.size(); k++)
            {
                if(files[k] == filePaths[i])
                {
                    ok = false;
                    break;
                }
            }

            if(!ok) continue;

            files.append(filePaths[i]);
            ui->initPlotButton->setEnabled(true);
            ui->cleanPlotButton->setEnabled(true);
            projectTreeAppend(fileName, true, filePaths[i]);
        }
    }
}

// слот для обновления положения слайдера графика
void MainWindow::sliderUpdate(int value)
{
    ui->xAxisScrollBar->setValue(value);
}

void MainWindow::saveLastPath(const QString &path)
{
    QSettings settings("GORIZONT", "DepthCalc");
    settings.setValue("lastPath", path);
}

QString MainWindow::lastPath()
{
    QSettings settings("GORIZONT", "DepthCalc");
    // возвращаем путь. Если его нет, то домашний путь
    return settings.value("lastPath", QDir::homePath()).toString();
}


void MainWindow::setupProjectTree()
{
    if(!model)
        model = new QStandardItemModel(this);
    else
        model->clear();

    ui->fileTreeView->setModel(model);

    przItem = new QStandardItem("Преобразованная кривая");
    ADNItem = new QStandardItem("Датчик нагрузки");
    MKItem = new QStandardItem("Мерное колесо");
    dvlItem = new QStandardItem("Датчики вращения");

    przItem->setForeground(QBrush(Qt::red));
    ADNItem->setForeground(QBrush(Qt::blue));
    MKItem->setForeground(QBrush(Qt::darkGreen));
    dvlItem->setForeground(QBrush(Qt::red));

    przItem->setFlags({});
    ADNItem->setFlags({});
    MKItem->setFlags({});
    dvlItem->setFlags({});
}


void MainWindow::projectTreeUpdate() // обновление дерева
{
    updateTreeSection(przItem);
    updateTreeSection(ADNItem);
    updateTreeSection(MKItem);
    updateTreeSection(dvlItem);

    ui->fileTreeView->expandAll();
}


void MainWindow::updateTreeSection(QStandardItem *item) // проверка группы дерева
{
    if (!item) return;

    QModelIndex index = model->indexFromItem(item);


    if (!index.isValid() && item->rowCount() > 0)
    {
        model->appendRow(item);
        int row = model->indexFromItem(item).row();
        ui->fileTreeView->setRowHidden(row, QModelIndex(), false);
    }

    else if(index.isValid() && item->rowCount() == 0)
    {
        int row = model->indexFromItem(item).row();
        ui->fileTreeView->setRowHidden(row, QModelIndex(), true);
    }

    else if (index.isValid() && item->rowCount() > 0)
    {
        int row = model->indexFromItem(item).row();
        ui->fileTreeView->setRowHidden(row, QModelIndex(), false);
    }
}

void MainWindow::choosingPointPlotClick(QMouseEvent *event)
{
    if (!(event->modifiers() & Qt::ControlModifier)) return; // выход, если ctrl не зажат

    if (choosingFisrtPoint)
    {
        double x = ui->main_plot->xAxis->pixelToCoord(event->pos().x());
        QDateTime dt = QDateTime::fromSecsSinceEpoch(x);
        QString date = dt.toString("hh:mm:ss dd-MM-yyyy");

        ui->firstPointTimeEdit->setStyleSheet("background-color: none;");
        ui->firstPointTimeEdit->setText(date);
        ui->main_plot->setFocus();
        choosingFisrtPoint = false;
        ui->main_plot->plotChoosing(true);
    }

    if(choosingSecondPoint)
    {
        double x = ui->main_plot->xAxis->pixelToCoord(event->pos().x());
        QDateTime dt = QDateTime::fromSecsSinceEpoch(x);
        QString date = dt.toString("hh:mm:ss dd-MM-yyyy");

        ui->secondPointTimeEdit->setStyleSheet("background-color: none;");
        ui->secondPointTimeEdit->setText(date);
        ui->main_plot->setFocus();
        choosingSecondPoint = false;
        ui->main_plot->plotChoosing(true);
    }

    if(choosingLoad)
    {
        double y = -1;
        bool ok = false;

        for(int i = 0; i < loaders.size() && i < ui->main_plot->graphCount(); i++)
        {
            qDebug() << i << loaders[i]->getName();

            if(loaders[i]->getName().contains("DN", Qt::CaseInsensitive))
            {
                y = ui->main_plot->axes[i]->pixelToCoord(event->pos().y());
                ok = true;
                break;
            }
        }

        if(!ok)
        {   
            qWarning() << "Ошибка выбора пороговой нагрузки";
            return;
        }

        ui->loadLineEdit->setStyleSheet("background-color: none;");
        ui->loadLineEdit->setText(QString::number(y));

        ui->main_plot->setFocus();
        choosingLoad = false;
        ui->main_plot->plotChoosing(true);
        ui->main_plot->verticalLine(true);
        ui->main_plot->horizontalLine(false);

        if(!ui->loadLineEdit->text().isEmpty())
        {
            // подключен к DCController::manualADNLoad
            emit goManLoad(y);
        }
    }

    if(choosingRefTime)
    {
        double x = ui->main_plot->xAxis->pixelToCoord(event->pos().x());
        QDateTime dt = QDateTime::fromSecsSinceEpoch(x);
        QString date = dt.toString("hh:mm:ss dd-MM-yyyy");

        ui->refTimeLineEdit->setStyleSheet("background-color: none;");
        ui->refTimeLineEdit->setText(date);
        ui->main_plot->setFocus();
        choosingRefTime = false;
        ui->main_plot->plotChoosing(true);
    }

    if (choosingFirstPDLogPoint)
    {
        double x = ui->main_plot->xAxis->pixelToCoord(event->pos().x());
        QDateTime dt = QDateTime::fromSecsSinceEpoch(x);
        QString date = dt.toString("hh:mm:ss dd-MM-yyyy");

        ui->pdLogFirstLineEdit->setStyleSheet("background-color: none;");
        ui->pdLogFirstLineEdit->setText(date);
        ui->main_plot->setFocus();
        choosingFirstPDLogPoint = false;
        ui->main_plot->plotChoosing(true);
    }

    if (choosingSecondPDLogPoint)
    {
        double x = ui->main_plot->xAxis->pixelToCoord(event->pos().x());
        QDateTime dt = QDateTime::fromSecsSinceEpoch(x);
        QString date = dt.toString("hh:mm:ss dd-MM-yyyy");

        ui->pdLogSecondLineEdit->setStyleSheet("background-color: none;");
        ui->pdLogSecondLineEdit->setText(date);
        ui->main_plot->setFocus();
        choosingSecondPDLogPoint = false;
        ui->main_plot->plotChoosing(true);
    }

    if (choosingFirstPDEditPoint)
    {
        double x = ui->main_plot->xAxis->pixelToCoord(event->pos().x());
        double msecs = static_cast<double>(x * 1000.0);
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(msecs);
        QString date = dt.toString("hh:mm:ss.zzz dd-MM-yyyy");

        ui->firstPointPDEdit->setStyleSheet("background-color: none;");
        ui->firstPointPDEdit->setText(date);
        ui->main_plot->setFocus();
        choosingFirstPDEditPoint = false;
        ui->main_plot->plotChoosing(true);
    }

    if (choosingSecondPDEditPoint)
    {
        double x = ui->main_plot->xAxis->pixelToCoord(event->pos().x());
        double msecs = static_cast<double>(x * 1000.0);
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(msecs);
        QString date = dt.toString("hh:mm:ss.zzz dd-MM-yyyy");

        ui->secondPointPDEdit->setStyleSheet("background-color: none;");
        ui->secondPointPDEdit->setText(date);
        ui->main_plot->setFocus();
        choosingSecondPDEditPoint = false;
        ui->main_plot->plotChoosing(true);
    }

    if (choosingFirstDvlPoint)
    {
        double x = ui->main_plot->xAxis->pixelToCoord(event->pos().x());

        emit sendFirstDvlPoint(x);

        QDateTime dt = QDateTime::fromSecsSinceEpoch(x);
        QString date = dt.toString("hh:mm:ss dd-MM-yyyy");

        ui->firstDvlLineEdit->setStyleSheet("background-color: none;");
        ui->firstDvlLineEdit->setText(date);
        ui->main_plot->setFocus();
        choosingFirstDvlPoint = false;
        ui->main_plot->plotChoosing(true);

        if(!ui->firstDvlLineEdit->text().isEmpty() && !ui->secondDvlLineEdit->text().isEmpty())
            ui->applyDvlPushButton->setEnabled(true);
    }

    if (choosingSecondDvlPoint)
    {
        double x = ui->main_plot->xAxis->pixelToCoord(event->pos().x());

        emit sendSecondDvlPoint(x);

        QDateTime dt = QDateTime::fromSecsSinceEpoch(x);
        QString date = dt.toString("hh:mm:ss dd-MM-yyyy");

        ui->secondDvlLineEdit->setStyleSheet("background-color: none;");
        ui->secondDvlLineEdit->setText(date);
        ui->main_plot->setFocus();
        choosingSecondDvlPoint = false;
        ui->main_plot->plotChoosing(true);

        if(!ui->firstDvlLineEdit->text().isEmpty() && !ui->secondDvlLineEdit->text().isEmpty())
            ui->applyDvlPushButton->setEnabled(true);
    }
}

void MainWindow::openStageFiles()
{
    on_cleanPlotButton_clicked();

    QString path = lastPath(); // путь к последней открытой папке
    QStringList selectedPaths = QFileDialog::getOpenFileNames(this, "Выберите файлы", path, "Все файлы (*.*)");

    qDebug() << "MW open stage files count:" << selectedPaths.size();

    if (!selectedPaths.isEmpty())
    {
    }
    else return;

    QStringList filters;
    filters << "*.prz" << "*.ifh" << "*.pfs" << "*.dfs"
            << "*.mfs" << "*.psc" << "*.dsc";

    QStringList filePaths;

    for (int i = 0; i < selectedPaths.size(); i++) {
        QString path = selectedPaths[i];
        QFileInfo fileInfo(path);

        if (fileInfo.isDir()) {
            // если выбрана папка, ищем в ней файлы рекурсивно
            QDirIterator it(path, filters, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                filePaths.append(it.next());
            }
        } else if (fileInfo.isFile()) {

            // если выбран файл, добавляем его, если он соответствует фильтру
            QString name = fileInfo.fileName();

            if (name.endsWith("fs") || name.endsWith("sc"))
            {
                filePaths.append(path);
            }
        }
    }

    qDebug() << "filePaths size:" << filePaths.size();

    setStage(filePaths);
}

void MainWindow::setStage(const QStringList &paths)
{
    if(paths.isEmpty()) return;

    bool prz = false, dn = false, mk = false, dvl = false;

    for(int i = 0; i < paths.size(); i++)
    {
        QString name = QFileInfo(paths[i]).fileName();

        if(name.contains("prz", Qt::CaseInsensitive))
        {
            if(prz) continue;
            prz = true;
        }

        if(name.contains("DN", Qt::CaseInsensitive))
        {
            if(dn) continue;
            dn = true;
        }

        if(name.contains("MK", Qt::CaseInsensitive)
                || name.contains("KM", Qt::CaseInsensitive))
        {
            if(mk) continue;
            mk = true;
        }

        if(name.contains("DV", Qt::CaseInsensitive))
        {
            if(dvl) continue;
            dvl = true;
        }
    }

    if(prz || mk || dn || dvl)
        ui->xAxisScrollBar->setEnabled(true);

    if(dvl) 
    {
        ui->tab_1->setEnabled(true);
        ui->tab_2->setEnabled(false);
        ui->tab_3->setEnabled(false);

        ui->tabWidget->setCurrentIndex(0);

        return;
    }

    else if(prz && mk)
    {
        ui->tab_1->setEnabled(false);
        ui->tab_2->setEnabled(true);
        ui->tab_3->setEnabled(false);

        ui->tabWidget->setCurrentIndex(1);

        return;
    }

    else if(prz && !mk)
    {
        ui->tab_1->setEnabled(false);
        ui->tab_2->setEnabled(false);
        ui->tab_3->setEnabled(true);

        ui->tabWidget->setCurrentIndex(2);


        return;
    }
}

QString MainWindow::getFName(const QString &name)
{
    if(name.endsWith("psc"))
        return "Положение тальблока";
    if (name.contains("prz", Qt::CaseInsensitive))
        return "Преобразованная кривая";
    if (name.contains("DN", Qt::CaseInsensitive))
        return "Датчик нагрузки";
    if (name.contains("MK", Qt::CaseInsensitive) || name.contains("KM", Qt::CaseInsensitive))
        return "Мерное колесо";
    if(name.contains("PDOL"))
        return "Положение долота";
    if(name.contains("gl1"))
        return "Глубина";
    if(name.contains("DV", Qt::CaseInsensitive))
        return name;

    return "Кривая";
}

void MainWindow::addIntervalsTableRow(QTableWidget *table,
                                      int number,
                                      double lenght,
                                      double mes,
                                      double err,
                                      double depth,
                                      double speed)
{
    int index = 0;

    if(number <= table->rowCount())
        index = qMax(0, number - 1);
    else
    {
        index = table->rowCount();
        table->insertRow(index);
    }

    QTableWidgetItem *num = new QTableWidgetItem(QString::number(number));
    num->setTextAlignment(Qt::AlignCenter);
    table->setItem(index, 0, num);

    QTableWidgetItem *measItem = table->item(index, 2);
    if (!measItem)
    {
        measItem = new QTableWidgetItem;
        table->setItem(index, 2, measItem);
        measItem->setFlags(measItem->flags() | Qt::ItemIsEditable);
        measItem->setTextAlignment(Qt::AlignCenter);
    }

    if (lenght != -1)
    {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(qRound(lenght)));
        item->setTextAlignment(Qt::AlignCenter);
        table->setItem(index, 1, item);
    }

    if (mes != -1)
    {
        QTableWidgetItem *item = table->item(index, 2);
        item->setText(QString::number(qRound(err)));
        item->setTextAlignment(Qt::AlignCenter);
        table->setItem(index, 2, item);
    }

    if (err != -1)
    {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(qRound(err)));
        item->setTextAlignment(Qt::AlignCenter);
        table->setItem(index, 3, item);
    }

    if (depth != -1)
    {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(qRound(depth)));
        item->setTextAlignment(Qt::AlignCenter);
        table->setItem(index, 4, item);
    }

    if (speed != -1)
    {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(qRound(speed)));
        item->setTextAlignment(Qt::AlignCenter);
        table->setItem(index, 5, item);
    }


    for(int i = 0 ; i < table->columnCount(); i++)
    {
        QTableWidgetItem *item = table->item(index, i);
        if(item)
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        else
        {
            item = new QTableWidgetItem("");
            item->setTextAlignment(Qt::AlignCenter);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            table->setItem(index, i, item);
        }

        if(i == 2)
            item->setFlags(item->flags() | Qt::ItemIsEditable);
    }
}

void MainWindow::deleteTableRow(QTableWidget *table, const int &number)
{
    if(number > table->rowCount() || number <= 0) return;

    int index = 0;

    index = number - 1;
    table->removeRow(index);

    for (int i = index; i < table->rowCount(); i++)
    {
        QTableWidgetItem *num = new QTableWidgetItem(QString::number(i + 1));
        num->setTextAlignment(Qt::AlignCenter);
        table->setItem(i, 0, num);
    }
}

void MainWindow::setMeasure(QTableWidget *table, const QVector<double> &data)
{
    for(int i = 0; i < data.size(); i++)
    {
        if(table->rowCount() < i + 1)
        {
            table->insertRow(i);
        }

        QTableWidgetItem *item = new QTableWidgetItem(QString::number(qRound(data[i])));
        item->setTextAlignment(Qt::AlignCenter);
        table->setItem(i, 2, item);

        if(!hasNumberInRow(table, i))
        {
            QTableWidgetItem *num = new QTableWidgetItem(QString::number(i + 1));
            num->setTextAlignment(Qt::AlignCenter);
            num->setFlags(num->flags() & ~Qt::ItemIsEditable);
            table->setItem(i, 0, num);
        }

        for(int col = 0; col < table->columnCount(); col++)
        {
            if(col == 2) continue;

            QTableWidgetItem *cellItem = table->item(i, col);
            if(cellItem)
            {
                cellItem->setFlags(cellItem->flags() & ~Qt::ItemIsEditable);
            }
            else
            {
                cellItem = new QTableWidgetItem("");
                cellItem->setTextAlignment(Qt::AlignCenter);
                cellItem->setFlags(cellItem->flags() & ~Qt::ItemIsEditable);
                table->setItem(i, col, cellItem);
            }
        }
    }
}


// локальный метод для удаления столбца с мерой из таблицы
void MainWindow::clearMeasure(QTableWidget *table)
{
    for (int i = 0; i < table->rowCount(); i++)
    {
        QTableWidgetItem *item1 = table->item(i, 2);
        if (item1)
            table->takeItem(i, 2);

        QTableWidgetItem *item2 = table->item(i, 3);
        if (item2)
            table->takeItem(i, 3);
    }
}

bool MainWindow::isCellFull(const QTableWidgetItem *cell)
{
    if(!cell) return false;

    bool ok = false;
    cell->text().toDouble(&ok);

    return ok;
}


// метод для обновления общей длины по мере инструмента
void MainWindow::countMeasureLen(const QString &type)
{
    QTableWidget* table;
    QLineEdit *box;

    if(type == "PDOL")
    {
        table = ui->pdTableWidget;
        box = ui->pdManualMeasLineEdit;
    }
    else if(type == "gl1")
    {
        table = ui->gl1TableWidget;
        box = ui->manualMeasLineEdit;
    }
    else return;

    double totalLen = 0;

    for(int i = 0; i < table->rowCount(); i++)
    {
        QTableWidgetItem *item = table->item(i,2);
        if(isCellFull(item))
        {
            totalLen += item->text().toDouble();
        }

        else break;
    }

    // box->setText(QString::number(totalLen));
}

void MainWindow::clearEmptyMeasure(const QString &type)
{
    QTableWidget* table;

    if(type == "PDOL")
        table = ui->pdTableWidget;
    else if(type == "gl1")
        table = ui->gl1TableWidget;
    else return;

    for(int i = 0; i < table->rowCount(); i++)
    {
        QTableWidgetItem *len = table->item(i, 1);
        QTableWidgetItem *mes = table->item(i, 2);
        if(isCellFull(len))
        {
            if(!isCellFull(mes))
            {
                double val = len->text().toDouble();
                mes->setText(QString::number(val, 'f', 2));
            }
        }

        else return;
    }
}

void MainWindow::initTable(const QString &type)
{
    QTableWidget *table;

    if(type == "PDOL")
        table = ui->pdTableWidget;
    else if(type == "gl1")
        table = ui->gl1TableWidget;
    else return;

    table->setColumnCount(6);
    table->setHorizontalHeaderLabels({"№","Длина, см", "Мера, см", "Ошибка, см", "Глубина, м", "Скорость, м/ч"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table->setColumnWidth(0, 40);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);

    QPalette palettePD = table->palette();
    palettePD.setColor(QPalette::AlternateBase, QColor(255, 200, 200, 80));
    table->setPalette(palettePD);

    for (int i = 1; i < 6; i++)
        table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);

    table->verticalHeader()->setVisible(false);
    table->setRowCount(100);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->setAlternatingRowColors(true);
    
    QHeaderView *header = table->horizontalHeader();
    connect(header, &QHeaderView::sectionResized, this, &MainWindow::tableSectionResized);
}

void MainWindow::cleanTableBoxes()
{
    ui->gl1LenBox->setText(QString::number(0.0));
    ui->gl1MeasBox->setText(QString::number(0.0));
    ui->gl1ErrBox->setText(QString::number(0.0));
    ui->gl1SpeedComboBox->lineEdit()->setText(QString::number(0.0));

    ui->pdLenBox->setText(QString::number(0.0));
    ui->pdMeasBox->setText(QString::number(0.0));
    ui->pdErrBox->setText(QString::number(0.0));
    ui->pdSpeedComboBox->lineEdit()->setText(QString::number(0.0));
}

void MainWindow::setStage()
{
    if (files.isEmpty()) return;


    bool prz = false, dvl = false;

    for(int i = 0; i < files.size(); i++)
    {
        QString name = QFileInfo(files[i]).fileName();

        if(name.contains("prz", Qt::CaseInsensitive))
            prz = true;

        if(name.contains("DV", Qt::CaseInsensitive))
            dvl = true;
    }

    if(prz && dvl || prz)
    {
        for(int i = 0; i < files.size(); i++)
        {
            QString name = QFileInfo(files[i]).fileName();

            if(name.contains("DV", Qt::CaseInsensitive))
            {
                files.remove(i);
                i--;
            }
        }

        ui->tabWidget->setCurrentIndex(1);
        ui->tab_2->setEnabled(true);
        ui->dvlGroupBox->setEnabled(false);

        return;
    }

    else if(dvl)
    {
        ui->tabWidget->setCurrentIndex(0);
        ui->dvlGroupBox->setEnabled(true);
    }
}

void MainWindow::showShiftLayout(bool state)
{
    auto lay = ui->shiftLayout;
    for (int i = 0; i < lay->count(); ++i) {
        if (auto w = lay->itemAt(i)->widget())
            w->setVisible(state);
    }
}

void MainWindow::showSettings()
{
    settingsDialog.exec();
}

void MainWindow::paintButton(QPushButton *btn, const QColor &c)
{
    const QString hex = c.name(QColor::HexArgb); // учитывает альфу
    btn->setStyleSheet(QStringLiteral(
                           "QPushButton{ background:%1; border:1px solid #666; border-radius:4px; }"
                           "QPushButton:hover{ border-color:#333; }"
                           "QPushButton:pressed{ border-color:#000; }"
                           ).arg(hex));
}

bool MainWindow::hasNumberInRow(QTableWidget *table, int row)
{
    QTableWidgetItem *item = table->item(row, 0);
    return isCellFull(item);
}

void MainWindow::projectTreeAppend(QString const &name, bool active, QString const &path)
{
    QStandardItem *item = new QStandardItem(name);

    if(!active)
        item->setForeground(QBrush(Qt::gray));

    if(name.contains("prz", Qt::CaseInsensitive))
        przItem->appendRow(item);

    else if(name.contains("DN", Qt::CaseInsensitive))
        ADNItem->appendRow(item);

    else if(name.contains("MK", Qt::CaseInsensitive) || name.contains("KM", Qt::CaseInsensitive))
        MKItem->appendRow(item);

    else if(name.contains("DV", Qt::CaseInsensitive))
        dvlItem->appendRow(item);
    else
    {
        delete item;
        return;
    }

    item->setToolTip(path);

    projectTreeUpdate();
}

void MainWindow::setLoaders(const QVector<const DataLoader*> &loaders) // слот сохранения загрузчиков из main_plot
{
    this->loaders = loaders;
    qDebug() << "added" << loaders.size() << "loaders into the mainwindow";
}

void MainWindow::updateProgress(const QString &fileId, int percent)
{
    progressDialog.updateProgress(fileId, percent);
}


void MainWindow::deleteFile(const QString &name) // удаление файла из открытых
{
    for (int i = 0; i < files.size(); i++)
    {
        if(files[i].contains(name))
        {
            files.remove(i);
            if (files.isEmpty())
            {
                ui->cleanPlotButton->setEnabled(false);
                ui->initPlotButton->setEnabled(false);
            }
            return;
        }
    }
}


void MainWindow::projectTreeRemove(const QModelIndex &index) // удаление файла из дерева
{
    if (!index.isValid())
        return;

    QStandardItem *item = model->itemFromIndex(index);
    if (!item)
        return;

    QStandardItem *parent = item->parent();
    if (!parent)
        return;

    qDebug() << "file" << index.data().toString() << "removed";

    parent->removeRow(item->row());
    projectTreeUpdate();
}


void MainWindow::onFileTreeClicked(const QPoint &pos) // обработчик кликов по дереву
{
    QModelIndex index = ui->fileTreeView->indexAt(pos);


    QMenu menu(this);

    if(index.isValid())
    {
        QStandardItem *item = model->itemFromIndex(index);
        if (item == przItem || item == ADNItem || item == MKItem || item == dvlItem)
            return;

        menu.addAction("Удалить");
        QString fileName = index.data().toString();
        QAction *selected = menu.exec(ui->fileTreeView->viewport()->mapToGlobal(pos));

        if(!selected)
            return;

        QString choice = selected->text();

        if(choice == "Удалить")
        {
            deleteFile(fileName);
            projectTreeRemove(index);
        }
    }
}


void MainWindow::on_applyPaletteButton_clicked()
{
    if(ui->firstPointTimeEdit->text().isEmpty()
        || ui->secondPointTimeEdit->text().isEmpty())
    return;

    ui->applyPaletteButton->setEnabled(false);
    ui->firstPointTimeEdit->setEnabled(false);
    ui->secondPointTimeEdit->setEnabled(false);
    ui->mkFactorSpinBox->setEnabled(false);

    // сигнал для вывода палетки
    // подключен к DCController::applyPalette
    emit showPalette();
}

bool MainWindow::eventFilter
    (QObject *watched, QEvent *event)
{
    if (watched == ui->firstPointTimeEdit)
    {
        if(event->type() == QEvent::FocusIn)
        {
            ui->main_plot->setFocus();
            ui->firstPointTimeEdit->setStyleSheet("background-color: #e6f2ff;");
            choosingFisrtPoint = true;

            ui->main_plot->plotChoosing(false);
        }

    }

    if (watched == ui->secondPointTimeEdit)
    {
        if(event->type() == QEvent::FocusIn)
        {
            ui->main_plot->setFocus();
            ui->secondPointTimeEdit->setStyleSheet("background-color: #e6f2ff;");
            choosingSecondPoint = true;

            ui->main_plot->plotChoosing(false);
        }
    }

    if(watched == ui->loadLineEdit)
    {
        if(event->type() == QEvent::FocusIn)
        {
            ui->main_plot->plotChoosing(false);
            ui->main_plot->verticalLine(false);
            ui->main_plot->horizontalLine(true);
            ui->main_plot->setFocus();
            ui->loadLineEdit->setStyleSheet("background-color: #e6f2ff;");
            choosingLoad = true;
        }
    }


    if(watched == ui->refTimeLineEdit)
    {
        if(event->type() == QEvent::FocusIn)
        {
            ui->main_plot->setFocus();
            ui->refTimeLineEdit->setStyleSheet("background-color: #e6f2ff;");
            choosingRefTime = true;
            ui->main_plot->plotChoosing(false);
        }
    }

    if(watched == ui->pdLogFirstLineEdit)
    {
        if(event->type() == QEvent::FocusIn)
        {
            ui->main_plot->setFocus();
            ui->pdLogFirstLineEdit->setStyleSheet("background-color: #e6f2ff;");
            choosingFirstPDLogPoint = true;

            ui->main_plot->plotChoosing(false);
        }
    }

    if(watched == ui->pdLogSecondLineEdit)
    {
        if(event->type() == QEvent::FocusIn)
        {
            ui->main_plot->setFocus();
            ui->pdLogSecondLineEdit->setStyleSheet("background-color: #e6f2ff;");
            choosingSecondPDLogPoint = true;

            ui->main_plot->plotChoosing(false);
        }
    }

    if (watched == ui->firstDvlLineEdit)
    {
        if(event->type() == QEvent::FocusIn)
        {
            ui->main_plot->setFocus();
            ui->firstDvlLineEdit->setStyleSheet("background-color: #e6f2ff;");
            choosingFirstDvlPoint = true;

            ui->main_plot->plotChoosing(false);
        }

    }

    if (watched == ui->secondDvlLineEdit)
    {
        if(event->type() == QEvent::FocusIn)
        {
            ui->main_plot->setFocus();
            ui->secondDvlLineEdit->setStyleSheet("background-color: #e6f2ff;");
            choosingSecondDvlPoint = true;

            ui->main_plot->plotChoosing(false);
        }

    }

    if (watched == ui->main_plot)
    {
        if(event->type() == QEvent::FocusOut)
        {
            if (choosingFisrtPoint)
            {
                ui->firstPointTimeEdit->clearFocus();
                ui->firstPointTimeEdit->setStyleSheet("background-color: none;");
                choosingFisrtPoint = false;

                ui->main_plot->plotChoosing(true);
            }

            if(choosingSecondPoint)
            {
                ui->secondPointTimeEdit->clearFocus();
                ui->secondPointTimeEdit->setStyleSheet("background-color: none;");
                choosingSecondPoint = false;

                ui->main_plot->plotChoosing(true);
            }

            if(choosingLoad)
            {
                ui->loadLineEdit->clearFocus();
                ui->loadLineEdit->setStyleSheet("background-color: none;");
                choosingLoad = false;

                ui->main_plot->plotChoosing(true);
                ui->main_plot->verticalLine(true);
            }

            if(choosingRefTime)
            {
                ui->refTimeLineEdit->clearFocus();
                ui->refTimeLineEdit->setStyleSheet("background-color: none;");
                choosingRefTime = false;

                ui->main_plot->plotChoosing(true);
            }

            if (choosingFirstPDLogPoint)
            {
                ui->pdLogFirstLineEdit->clearFocus();
                ui->pdLogFirstLineEdit->setStyleSheet("background-color: none;");
                choosingFirstPDLogPoint = false;

                ui->main_plot->plotChoosing(true);
            }

            if (choosingSecondPDLogPoint)
            {
                ui->pdLogSecondLineEdit->clearFocus();
                ui->pdLogSecondLineEdit->setStyleSheet("background-color: none;");
                choosingSecondPDLogPoint = false;

                ui->main_plot->plotChoosing(true);
            }

            if (choosingFirstPDEditPoint)
            {
                ui->firstPointPDEdit->clearFocus();
                ui->firstPointPDEdit->setStyleSheet("background-color: none;");
                choosingFirstPDEditPoint = false;

                ui->main_plot->plotChoosing(true);
            }

            if(choosingSecondPDEditPoint)
            {
                ui->secondPointPDEdit->clearFocus();
                ui->secondPointPDEdit->setStyleSheet("background-color: none;");
                choosingSecondPDEditPoint = false;

                ui->main_plot->plotChoosing(true);
            }

            if(choosingFirstDvlPoint)
            {
                ui->firstDvlLineEdit->clearFocus();
                ui->firstDvlLineEdit->setStyleSheet("background-color: none;");
                choosingFirstDvlPoint = false;

                ui->main_plot->plotChoosing(true);
            }

            if(choosingSecondDvlPoint)
            {
                ui->secondDvlLineEdit->clearFocus();
                ui->secondDvlLineEdit->setStyleSheet("background-color: none;");
                choosingSecondDvlPoint = false;

                ui->main_plot->plotChoosing(true);
            }
        }
    }

    if (watched == ui->firstPointPDEdit)
    {
        if(event->type() == QEvent::FocusIn)
        {
            ui->main_plot->setFocus();
            ui->firstPointPDEdit->setStyleSheet("background-color: #e6f2ff;");
            choosingFirstPDEditPoint = true;

            ui->main_plot->plotChoosing(false);
        }

    }

    if (watched == ui->secondPointPDEdit)
    {
        if(event->type() == QEvent::FocusIn)
        {
            ui->main_plot->setFocus();
            ui->secondPointPDEdit->setStyleSheet("background-color: #e6f2ff;");
            choosingSecondPDEditPoint = true;

            ui->main_plot->plotChoosing(false);
        }
    }

    return QMainWindow::eventFilter(watched, event);
}


void MainWindow::on_cleanPaletteButton_clicked()
{
    ui->firstPointTimeEdit->clear();
    ui->secondPointTimeEdit->clear();
    ui->firstPointTimeEdit->setEnabled(true);
    ui->secondPointTimeEdit->setEnabled(true);
    ui->rawPalettePlot->clean();
    ui->palettePlot->clean();
    ui->applyPaletteButton->setEnabled(true);
    ui->mkFactorSpinBox->setEnabled(true);
    ui->przInvertCheckBox->setEnabled(true);
    ui->cleanPaletteButton->setEnabled(true);
    ui->convertPrzButton->setEnabled(false);
}


void MainWindow::on_convertPrzButton_clicked()
{
    ui->tab_3->setEnabled(true);
    ui->convertPrzButton->setEnabled(false);

    // подключен к DCController::przConvert
    emit goPrzConvert();
}

void MainWindow::cleanAll()
{
    on_cleanPlotButton_clicked();
    ui->tabWidget->setCurrentIndex(0);
    ui->tab_1->setEnabled(true);
    ui->tab_2->setEnabled(false);
    ui->tab_3->setEnabled(false);
}

void MainWindow::createLoadingDialog(QDialog &loadingDialog)
{
    loadingDialog.setWindowModality(Qt::WindowModal);
    QVBoxLayout layout(&loadingDialog);
    QLabel label("Пожалуйста, подождите...");
    label.setAlignment(Qt::AlignCenter);
    layout.addWidget(&label);

    layout.setAlignment(Qt::AlignCenter);

    loadingDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    loadingDialog.setStyleSheet("background-color: lightblue;");
    loadingDialog.setFixedSize(250, 100);
}

// кнопка для создания графика ПД
void MainWindow::on_applyLoadButton_clicked()
{
    ui->applyLoadButton->setEnabled(false);
    ui->savePDButton->setEnabled(true);

    QString d = ui->refDepthLineEdit->text();

    QDateTime dt = QDateTime::fromString(ui->refTimeLineEdit->text(), "hh:mm:ss dd-MM-yyyy");
    QDateTime fs = QDateTime::fromString(ui->pdLogFirstLineEdit->text(), "hh:mm:ss dd-MM-yyyy");
    QDateTime sc = QDateTime::fromString(ui->pdLogSecondLineEdit->text(), "hh:mm:ss dd-MM-yyyy");

    if(!dt.isValid())
    {
        qWarning() << "Некорректный формат даты/времени";
        return;
    }

    double time = dt.toSecsSinceEpoch();
    double depth = d.toDouble();
    double firstLogPoint = fs.toSecsSinceEpoch();
    double secondLogPoint = sc.toSecsSinceEpoch();

    // подключено к DCController::PDcreate
    emit goPDcreate(time, depth, firstLogPoint, secondLogPoint);

    bool meas = this->hasMeasure("PDOL");
    enablePDCorrection(meas, meas);
}


// слот для проверки всех полей при создании положения долота
// подключен ко всем сигналам изменения полей
bool MainWindow::loadLinesChanged(const QString &text)
{
    if(!ui->refTimeLineEdit->text().isEmpty()
        && !ui->loadLineEdit->text().isEmpty()
        && !ui->refDepthLineEdit->text().isEmpty()
        && !ui->pdLogFirstLineEdit->text().isEmpty()
        && !ui->pdLogSecondLineEdit->text().isEmpty())
    {
        ui->applyLoadButton->setEnabled(true);
        ui->applyGl1PushButton->setEnabled(true);
        return true;
    }
    else
    {
        ui->applyLoadButton->setEnabled(false);
        ui->savePDButton->setEnabled(false);
        ui->applyGl1PushButton->setEnabled(false);
        return false;
    }
}

void MainWindow::gl1LinesChanged(const int &index)
{
    ui->applyGl1PushButton->setEnabled(true);
}

void MainWindow::loaderAdded(QString lName)
{
    QString name = getFName(lName);

    QAction *action = new QAction(name, this);

    action->setCheckable(true);
    action->setChecked(true);

    connect(action, &QAction::toggled, this, &MainWindow::graphViewChanged);

    ui->ViewMenu->addAction(action);
}


// слот для обновления меню отображения графиков
void MainWindow::loaderDeleted(QString lName)
{
    QList<QAction*> list = ui->ViewMenu->actions();

    QString label = getFName(lName);

    for(int i = 0; i < list.size(); i++)
    {
        if(list[i]->text() == label)
        {
            ui->ViewMenu->removeAction(list[i]);
            delete list[i];
            return;
        }
    }

    for (int i = 0; i < loaders.size(); i++)
    {
        QString name = loaders[i]->getName();
        if(name.contains(lName, Qt::CaseInsensitive))
            loaders.remove(i);
    }
}

void MainWindow::graphViewChanged(bool checked)
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action) return;

    QString text = action->text();

    ui->main_plot->hideGraph(text, checked);

    QList<QAction*> list = ui->menubar->actions();

    for(int i = 0; i < list.size(); i++)
    {
        if(list[i]->text() == text)
        {
            QFont font;
            font.setBold(checked);
            list[i]->setFont(font);
        }
    }
}


void MainWindow::setCalGraph(QVector<double> x, QVector<double> y)
{
    ui->rawPalettePlot->setCalGraph(x,y);
}

void MainWindow::calFinished()
{
    ui->convertPrzButton->setEnabled(true);
}

void MainWindow::przToPDFinished(const QVector<double> *lenghts)
{
    for (int i = 0; i < lenghts->size(); i++)
    {
        addIntervalRow("PDOL", i + 1, (*lenghts)[i]);
    }
}

void MainWindow::graphSelected(const bool &state, const QString &type)
{
    showShiftLayout(state);
    ui->shiftTimeSpinBox->setEnabled(state);
    ui->shiftLeftPushButton->setEnabled(state);
    ui->shiftRightPushButton->setEnabled(state);
    ui->cancelShiftPushButton->setEnabled(state);

    paintButton(ui->colorPushButton, ui->main_plot->getCurrentGraphColor());

    if(state && type.contains("DV", Qt::CaseInsensitive))
        ui->shiftTimeSpinBox->setMinimum(8);

    else if(state)
        ui->shiftTimeSpinBox->setMinimum(1000);
}

void MainWindow::removeLoader(const DataLoader *loader)
{
    int index = -1;

    for(int i = 0; i < loaders.size(); i++)
    {
        if(loaders[i] == loader)
        {
            index = i;
            break;
        }
    }

    if(index < 0) return;

    qDebug() << "loader" << loader->getName() << "has removed from MW";

    QList<QAction*> list = ui->ViewMenu->actions();

    QString label = getFName(loader->getName());

    for(int i = 0; i < list.size(); i++)
    {
        if(list[i]->text() == label)
        {
            ui->ViewMenu->removeAction(list[i]);
            delete list[i];
            break;
        }
    }

    loaders.remove(index);
}

void MainWindow::addLoader(const DataLoader *loader)
{
    if(loader == nullptr) return;

    loaders.append(loader);

    QString name = getFName(loader->getName());
    QAction *action = new QAction(name, this);

    action->setCheckable(true);
    action->setChecked(true);

    connect(action, &QAction::toggled, this, &MainWindow::graphViewChanged);

    ui->ViewMenu->addAction(action);
}

void MainWindow::setScrollRange(const double &start, const double &finish)
{
    ui->xAxisScrollBar->setRange(start, finish);
    ui->xAxisScrollBar->setValue(start);
}

void MainWindow::filesLoadFinished()
{
    QList<QAction*> acts = ui->ViewMenu->actions();

    bool flag = false;

    for(int i = 0; i < acts.size(); i++)
    {
        if(acts[i]->text().contains("_X"))
        {
            acts[i]->setChecked(false);
            flag = true;
        }
    }

    if(flag)
    {
        for(int i = 0; i < acts.size(); i++)
        {
            if(acts[i]->text() == "Датчик нагрузки"
                || acts[i]->text() == "Мерное колесо")
                acts[i]->setChecked(false);
        }
    }

    progressDialog.close();
    progressDialog.clear();
    this->setEnabled(true);
}

void MainWindow::initLogDebug(const QString &text)
{
    ui->initLogView->appendPlainText(text);
}

void MainWindow::przCreated()
{
    ui->main_plot->setAtStartPosition();

    QList<QAction*> acts = ui->ViewMenu->actions();

    for(int i = 0; i < acts.size(); i++)
        acts[i]->setChecked(true);

    loadDialog.close();

    ui->tabWidget->setCurrentIndex(1);
    ui->tab_2->setEnabled(true);
}

void MainWindow::manualLoadAdded()
{
    loadDialog.close();
}


void MainWindow::setUnixFormat()
{
    this->pdFormat = "UNIX";
    ui->savePDButton->setText("Сохранить (UNIX)");
}

void MainWindow::setDateFormat()
{
    this->pdFormat = "DATE";
    ui->savePDButton->setText("Сохранить (Дата/время)");
}


void MainWindow::on_savePDButton_clicked()
{
    QString openPath = lastPath();
    QString saveFileName = QFileDialog::getSaveFileName(this, "Сохранить файл", openPath, "Текстовые файлы (*.txt);");

    emit goSavePD(saveFileName, pdFormat);
}


void MainWindow::on_applyGl1PushButton_clicked()
{
    ui->applyGl1PushButton->setEnabled(false);
    ui->saveGl1PushButton->setEnabled(true);

    QString d = ui->refDepthLineEdit->text();

    QDateTime dt = QDateTime::fromString(ui->refTimeLineEdit->text(), "hh:mm:ss dd-MM-yyyy");
    QDateTime fs = QDateTime::fromString(ui->pdLogFirstLineEdit->text(), "hh:mm:ss dd-MM-yyyy");
    QDateTime sc = QDateTime::fromString(ui->pdLogSecondLineEdit->text(), "hh:mm:ss dd-MM-yyyy");

    if(!dt.isValid())
    {
        qWarning() << "Некорректный формат даты/времени";
        return;
    }

    double time = dt.toSecsSinceEpoch();
    double depth = d.toDouble();
    double firstLogPoint = fs.toSecsSinceEpoch();
    double secondLogPoint = sc.toSecsSinceEpoch();

    QString direction = ui->gl1DirComboBox->currentText();
    QString method = ui->gl1MethodComboBox->currentText();

    // подключен к DCController::gl1Create
    emit goGl1Create(time,
                     depth,
                     firstLogPoint,
                     secondLogPoint,
                     direction,
                     method);

    ui->main_plot->setFocus();

    if(this->hasMeasure("gl1"))
    {
        ui->candleCorrectionCheckBox->setEnabled(true);
        ui->candleCorrectionSpinBox->setEnabled(true);
        ui->candleCorrectionLabel->setEnabled(true);
    }

    ui->applyGL1CorPushButton->setEnabled(true);
    ui->lengthCorrectionCheckBox->setEnabled(true);
    ui->manualMeasLabel->setEnabled(true);
    ui->manualMeasLineEdit->setEnabled(true);
}


void MainWindow::on_pdRadioButton_clicked()
{
    ui->pdGl1SaveStackedWidget->setCurrentIndex(0);
    ui->tablesStackedWidget->setCurrentIndex(0);
}


void MainWindow::on_gl1RadioButton_clicked()
{
    ui->pdGl1SaveStackedWidget->setCurrentIndex(1);
    ui->tablesStackedWidget->setCurrentIndex(1);
}

// кнопка для удаления интервала движения (положение долота)
void MainWindow::on_delPDIntervalButton_clicked()
{
    QString first = ui->firstPointPDEdit->text();
    QString second = ui->secondPointPDEdit->text();

    if(first.isEmpty() || second.isEmpty()) return;

    ui->firstPointPDEdit->clear();
    ui->secondPointPDEdit->clear();

    // подключено к DCController::deleteInterval
    emit goDeleteInterval(first, second);
}

void MainWindow::on_cancelPDEditingButton_clicked()
{
    ui->firstPointPDEdit->clear();
    ui->secondPointPDEdit->clear();
}


void MainWindow::editLoadLonesChahged(const QString &text)
{
    // PD
    if(!ui->firstPointPDEdit->text().isEmpty()
        || !ui->secondPointPDEdit->text().isEmpty())
        ui->cancelPDEditingButton->setEnabled(true);
    else
        ui->cancelPDEditingButton->setEnabled(false);

    if(!ui->firstPointPDEdit->text().isEmpty()
            && !ui->secondPointPDEdit->text().isEmpty())
    {
        ui->delPDIntervalButton->setEnabled(true);
        ui->addPDIntervalButton->setEnabled(true);
    }
    else
    {
        ui->delPDIntervalButton->setEnabled(false);
        ui->addPDIntervalButton->setEnabled(false);
    }
}


void MainWindow::on_shiftLeftPushButton_clicked()
{
    double num = -ui->shiftTimeSpinBox->value();

    emit goShiftGraph(num);
    ui->main_plot->setFocus();
}


void MainWindow::on_shiftRightPushButton_clicked()
{
    double num = ui->shiftTimeSpinBox->value();

    emit goShiftGraph(num);
    ui->main_plot->setFocus();
}


void MainWindow::on_cancelShiftPushButton_clicked()
{
    // подключен к DCController::cancelShift
    emit goCancelShift();
    ui->main_plot->setFocus();
}


void MainWindow::on_addPDIntervalButton_clicked()
{
    QString first = ui->firstPointPDEdit->text();
    QString second = ui->secondPointPDEdit->text();

    QString firstLog = ui->pdLogFirstLineEdit->text();
    QString secondLog = ui->pdLogSecondLineEdit->text();

    QDateTime dtStart = QDateTime::fromString(first, "hh:mm:ss dd-MM-yyyy");
    QDateTime dtFinish = QDateTime::fromString(second, "hh:mm:ss dd-MM-yyyy");

    QDateTime dtStartLog = QDateTime::fromString(firstLog, "hh:mm:ss dd-MM-yyyy");
    QDateTime dtFinishLog = QDateTime::fromString(secondLog, "hh:mm:ss dd-MM-yyyy");

    if(first.isEmpty() || second.isEmpty()) return;

    if(dtStart < dtStartLog)
        ui->pdLogFirstLineEdit->setText(first);

    if(dtFinish > dtFinishLog)
        ui->pdLogSecondLineEdit->setText(second);

    ui->firstPointPDEdit->clear();
    ui->secondPointPDEdit->clear();

    // подключено к DCController::addInterval
    emit goAddInterval(first, second);
}


void MainWindow::on_openMeasureButton_clicked()
{
    qDebug() << "on_openMeasureButton_clicked";

    QString path = lastPath(); // путь к последней открытой папке
    QStringList selectedPaths = QFileDialog::getOpenFileNames(this, "Выберите файлы", path, "(*.DSV*)");

    if (!selectedPaths.isEmpty())
    {
        QFileInfo fileInfo(selectedPaths[0]);
        QString fileName = fileInfo.fileName(); // путь к папке с файлом
        ui->measureLabel->setText(fileName);
    }

    else return;

    ui->openMeasureButton->setEnabled(false);
    ui->cleanMeasureButton->setEnabled(true);

    emit goOpenMeasure(selectedPaths[0]);
}


void MainWindow::on_cleanMeasureButton_clicked()
{
    ui->measureLabel->setText("");
    ui->openMeasureButton->setEnabled(true);
    ui->candleCorrectionCheckBox->setEnabled(false);
    ui->candleCorrectionLabel->setEnabled(false);
    ui->candleCorrectionSpinBox->setEnabled(false);
    ui->manualMeasLineEdit->setText(0);

    clearMeasure(ui->pdTableWidget);
    clearMeasure(ui->gl1TableWidget);

    emit goCleanMeasure();
}


// слот об изменении ячеек таблицы
void MainWindow::tableCellChanged(int row, int column)
{
    QTableWidget *table = qobject_cast<QTableWidget *>(sender()); // получение указателя на таблицу-отправитель
    if(!table) return;

    QString type;

    if(table == ui->pdTableWidget)
        type = "PDOL";
    else if (table == ui->gl1TableWidget)
        type = "gl1";
    else return;

    QTableWidgetItem *len = table->item(row, 1);
    QTableWidgetItem *meas = table->item(row, 2);
    QTableWidgetItem *err = table->item(row, 3);

    if((column == 1 || column == 2) && (isCellFull(len) && isCellFull(meas)))
    {
        if(column == 2)
            countMeasureLen(type);


        if(meas)
        {
            meas->setTextAlignment(Qt::AlignCenter);
        }

        if (!err)
        {
            err = new QTableWidgetItem();
            table->setItem(row, 3, err);
            err->setTextAlignment(Qt::AlignCenter);
        }

        double lenValue, measValue;

        lenValue = len->text().toDouble();
        measValue = meas->text().toDouble();

        err->setText(QString::number(lenValue - measValue));
        err->setFlags(err->flags() & ~Qt::ItemIsEditable);
    }

    if(column == 1)
    {
        double totalLen = 0.0;
        QPushButton *box = nullptr;
        const DataLoader *loader = nullptr;

        if(type == "gl1")
            box = ui->gl1LenBox;
        else if(type == "PDOL")
            box = ui->pdLenBox;
        else return;

        for(int i = 0; i < loaders.size(); i++)
        {
            if(loaders[i]->getName().contains(type))
            {
                loader = loaders[i];
                break;
            }
        }

        if(loader != nullptr)
        {
            box->setText(QString::number(loader->totalLen() * 100));
            return;
        }

        else
        {
            for(int i = 0; i < table->rowCount(); i++)
            {
                QTableWidgetItem *item = table->item(i,column);

                if(isCellFull(item))
                {
                    totalLen += item->text().toDouble();
                }

                else break;
            }

            box->setText(QString::number(totalLen));
        }

        return;
    }

    if(column == 2)
    {
        double totalLen = 0.0;
        QPushButton *box = nullptr;
        if(type == "gl1")
            box = ui->gl1MeasBox;
        else if(type == "PDOL")
            box = ui->pdMeasBox;
        else return;

        for(int i = 0; i < table->rowCount(); i++)
        {
            QTableWidgetItem *item = table->item(i,column);

            if(isCellFull(item))
            {
                totalLen += item->text().toDouble();
            }

            else break;
        }

        box->setText(QString::number(totalLen));
        return;
    }

    if(column == 3)
    {
        double totalError = 0.0;

        QPushButton *box = nullptr;
        if(type == "gl1")
            box = ui->gl1ErrBox;
        else if(type == "PDOL")
            box = ui->pdErrBox;
        else return;

        for(int i = 0; i < table->rowCount(); i++)
        {
            QTableWidgetItem *item = table->item(i,column);

            if(isCellFull(item))
            {
                totalError += item->text().toDouble();
            }

            else break;
        }

        box->setText(QString::number(totalError));
        return;
    }

    if(column == 5)
    {
        QComboBox *box = nullptr;

        if(type == "gl1")
            box = ui->gl1SpeedComboBox;
        else if(type == "PDOL")
            box = ui->pdSpeedComboBox;
        else return;

        speedBoxChanged(box->currentIndex());
    }

    // удаление пустых строк в конце таблицы
    table->blockSignals(true);
    for (int i = table->rowCount() - 1; i >= 0; --i)
    {
        bool rowEmpty = true;
        for (int c = 0; c < table->columnCount(); ++c)
        {
            QTableWidgetItem *it = table->item(i, c);
            if (it && !it->text().trimmed().isEmpty())
            {
                rowEmpty = false;
                break;
            }
        }

        if (!rowEmpty)
            break; // встретили первую заполненную строку — больше не удаляем
        table->removeRow(i);
    }
    table->blockSignals(false);
}


void MainWindow::on_candleCorrectionCheckBox_toggled(bool checked)
{
    ui->candleCorrectionSpinBox->setEnabled(checked);
    ui->candleCorrectionLabel->setEnabled(checked);

    if(!checked && !ui->lengthCorrectionCheckBox->isChecked())
    {
        ui->applyGL1CorPushButton->setEnabled(false);
    }

    else
    {
        ui->applyGL1CorPushButton->setEnabled(true);
    }
}


void MainWindow::on_lengthCorrectionCheckBox_toggled(bool checked)
{
    ui->manualMeasLineEdit->setEnabled(checked);
    ui->manualMeasLabel->setEnabled(checked);

    if(!checked && !ui->candleCorrectionCheckBox->isChecked())
    {
        ui->applyGL1CorPushButton->setEnabled(false);
    }

    else
    {
        ui->applyGL1CorPushButton->setEnabled(true);
    }
}


void MainWindow::on_applyGL1CorPushButton_clicked()
{
    bool candle = ui->candleCorrectionCheckBox->isChecked();
    bool len = ui->lengthCorrectionCheckBox->isChecked();
    int totalLen;
    bool ok = false;
    double refDepth = ui->refDepthLineEdit->text().toDouble(&ok);
    if (!ok) 
        refDepth = 0.0;

    QDateTime dt = QDateTime::fromString(ui->refTimeLineEdit->text(), "hh:mm:ss dd-MM-yyyy");
    double refTime = 0.0;
    if (dt.isValid()) 
    refTime = dt.toSecsSinceEpoch();

    if(len)
        getRefTotalLen(totalLen);

    // подключен к DCController::gl1Correction
    emit goGl1Correction(candle, len, totalLen, refTime, refDepth);

    enableGl1Correction(!(candle || len), !(candle || len));

    ui->applyGl1PushButton->setEnabled(true);
    ui->cancelGl1CorPushButton->setEnabled(true);
}


void MainWindow::on_saveGl1PushButton_clicked()
{
    QString openPath = lastPath();
    QString saveFileName = QFileDialog::getSaveFileName(this, "Сохранить файл", openPath, "(*.gl1);");

    int startFrame = ui->startFrameSpinBox->value();

    // подключено к DCController::saveGl1File
    emit goSaveGl1(saveFileName, startFrame);
}


void MainWindow::on_pdCandleCorrectionCheckBox_toggled(bool checked)
{
    bool can = ui->pdCandleCorrectionCheckBox->isChecked();
    bool len = ui->pdLengthCorrectionCheckBox->isChecked();

    ui->applyPdCorPushButton->setEnabled(can || len);

    ui->pdCandleCorrectionSpinBox->setEnabled(checked);
    ui->pdCandleCorrectionLabel->setEnabled(checked);
}


void MainWindow::on_pdLengthCorrectionCheckBox_toggled(bool checked)
{
    bool can = ui->pdCandleCorrectionCheckBox->isChecked();
    bool len = ui->pdLengthCorrectionCheckBox->isChecked();

    ui->applyPdCorPushButton->setEnabled(can || len);

    ui->pdManualMeasLabel->setEnabled(checked);
    ui->pdManualMeasLineEdit->setEnabled(checked);
}


void MainWindow::on_applyPdCorPushButton_clicked()
{
    bool candle = ui->pdCandleCorrectionCheckBox->isChecked();
    bool len = ui->pdLengthCorrectionCheckBox->isChecked();
    int totalLen;
    bool ok = false;
    double refDepth = ui->refDepthLineEdit->text().toDouble(&ok);
    if (!ok) 
        refDepth = 0.0;

    QDateTime dt = QDateTime::fromString(ui->refTimeLineEdit->text(), "hh:mm:ss dd-MM-yyyy");
    double refTime = 0.0;
    if (dt.isValid()) 
    refTime = dt.toSecsSinceEpoch();

    if(len)
        getPdRefTotalLen(totalLen);

    enablePDCorrection(false, false);
    ui->applyLoadButton->setEnabled(true);
    ui->cancelPdCorPushButton->setEnabled(true);

    // подключен к DCController::pdCorrection
    emit goPdCorrection(candle, len, totalLen, refTime, refDepth);
}

void MainWindow::enablePDCorrection(bool candle, bool len)
{
    ui->pdCandleCorrectionCheckBox->setEnabled(candle);
    ui->pdCandleCorrectionSpinBox->setEnabled(candle);
    ui->pdCandleCorrectionLabel->setEnabled(candle);
    ui->applyPdCorPushButton->setEnabled(candle || len);
    ui->pdLengthCorrectionCheckBox->setEnabled(len);
    ui->pdManualMeasLabel->setEnabled(len);
    ui->pdManualMeasLineEdit->setEnabled(len);
}

void MainWindow::enableGl1Correction(bool candle, bool len)
{
    ui->candleCorrectionCheckBox->setEnabled(candle);
    ui->candleCorrectionSpinBox->setEnabled(candle);
    ui->candleCorrectionLabel->setEnabled(candle);
    ui->applyGL1CorPushButton->setEnabled(candle || len);
    ui->lengthCorrectionCheckBox->setEnabled(len);
    ui->manualMeasLabel->setEnabled(len);
    ui->manualMeasLineEdit->setEnabled(len);
}


void MainWindow::on_cancelGl1CorPushButton_clicked()
{
    on_applyGl1PushButton_clicked();
}


void MainWindow::on_cancelPdCorPushButton_clicked()
{
    on_applyLoadButton_clicked();
}

void MainWindow::tableSectionResized(int index, int, int newSize)
{
    if(index == 1)
    {
        newSize++;
        ui->gl1LenBox->setMaximumWidth(newSize);
        ui->gl1LenBox->setMinimumWidth(0);
        ui->pdLenBox->setMaximumWidth(newSize);
        ui->pdLenBox->setMinimumWidth(0);
        return;
    }

    else if(index == 2)
    {
        ui->gl1MeasBox->setMaximumWidth(newSize);
        ui->gl1MeasBox->setMinimumWidth(0);
        ui->pdMeasBox->setMaximumWidth(newSize);
        ui->pdMeasBox->setMinimumWidth(0);
        return;
    }

    else if(index == 3)
    {
        ui->gl1ErrBox->setMaximumWidth(newSize);
        ui->gl1ErrBox->setMinimumWidth(0);
        ui->pdErrBox->setMaximumWidth(newSize);
        ui->pdErrBox->setMinimumWidth(0);
        return;
    }

    else if(index == 4)
    {
        ui->gl1DepthBox->setMaximumWidth(newSize);
        ui->gl1DepthBox->setMinimumWidth(0);
        ui->pdDepthBox->setMaximumWidth(newSize);
        ui->pdDepthBox->setMinimumWidth(0);
        return;
    }

    else if(index == 5)
    {
        ui->gl1SpeedComboBox->setMaximumWidth(newSize);
        ui->gl1SpeedComboBox->setMinimumWidth(0);
        ui->pdSpeedComboBox->setMaximumWidth(newSize);
        ui->pdSpeedComboBox->setMinimumWidth(0);
        return;
    }

    else return;
}

void MainWindow::speedBoxChanged(int index)
{
    QComboBox *box = qobject_cast<QComboBox*>(sender());
    if(!box) return;

    QTableWidget *table;
    if(box == ui->gl1SpeedComboBox)
    {
        table = ui->gl1TableWidget;
    }

    else table = ui->pdTableWidget;

    double val, ref;

    if(index == 0)
    {
        val = 100000.0;
        ref = val;

        for(int i = 0; i < table->rowCount(); i++)
        {
            QTableWidgetItem *item = table->item(i,5);

            if(isCellFull(item))
            {
                val = qMin(val,item->text().toDouble());
            }

            else break;
        }

        box->setToolTip("Минимальная скорость");
    }

    else
    {
        val = -100000;
        ref = val;

        for(int i = 0; i < table->rowCount(); i++)
        {
            QTableWidgetItem *item = table->item(i,5);

            if(isCellFull(item))
            {
                val = qMax(val,item->text().toDouble());
            }

            else break;
        }

        box->setToolTip("Максимальная скорость");
    }

    if(val != ref)
        box->lineEdit()->setText(QString::number(val));
    else
        box->lineEdit()->setText("0.0");

    box->clearFocus();
    box->lineEdit()->clearFocus();
}


void MainWindow::on_applyDvlPushButton_clicked()
{
    loadDialog.setWindowModality(Qt::WindowModal);
    QVBoxLayout layout(&loadDialog);
    QLabel label("Пожалуйста, подождите...");
    label.setAlignment(Qt::AlignCenter);
    layout.addWidget(&label);

    layout.setAlignment(Qt::AlignCenter);

    loadDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    loadDialog.setStyleSheet("background-color: lightblue;");
    loadDialog.setFixedSize(250, 100);

    loadDialog.show();

    QCoreApplication::processEvents();

    emit goPrzCreate();
    ui->applyDvlPushButton->setEnabled(false);
}


void MainWindow::on_colorPushButton_clicked()
{
    QColor color = QColorDialog::getColor(Qt::white, this, "Выберите цвет");

    if (color.isValid())
    {
        paintButton(ui->colorPushButton, color);
        ui->main_plot->updateColor(color);
    }
}

void MainWindow::synchronizationChanged(bool state)
{
    progressDialog.synchronization(state);
}


void MainWindow::startLoadingProgress(const QString &op)
{
    loadProgressBar->setValue(0);
    loadProgressBar->setVisible(true);
}


void MainWindow::updateLoadingProgress(int percent)
{
    loadProgressBar->setValue(percent);
}


void MainWindow::finishLoadingProgress()
{
    loadProgressBar->setVisible(false);

    if(SnapshotManager::instance().canUndo())
        undoButton->setEnabled(true);
    else
        undoButton->setEnabled(false);

    if(SnapshotManager::instance().canRedo())
        redoButton->setEnabled(true);
    else
        redoButton->setEnabled(false);
}

void MainWindow::snapshotHistoryChanged()
{
    if(SnapshotManager::instance().canUndo())
        undoButton->setEnabled(true);
    else
        undoButton->setEnabled(false);

    if(SnapshotManager::instance().canRedo())
        redoButton->setEnabled(true);
    else
        redoButton->setEnabled(false);
}

void MainWindow::pdCreated()
{
    ui->applyGl1PushButton->setEnabled(true);
    ui->applyPdCorPushButton->setEnabled(true);
    ui->main_plot->setFocus();
}

void MainWindow::undoButtonClicked()
{
    emit goUndo();
}

void MainWindow::redoButtonClicked()
{
    emit goRedo();
}


void MainWindow::clear()
{
    ui->main_plot->init();
    cleanTableBoxes();

    ui->main_plot->resetActiveGraph();
    ui->cleanPlotButton->setEnabled(false);
    ui->initPlotButton->setEnabled(false);
    ui->xAxisScrollBar->setEnabled(false);
    ui->openProjectButton->setEnabled(true);
    ui->openFilesAction->setEnabled(true);
    ui->fileTreeView->blockSignals(false);
    ui->tab_2->setEnabled(false);
    ui->tab_3->setEnabled(false);
    ui->fileTreeView->setFocusPolicy(Qt::ClickFocus);
    ui->przInvertCheckBox->setChecked(false);
    ui->loadLineEdit->clear();
    ui->loadLineEdit->setEnabled(true);
    ui->applyLoadButton->setEnabled(false);
    ui->pdLogFirstLineEdit->clear();
    ui->pdLogSecondLineEdit->clear();
    ui->refDepthLineEdit->clear();
    ui->refTimeLineEdit->clear();
    ui->firstDvlLineEdit->clear();
    ui->secondDvlLineEdit->clear();

    ui->pdTableWidget->clear();
    initTable("PDOL");
    ui->gl1TableWidget->clear();
    initTable("gl1");

    ui->firstPointPDEdit->clear();
    ui->secondPointPDEdit->clear();
    ui->candleCorrectionCheckBox->setChecked(true);
    ui->candleCorrectionCheckBox->setEnabled(true);
    ui->lengthCorrectionCheckBox->setChecked(true);
    ui->lengthCorrectionCheckBox->setEnabled(true);
    ui->applyGl1PushButton->setEnabled(true);
    ui->candleCorrectionCheckBox->setEnabled(false);
    ui->candleCorrectionLabel->setEnabled(false);
    ui->candleCorrectionSpinBox->setEnabled(false);
    ui->manualMeasLineEdit->setEnabled(true);
    ui->manualMeasLabel->setEnabled(true);
    ui->initLogView->clear();
    ui->applyDvlPushButton->setEnabled(false);

    on_cleanMeasureButton_clicked();

    model->clear();
    setupProjectTree();
    on_cleanPaletteButton_clicked();

    files.clear();
    datFiles.clear();
    ui->main_plot->setFocus();
}


void MainWindow::updateStage()
{
    QVector<QString> files;

    for(int i = 0; i < loaders.size(); i++)
    {
        files.append(loaders[i]->getName());
    }

    //ui->main_plot->initPlot(loaders);
    setStage(files);
}
